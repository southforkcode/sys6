# Clock Phase Model for CPU6502 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Model `CPU6502`'s clock as an explicit 4-phase state machine (`Low`, `LowToHigh`, `High`, `HighToLow`), where `tick()` advances exactly one phase per call, sequential logic only commits on the two stable phases, and the `ALU` becomes always-on combinational logic that recomputes from its input latches on every single `tick()` call.

**Architecture:** `CPU6502` gains a `ClockPhase` enum and `m_clockPhase` member. `tick()` unconditionally recomputes `m_aluOutput` at the top of every call (the "always on" ALU), then advances `m_clockPhase` by one step, then dispatches to `onClockHigh()` (input capture: reads from the bus into latches) or `onClockLow()` (output commit: `PC++`, `m_cpuStep` advance, ALU output → registers/flags) depending on which stable phase was just reached. The two existing opcodes (`ADC #immediate`, `ADC absolute`) are re-expressed as capture/commit halves per T-state instead of one monolithic per-cycle action.

**Tech Stack:** C++17, CMake, GoogleTest.

## Global Constraints

- Public API is unchanged: `executeInstruction()`, all register/flag getters/setters, and `reset()` keep their current signatures and behavior. Only `tick()`'s *meaning* changes (one call = one clock phase, not one T-state).
- Binary-mode ADC only — `DFlag()` is not consulted (unchanged from the prior spec).
- No new opcodes, addressing modes, or ALU operations in this pass — only `ADC #immediate` (`0x69`) and `ADC absolute` (`0x6D`) exist today and both must keep working end-to-end.
- Follow existing style: 4-space indentation, `m_`-prefixed members, explicit repeated statements over loops in test files (matches existing `test/cpu/*.cpp` convention), no single-letter loop variables if a loop is used anywhere.
- Every new/modified source and test file must build cleanly via `cmake --build build --target sys6_tests` and pass via `./build/test/sys6_tests`.

---

## Task 1: `ClockPhase` scaffolding + generic opcode-fetch/unimplemented-opcode split

This task introduces the `ClockPhase` state machine and the always-on ALU
recompute, and proves the mechanism out on the two opcode-agnostic code
paths: the generic T0 opcode fetch (shared by every opcode) and the
unimplemented-opcode fallback. The two existing ADC opcode handlers
(`tickADCImmediate`, `tickADCAbsolute`) are **not** touched yet — they keep
running as monolithic per-cycle actions, just now invoked only from the
`Low` commit phase instead of from a bare `tick()` call. That's an
intentional interim state; Task 2 gives them the same capture/commit split.

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Modify: `test/cpu/cpu6502_test.cpp`
- Modify: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Produces: `enum class ClockPhase : uint8_t { Low, LowToHigh, High, HighToLow };` (declared in `cpu6502.h`, next to `CpuStep`); protected member `ClockPhase m_clockPhase = ClockPhase::Low;`; new private methods `void onClockHigh()`, `void onClockLow()`, `void captureOpcodeFetch()`, `void commitOpcodeFetch()`.
- Consumes: existing `m_bus`, `m_PC`, `m_IR`, `m_cpuStep`, `CpuStep`, `m_tracing`/`m_logger` (from `CPU`), `m_alu`, `m_aluA`/`m_aluB`/`m_aluCarryIn`/`m_aluOutput`, and the untouched `tickADCImmediate()`/`tickADCAbsolute()`.

- [ ] **Step 1: Update `test/cpu/cpu6502_test.cpp` to the new tick-per-phase semantics**

