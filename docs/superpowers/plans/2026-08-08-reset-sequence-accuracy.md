# Reset Sequence Accuracy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `CPU6502::reset()`'s synchronous register-slam with a cycle-accurate 7-T-state reset sequence: `PC` loaded from `cResetVector`, `SP` decremented by 3 from whatever it already held, and `A`/`X`/`Y`/`D`/`B`/bit 5 left untouched (only `I` is unconditionally forced), matching real 6502 hardware.

**Architecture:** A new private `m_resetActive` flag routes `onClockHigh()`/`onClockLow()` to new `captureReset()`/`commitReset()` handlers — checked ahead of the existing `m_cpuStep == T0` opcode-fetch branch — following the exact same capture/commit-pair convention every other operation in `CPU6502` already uses. A new public `beginReset()` arms the sequence without pumping it; `reset()` becomes a thin wrapper calling `beginReset()` then looping `tick()` to completion, mirroring `executeInstruction()`'s existing pump-to-completion pattern. This keeps every existing `cpu.reset()` call site working unchanged while making the sequence genuinely `tick()`-observable for a driver that wants manual control.

**Tech Stack:** C++17, GoogleTest/GTest (via CMake FetchContent), CMake/CTest. No new external dependencies, no new files, no `CMakeLists.txt` changes (all edits land in files already registered in the build).

## Global Constraints

- Follow the existing code style: 4-space indent, `m_` member prefix, `c`-prefixed constants, capture/commit function pairs dispatched via `switch (m_cpuStep)`/`switch (m_IR)`, comments only where they explain a non-obvious *why* (see existing `// idle: ...` and `// Unreachable: ...` comment conventions in `cpu6502.cpp`).
- Design spec: `docs/superpowers/specs/2026-08-08-reset-sequence-accuracy-design.md` — read it before starting; this plan implements it in full.
- Test fixture: `test/cpu/cpu6502_test.cpp`'s existing `CPU6502Test` fixture (`RAM ram{0x10000}; Bus bus; CPU6502 cpu{bus};` with `bus.attach(0x0000, 0xFFFF, ram);` in the constructor) — reuse it; do not create a new fixture or test file.
- After the task's tests pass, also run the *full* suite (`ctest --test-dir build --output-on-failure`) before committing — `reset()`'s behavior change touches every single test in the suite indirectly (nearly all of them call `cpu.reset()` first), so this is the one change where a full-suite regression pass matters most.

---

## Task 1: Cycle-accurate reset sequence

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Modify: `test/cpu/cpu6502_test.cpp` (rewrite one existing test, add four new ones)

**Interfaces:**
- Consumes: existing `CPU6502` members `m_bus`, `m_PC`, `m_SP`, `m_addrLatch`, `m_cpuStep`, `m_clockPhase`, `m_halted`, `cResetVector`; existing `IFlag(bool)`, `tick()`.
- Produces: new public `void CPU6502::beginReset()`. `CPU6502::reset()` keeps its existing signature/semantics from the caller's point of view (synchronous, fully-settled on return) but is now implemented via `beginReset()` + a `tick()` pump loop. No other public API changes.

- [ ] **Step 1: Write the failing tests**

In `test/cpu/cpu6502_test.cpp`, replace the existing `ResetSetsRegistersAndFlagsToPowerOnState` test:

```cpp
TEST_F(CPU6502Test, ResetSetsRegistersAndFlagsToPowerOnState) {
    cpu.reset();

    EXPECT_EQ(cpu.A(), 0);
    EXPECT_EQ(cpu.X(), 0);
    EXPECT_EQ(cpu.Y(), 0);
    EXPECT_EQ(cpu.PC(), 0);
    EXPECT_EQ(cpu.SP(), 0xFF);
    EXPECT_TRUE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_TRUE(cpu.BFlag());
}
```

with:

```cpp
TEST_F(CPU6502Test, ColdResetLoadsPCFromVectorAndLandsSPOnFD) {
    ram.write(0xFFFC, 0x34);
    ram.write(0xFFFD, 0x12);

    cpu.reset();

    // A freshly constructed CPU (not yet reset) default-initializes A/X/Y/D
    // to 0/0/0/false, so these read as "power-on defaults", not because
    // reset() force-clears them -- see WarmResetPreservesAXYAndDFlag below,
    // which proves reset() genuinely leaves them alone.
    EXPECT_EQ(cpu.A(), 0);
    EXPECT_EQ(cpu.X(), 0);
    EXPECT_EQ(cpu.Y(), 0);
    EXPECT_EQ(cpu.PC(), 0x1234);
    EXPECT_EQ(cpu.SP(), 0xFD); // 0x00 - 3, wrapping
    EXPECT_TRUE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_FALSE(cpu.halted());
}

TEST_F(CPU6502Test, WarmResetPreservesAXYAndDFlagAndDecrementsSPByThree) {
    ram.write(0xFFFC, 0x00);
    ram.write(0xFFFD, 0x90);
    cpu.reset();
    cpu.A(0x11);
    cpu.X(0x22);
    cpu.Y(0x33);
    cpu.DFlag(true);
    cpu.SP(0x80);

    cpu.reset();

    EXPECT_EQ(cpu.A(), 0x11);
    EXPECT_EQ(cpu.X(), 0x22);
    EXPECT_EQ(cpu.Y(), 0x33);
    EXPECT_TRUE(cpu.DFlag());
    EXPECT_EQ(cpu.SP(), 0x7D); // 0x80 - 3
}

TEST_F(CPU6502Test, ResetDecrementsSPWithPageWraparound) {
    ram.write(0xFFFC, 0x00);
    ram.write(0xFFFD, 0x90);
    cpu.reset();
    cpu.SP(0x01);

    cpu.reset();

    EXPECT_EQ(cpu.SP(), 0xFE); // 0x01 - 3, wraps within page 1
}

TEST_F(CPU6502Test, ResetDoesNotForceBFlagOrBit5) {
    ram.write(0xFFFC, 0x00);
    ram.write(0xFFFD, 0x90);
    cpu.reset();
    cpu.P(0x00); // clear everything, including B and bit 5

    cpu.reset();

    EXPECT_FALSE(cpu.BFlag());
    EXPECT_EQ(cpu.P() & 0x20, 0);
}

TEST_F(CPU6502Test, SevenTStatesCompleteResetWhenDrivenManually) {
    ram.write(0xFFFC, 0x34);
    ram.write(0xFFFD, 0x12);
    cpu.PC(0x5555);
    cpu.SP(0x80);

    cpu.beginReset();
    for (int step = 0; step < 7; ++step) {
        for (int i = 0; i < 4; ++i) {
            cpu.tick();
        }
    }

    EXPECT_EQ(cpu.PC(), 0x1234);
    EXPECT_EQ(cpu.SP(), 0x7D); // 0x80 - 3
    EXPECT_FALSE(cpu.halted());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake -S . -B build && cmake --build build`
Expected: build fails — `cpu.beginReset()` doesn't exist yet. Once stubbed in Step 3 (before the real implementation), the value-assertion tests fail because `reset()` still hardcodes `PC(0)`/`SP(0xff)`/etc.

- [ ] **Step 3: Implement the cycle-accurate reset sequence**

In `src/cpu/cpu6502.h`, give `m_A`/`m_X`/`m_Y`/`m_PC`/`m_SP` default member initializers (currently they have none, unlike every other data member in the class) — change:

```cpp
protected:
    Bus &m_bus;
    uint8_t m_A;   // accumulator register
    uint8_t m_X;   // index register X
    uint8_t m_Y;   // index register Y
    uint16_t m_PC; // program counter
    uint8_t m_SP;  // stack pointer
    std::bitset<8> m_pFlags;
```

to:

```cpp
protected:
    Bus &m_bus;
    uint8_t m_A = 0;   // accumulator register
    uint8_t m_X = 0;   // index register X
    uint8_t m_Y = 0;   // index register Y
    uint16_t m_PC = 0; // program counter
    uint8_t m_SP = 0;  // stack pointer
    std::bitset<8> m_pFlags;
```

Add `m_resetActive` next to `m_halted`:

```cpp
    bool m_branchTaken = false;
    bool m_halted = false;
    bool m_resetActive = false;
```

Add the new public `beginReset()` right after the existing `void reset() override;` declaration:

```cpp
    void reset() override;

    // Arms the reset sequence (m_resetActive = true, m_cpuStep = T0)
    // without pumping it to completion -- for a driver that wants to
    // observe/trace the 7 T-state reset sequence one tick() at a time.
    // reset() is the convenience wrapper most callers should use instead:
    // it calls this and then pumps via tick() until the sequence
    // completes, so the CPU is fully settled by the time reset() returns.
    void beginReset();

    void executeInstruction() override;
```

Add the new private handler declarations right after `commitOpcodeFetch();`:

```cpp
    void captureOpcodeFetch();
    void commitOpcodeFetch();

    // Reset sequence: models real 6502 RESET as 7 T-states (T0-T6) -- 2
    // dummy reads, 3 phantom stack decrements (no writes; real hardware
    // suppresses them), then a 2-cycle read of cResetVector. m_resetActive
    // routes onClockHigh()/onClockLow() here ahead of the normal
    // T0-opcode-fetch/m_IR dispatch.
    void captureReset();
    void commitReset();
```

In `src/cpu/cpu6502.cpp`, replace the existing `reset()`:

```cpp
void CPU6502::reset() {
    m_halted = false;
    A(0);
    X(0);
    Y(0);
    PC(0);
    SP(0xff);
    IFlag(true);
    DFlag(false);
    BFlag(true);
    m_pFlags.set(5, true);
}
```

with:

```cpp
void CPU6502::beginReset() {
    m_resetActive = true;
    m_cpuStep = CpuStep::T0;
}

void CPU6502::reset() {
    beginReset();
    do {
        tick();
    } while (m_cpuStep != CpuStep::T0 || m_clockPhase != ClockPhase::Low);
}
```

In `onClockHigh()`, route to `captureReset()` ahead of the opcode-fetch check:

```cpp
void CPU6502::onClockHigh() {
    if (m_resetActive) {
        captureReset();
        return;
    }

    if (m_cpuStep == CpuStep::T0) {
        captureOpcodeFetch();
        return;
    }
```

In `onClockLow()`, make the matching change:

```cpp
void CPU6502::onClockLow() {
    if (m_resetActive) {
        commitReset();
        return;
    }

    if (m_cpuStep == CpuStep::T0) {
        commitOpcodeFetch();
        return;
    }
```

At the end of the file (after the final `commitImpliedPull()` closing brace), append the new handler bodies:

```cpp
void CPU6502::captureReset() {
    switch (m_cpuStep) {
    case CpuStep::T0:
    case CpuStep::T1:
        static_cast<void>(m_bus.read(m_PC)); // dummy read: real hardware fetches and discards
        break;
    case CpuStep::T2:
    case CpuStep::T3:
    case CpuStep::T4:
        break; // idle: the phantom stack decrements happen in commit, where m_SP is authoritative
    case CpuStep::T5:
        m_addrLatch = m_bus.read(cResetVector);
        break;
    case CpuStep::T6:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(static_cast<uint16_t>(cResetVector + 1)))
                       << 8;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T0-T6.
    }
}

void CPU6502::commitReset() {
    switch (m_cpuStep) {
    case CpuStep::T0:
        m_halted = false;
        m_cpuStep = CpuStep::T1;
        break;
    case CpuStep::T1:
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_SP--; // phantom push #1: decrement only, real hardware suppresses the write
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_SP--; // phantom push #2
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_SP--; // phantom push #3
        IFlag(true);
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        m_cpuStep = CpuStep::T6;
        break;
    case CpuStep::T6:
        m_PC = m_addrLatch;
        m_resetActive = false;
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T0-T6.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502Test`
Expected: PASS (all `CPU6502Test.*` tests green, including the 5 reset-related tests).

Then run the full suite: `ctest --test-dir build --output-on-failure`
Expected: PASS — every other test file calls `cpu.reset()` at least once; this confirms none of them depended on the old hardcoded `PC(0)`/`SP(0xff)`/`A(0)`/`X(0)`/`Y(0)`/`DFlag(false)`/`BFlag(true)` values in a way that breaks under the new semantics (they don't: every other test either writes its own reset-vector-independent starting `PC` explicitly after `reset()`, or relies only on `IFlag()`/`halted()` being correct, both unchanged).

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_test.cpp
git commit -m "$(cat <<'EOF'
feat: make reset() cycle-accurate and match real 6502 semantics

PC now loads from cResetVector instead of hardcoding 0; SP decrements by
3 from its current value instead of being force-set to 0xff; A/X/Y/D/B/
bit5 are left untouched (only I is forced), matching real NMOS 6502
RESET behavior instead of the old synchronous zero-everything reset().

reset() is now a thin pump-to-completion wrapper around a new 7-T-state
capture/commit sequence (captureReset()/commitReset()), following the
same convention every other CPU operation already uses. A new
beginReset() arms the sequence without pumping it, for a driver that
wants to trace/single-step it via tick() the way BRK's cycle tests
already do.
EOF
)"
```

---

## Final Verification

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: full green suite, including the 5 new/rewritten `CPU6502Test.*` reset tests, with zero regressions across every other test file.