Replace the `TickPerformsOpcodeFetchOnFirstCall` test (it's no longer accurate — fetch now takes 4 calls, not 1) with:

```cpp
TEST_F(CPU6502Test, FourTicksCompleteOpcodeFetch) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502Test, TickDoesNotAdvancePCDuringIntermediateClockPhases) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.tick(); // Low -> LowToHigh: settling, no commit
    EXPECT_EQ(cpu.PC(), 0x0000);

    cpu.tick(); // LowToHigh -> High: capture opcode into m_IR, PC not yet touched
    EXPECT_EQ(cpu.PC(), 0x0000);

    cpu.tick(); // High -> HighToLow: settling, no commit
    EXPECT_EQ(cpu.PC(), 0x0000);

    cpu.tick(); // HighToLow -> Low: commit -- PC++, CpuStep advances to T1
    EXPECT_EQ(cpu.PC(), 0x0001);
}
```

Replace `TickCompletesUnimplementedOpcodeAsOneCycleNoOp` with:

```cpp
TEST_F(CPU6502Test, TickCompletesUnimplementedOpcodeAsOneCycleNoOp) {
    ram.write(0x0000, 0x42);
    ram.write(0x0001, 0x99);
    cpu.reset();

    cpu.tick(); // T0 phase 1/4: Low -> LowToHigh
    cpu.tick(); // T0 phase 2/4: LowToHigh -> High, capture opcode 0x42
    cpu.tick(); // T0 phase 3/4: High -> HighToLow
    cpu.tick(); // T0 phase 4/4: HighToLow -> Low, commit: PC -> 1, CpuStep -> T1

    cpu.tick(); // T1 phase 1/4
    cpu.tick(); // T1 phase 2/4: unimplemented opcode, no capture work
    cpu.tick(); // T1 phase 3/4
    cpu.tick(); // T1 phase 4/4: commit: CpuStep -> T0, instruction complete

    EXPECT_EQ(cpu.PC(), 0x0001);

    cpu.tick(); // next instruction, T0 phase 1/4
    cpu.tick(); // T0 phase 2/4: capture opcode 0x99
    cpu.tick(); // T0 phase 3/4
    cpu.tick(); // T0 phase 4/4: commit: PC -> 2

    EXPECT_EQ(cpu.PC(), 0x0002);
}
```

- [ ] **Step 2: Update `test/cpu/cpu6502_adc_test.cpp`'s direct-`tick()` tests to the new call-count arithmetic**

Replace `FirstTickAdvancesPCPastOpcodeOnly` with:

```cpp
TEST_F(CPU6502AdcTest, FourTicksAdvancePCPastOpcodeOnly) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}
```

Replace `AdcImmediateTakesExactlyTwoTicksToComplete` with:

```cpp
TEST_F(CPU6502AdcTest, EightTicksCompleteAdcImmediate) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0 complete: opcode fetched, PC -> 1

    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T1 complete: operand fetched, ALU applied, PC -> 2

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}
```

Replace `AdcAbsoluteLeavesAUnchangedUntilFinalCycle` with:

```cpp
TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcAbsoluteWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0: fetch opcode, PC -> 1
    EXPECT_EQ(cpu.PC(), 0x0001);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T1: fetch address low byte, PC -> 2
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T2: fetch address high byte, PC -> 3
    EXPECT_EQ(cpu.PC(), 0x0003);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T3: read operand from memory, apply ALU

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 3: Build and run to verify the updated tests fail**

Run: `cmake --build build --target sys6_tests -j && ./build/test/sys6_tests --gtest_filter='CPU6502Test.*:CPU6502AdcTest.*'`
Expected: FAIL — the new tests assert 4x-scaled tick-call semantics that the current (unmodified) `tick()` doesn't implement yet (e.g. `FourTicksCompleteOpcodeFetch` will see `PC` already far past `0x0001` after 4 raw `tick()` calls under the old one-call-per-T-state behavior).

- [ ] **Step 4: Add `ClockPhase` to `src/cpu/cpu6502.h`**

In `cpu6502.h`, add the enum right after `CpuStep`:

```cpp
enum class CpuStep : uint8_t { T0, T1, T2, T3 };
enum class ClockPhase : uint8_t { Low, LowToHigh, High, HighToLow };
```

Add the new member right after `CpuStep m_cpuStep = CpuStep::T0;`:

```cpp
CpuStep m_cpuStep = CpuStep::T0;
ClockPhase m_clockPhase = ClockPhase::Low;
```

Update the `private:` section to add the new methods alongside the existing ones (don't remove `tickAlu`/`applyAdc`/`tickADCImmediate`/`tickADCAbsolute` yet — Task 2 removes them):

```cpp
private:
    void onClockHigh();
    void onClockLow();
    void captureOpcodeFetch();
    void commitOpcodeFetch();
    void tickAlu();
    void applyAdc(uint8_t operand);
    void tickADCImmediate();
    void tickADCAbsolute();
```

- [ ] **Step 5: Rewrite `tick()` in `src/cpu/cpu6502.cpp` and add the new phase-dispatch methods**

Replace the entire `void CPU6502::tick() { ... }` function (currently lines 98-127) with:

```cpp
namespace {
ClockPhase nextClockPhase(ClockPhase phase) {
    switch (phase) {
    case ClockPhase::Low:
        return ClockPhase::LowToHigh;
    case ClockPhase::LowToHigh:
        return ClockPhase::High;
    case ClockPhase::High:
        return ClockPhase::HighToLow;
    case ClockPhase::HighToLow:
        return ClockPhase::Low;
    }
    return ClockPhase::Low; // unreachable: all enumerators handled above
}
} // namespace

void CPU6502::tick() {
    // The ALU is always-on combinational logic: it recomputes from whatever
    // is currently in its input latches on every tick, whether or not the
    // executing opcode is using the result this cycle.
    m_aluOutput = m_alu.adc(m_aluA, m_aluB, m_aluCarryIn);

    m_clockPhase = nextClockPhase(m_clockPhase);

    switch (m_clockPhase) {
    case ClockPhase::High:
        onClockHigh();
        break;
    case ClockPhase::Low:
        onClockLow();
        break;
    default:
        break; // LowToHigh / HighToLow: settling only, no commits.
    }
}

void CPU6502::onClockHigh() {
    if (m_cpuStep == CpuStep::T0) {
        captureOpcodeFetch();
        return;
    }

    // ADC opcodes don't split into capture/commit halves yet -- see Task 2
    // of the clock-phase-model plan. Their work still runs entirely from
    // onClockLow() below.
}

void CPU6502::onClockLow() {
    if (m_cpuStep == CpuStep::T0) {
        commitOpcodeFetch();
        return;
    }

    switch (m_IR) {
    case cOpADCImmediate:
        tickADCImmediate();
        break;
    case cOpADCAbsolute:
        tickADCAbsolute();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cpuStep = CpuStep::T0;
        break;
    }
}

void CPU6502::captureOpcodeFetch() { m_IR = m_bus.read(m_PC); }

void CPU6502::commitOpcodeFetch() {
    m_PC++;

    if (m_tracing && m_logger) {
        std::ostringstream oss;
        oss << "Fetched opcode 0x" << std::hex << std::uppercase << static_cast<int>(m_IR)
            << " at PC 0x" << (m_PC - 1);
        m_logger->trace(oss.str());
    }

    m_cpuStep = CpuStep::T1;
}
```

Leave `tickAlu()`, `applyAdc()`, `tickADCImmediate()`, and `tickADCAbsolute()` exactly as they are below this — they're unchanged in this task.

- [ ] **Step 6: Build and run to verify tests pass**

Run: `cmake --build build --target sys6_tests -j && ./build/test/sys6_tests`
Expected: PASS — all tests green, including the ones updated in Steps 1-2.

- [ ] **Step 7: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_test.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: model CPU6502 clock as a 4-phase state machine

tick() now advances one clock phase (Low/LowToHigh/High/HighToLow) per
call instead of one full T-state, and the ALU recomputes unconditionally
on every tick. ADC opcodes still commit their full per-cycle work on the
Low phase for now; splitting them into capture/commit halves is next."
```

---

## Task 2: Split `ADC #immediate` and `ADC absolute` into capture/commit halves

This task removes the interim "ADC opcodes run monolithically on Low" state
from Task 1 and gives both opcodes proper capture (bus reads / ALU input
loading, on `High`) and commit (`PC++`, `CpuStep` advance, ALU output →
registers/flags, on `Low`) halves, matching the tables in
`docs/superpowers/specs/2026-08-08-clock-phase-model-design.md`.

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Modify: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Produces: private methods `void loadAluInputs(uint8_t operand)`, `void commitAluResult()`, `void captureADCImmediate()`, `void commitADCImmediate()`, `void captureADCAbsolute()`, `void commitADCAbsolute()`. Removes `tickAlu()`, `applyAdc(uint8_t)`, `tickADCImmediate()`, `tickADCAbsolute()`.
- Consumes: `onClockHigh()`/`onClockLow()` from Task 1, `cOpADCImmediate`/`cOpADCAbsolute` constants, `m_addrLatch`, `m_A`/`A()`, `CFlag()`/`ZFlag()`/`VFlag()`/`NFlag()`, `m_aluOutput`.

- [ ] **Step 1: Add finer-grained inertness tests to `test/cpu/cpu6502_adc_test.cpp`**

Add these two new tests (they exercise behavior this task's production code must satisfy — they fail against Task 1's interim "ADC runs monolithically on Low" implementation, because under that implementation `A` is still unchanged through the intermediate ticks by coincidence, but only because the code isn't checking per-phase capture yet; run them now to lock in the target behavior before refactoring):

```cpp
TEST_F(CPU6502AdcTest, AdcImmediateDoesNotUpdateAUntilFinalPhaseOfLastCycle) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0 complete

    cpu.tick(); // T1 phase 1/4: Low -> LowToHigh
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T1 phase 2/4: capture operand + ALU inputs on arrival at High
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T1 phase 3/4: High -> HighToLow, ALU settles
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T1 phase 4/4: commit -- A and flags updated
    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteDoesNotUpdateAUntilFinalPhaseOfLastCycle) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T0 complete
    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T1 complete
    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T2 complete

    cpu.tick(); // T3 phase 1/4
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T3 phase 2/4: read operand from memory, load ALU inputs
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T3 phase 3/4: ALU settles
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // T3 phase 4/4: commit -- A and flags updated
    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Build and run to confirm current state (these should already pass — Task 1's monolithic-on-Low behavior happens to satisfy them too)**

Run: `cmake --build build --target sys6_tests -j && ./build/test/sys6_tests --gtest_filter='CPU6502AdcTest.*'`
Expected: PASS. This step just confirms the new tests are well-formed before the refactor; the real regression check is Step 5 after the production code changes, run alongside the rest of the ADC suite to make sure nothing else broke.

- [ ] **Step 3: Update `src/cpu/cpu6502.h`'s private method list**

Replace:

```cpp
private:
    void onClockHigh();
    void onClockLow();
    void captureOpcodeFetch();
    void commitOpcodeFetch();
    void tickAlu();
    void applyAdc(uint8_t operand);
    void tickADCImmediate();
    void tickADCAbsolute();
```

with:

```cpp
private:
    void onClockHigh();
    void onClockLow();
    void captureOpcodeFetch();
    void commitOpcodeFetch();
    void loadAluInputs(uint8_t operand);
    void commitAluResult();
    void captureADCImmediate();
    void commitADCImmediate();
    void captureADCAbsolute();
    void commitADCAbsolute();
```

- [ ] **Step 4: Update `src/cpu/cpu6502.cpp`**

Replace `onClockHigh()`'s body:

```cpp
void CPU6502::onClockHigh() {
    if (m_cpuStep == CpuStep::T0) {
        captureOpcodeFetch();
        return;
    }

    switch (m_IR) {
    case cOpADCImmediate:
        captureADCImmediate();
        break;
    case cOpADCAbsolute:
        captureADCAbsolute();
        break;
    default:
        break; // unimplemented opcode: nothing to capture
    }
}
```

Replace `onClockLow()`'s body:

```cpp
void CPU6502::onClockLow() {
    if (m_cpuStep == CpuStep::T0) {
        commitOpcodeFetch();
        return;
    }

    switch (m_IR) {
    case cOpADCImmediate:
        commitADCImmediate();
        break;
    case cOpADCAbsolute:
        commitADCAbsolute();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cpuStep = CpuStep::T0;
        break;
    }
}
```

Delete `tickAlu()`, `applyAdc(uint8_t operand)`, `tickADCImmediate()`, and `tickADCAbsolute()` entirely, replacing them with:

```cpp
void CPU6502::loadAluInputs(uint8_t operand) {
    m_aluA = m_A;
    m_aluB = operand;
    m_aluCarryIn = CFlag();
}

void CPU6502::commitAluResult() {
    A(m_aluOutput.value);
    CFlag(m_aluOutput.carry);
    ZFlag(m_aluOutput.zero);
    VFlag(m_aluOutput.overflow);
    NFlag(m_aluOutput.negative);
}

void CPU6502::captureADCImmediate() { loadAluInputs(m_bus.read(m_PC)); }

void CPU6502::commitADCImmediate() {
    m_PC++;
    commitAluResult();
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureADCAbsolute() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(m_PC)) << 8;
        break;
    case CpuStep::T3:
        loadAluInputs(m_bus.read(m_addrLatch));
        break;
    default:
        // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
        break;
    }
}

void CPU6502::commitADCAbsolute() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_PC++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
        break;
    }
}
```

- [ ] **Step 5: Build and run the full suite**

Run: `cmake --build build --target sys6_tests -j && ./build/test/sys6_tests`
Expected: PASS — all tests green, including every test from Task 1 and the two new inertness tests from Step 1.

- [ ] **Step 6: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "refactor: split ADC opcode handlers into capture/commit phases

ADC #immediate and ADC absolute now load their ALU inputs on the High
(capture) phase and write results back on the Low (commit) phase,
replacing the interim monolithic-on-Low handlers from the previous
commit. tickAlu()/applyAdc() are gone -- the ALU always recomputes in
tick() itself, and register writeback lives in commitAluResult()."
```

---

## Task 3: Direct test of the always-on ALU behavior

Tasks 1-2 make the ALU recompute unconditionally on every `tick()` call,
but every existing test only observes that indirectly, through an ADC
opcode's register writeback. This task adds a direct test proving the
recompute happens regardless of which opcode is executing, using a
minimal, non-invasive test seam: `m_aluA`, `m_aluB`, `m_aluCarryIn`, and
`m_aluOutput` are already `protected` on `CPU6502` (not `private`), so a
test-only subclass can expose them with a `using` declaration — no
production code changes needed.

**Files:**
- Create: `test/cpu/cpu6502_clock_test.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `CPU6502`'s protected members `m_aluA`, `m_aluB`, `m_aluCarryIn`, `m_aluOutput` (declared in `src/cpu/cpu6502.h`), `AluResult` (declared in `src/cpu/alu.h`), and the public `CPU6502(Bus&)` constructor, `reset()`, `tick()`.

- [ ] **Step 1: Write `test/cpu/cpu6502_clock_test.cpp`**

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502ClockTestAccess : public CPU6502 {
public:
    using CPU6502::CPU6502;
    using CPU6502::m_aluA;
    using CPU6502::m_aluB;
    using CPU6502::m_aluCarryIn;
    using CPU6502::m_aluOutput;
};

class CPU6502ClockTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502ClockTestAccess cpu{bus};

    CPU6502ClockTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502ClockTest, AluRecomputesOnTickRegardlessOfOpcode) {
    ram.write(0x0000, 0x42); // unimplemented opcode: no opcode-specific ALU use
    cpu.reset();
    cpu.m_aluA = 0x01;
    cpu.m_aluB = 0x01;
    cpu.m_aluCarryIn = false;

    cpu.tick();

    EXPECT_EQ(cpu.m_aluOutput.value, 0x02);
}

TEST_F(CPU6502ClockTest, AluOutputTracksLatestInputsAcrossTicks) {
    ram.write(0x0000, 0x42);
    cpu.reset();
    cpu.m_aluA = 0x01;
    cpu.m_aluB = 0x01;
    cpu.m_aluCarryIn = false;

    cpu.tick();
    EXPECT_EQ(cpu.m_aluOutput.value, 0x02);

    cpu.m_aluB = 0x05;
    cpu.tick();

    EXPECT_EQ(cpu.m_aluOutput.value, 0x06);
}
```

- [ ] **Step 2: Register the new test file in `test/CMakeLists.txt`**

Add `cpu/cpu6502_clock_test.cpp` to the `add_executable(sys6_tests ...)` sources list, alongside the other `cpu/*.cpp` test files:

```cmake
add_executable(sys6_tests
    cpu/alu_test.cpp
    cpu/cpu6502_adc_test.cpp
    cpu/cpu6502_clock_test.cpp
    cpu/cpu6502_test.cpp
    memory/ram_test.cpp
    memory/rom_test.cpp
    memory/bus_test.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/alu.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/cpu6502.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/ram.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/rom.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/bus.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/log.cpp
)
```

- [ ] **Step 3: Re-run CMake configure (new source file added) and build**

Run: `cmake -S . -B build && cmake --build build --target sys6_tests -j`
Expected: builds successfully with the new test file compiled in.

- [ ] **Step 4: Run the new tests to verify they pass**

Run: `./build/test/sys6_tests --gtest_filter='CPU6502ClockTest.*'`
Expected: PASS for both `AluRecomputesOnTickRegardlessOfOpcode` and `AluOutputTracksLatestInputsAcrossTicks`.

- [ ] **Step 5: Run the full suite to confirm no regressions**

Run: `./build/test/sys6_tests`
Expected: PASS — every test in the project, old and new.

- [ ] **Step 6: Commit**

```bash
git add test/cpu/cpu6502_clock_test.cpp test/CMakeLists.txt
git commit -m "test: prove the ALU recomputes on every tick regardless of opcode"
```
