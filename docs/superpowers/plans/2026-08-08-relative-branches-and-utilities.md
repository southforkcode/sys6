# Relative Branches, BRK Halt, and Program Loader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add relative conditional branches, a BRK-driven halt mechanism, a `run()` driver helper, a hex-string program loader, and the minimal LDA/STA opcodes needed to prove it all end-to-end with a hand-assembled Fibonacci program.

**Architecture:** Every new CPU behavior is added to the existing `CPU6502` capture/commit state-machine dispatch in `src/cpu/cpu6502.{h,cpp}` (switch on `m_IR` inside `onClockHigh()`/`onClockLow()`), following the file's established one-family-per-capture/commit-pair convention. The program loader is a standalone free function operating on `MemoryDevice`, independent of `CPU6502`/`Bus`.

**Tech Stack:** C++17, GoogleTest/GTest (via CMake FetchContent), CMake/CTest. No new external dependencies.

## Global Constraints

- Source/test files are listed explicitly in `CMakeLists.txt` / `test/CMakeLists.txt` (no globbing for the build targets themselves) — every new file must be added to the relevant list(s) or it will not compile/link/run.
- Follow the existing code style: 4-space indent, `m_` member prefix, `c`-prefixed constants, capture/commit function pairs dispatched via `switch (m_IR)`, comments only where they explain a non-obvious *why* (see existing `// idle: ...` and `// Unreachable: ...` comment conventions in `cpu6502.cpp`).
- Design spec: `docs/superpowers/specs/2026-08-08-relative-branches-and-utilities-design.md` — read it before starting; every task below implements a section of it.
- Test fixtures follow the pattern in `test/cpu/cpu6502_adc_test.cpp`: a `RAM ram{0x10000}; Bus bus; CPU6502 cpu{bus};` fixture with `bus.attach(0x0000, 0xFFFF, ram);` in the constructor.
- After every task's tests pass, also run the *full* suite (`ctest --test-dir build --output-on-failure`) before committing, to catch regressions in unrelated opcode families sharing the same switch statements.

---

## Task 1: Minimal load/store opcodes (LDA immediate/zero page, STA zero page/absolute,Y)

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_load_store_test.cpp` (new)
- Modify: `test/CMakeLists.txt` (register the new test file)

**Interfaces:**
- Consumes: existing `CPU6502` members `m_bus`, `m_PC`, `m_addrLatch`, `m_effAddr`, `m_A`, `m_Y`, `m_aluA`, `m_aluB`, `m_aluFunction`, `m_aluOutput`, `A()`/`A(uint8_t)`, `ZFlag()`/`ZFlag(bool)`, `NFlag()`/`NFlag(bool)`; the `indexedAddress(uint16_t base, uint8_t index)` static helper; free functions `aluZero(uint8_t)`/`aluNegative(uint8_t)` (anonymous namespace in `cpu6502.cpp`).
- Produces: opcodes `0xA9` (LDA immediate), `0xA5` (LDA zero page), `0x85` (STA zero page), `0x99` (STA absolute,Y) become executable. No new public API.

- [ ] **Step 1: Write the failing tests**

Create `test/cpu/cpu6502_load_store_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502LoadStoreTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502LoadStoreTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502LoadStoreTest, LdaImmediateLoadsValueIntoA) {
    ram.write(0x0000, 0xA9); // LDA #imm
    ram.write(0x0001, 0x42);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, LdaImmediateSetsZeroFlagOnZero) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x00);
    cpu.reset();
    cpu.A(0xFF);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.ZFlag());
    EXPECT_FALSE(cpu.NFlag());
}

TEST_F(CPU6502LoadStoreTest, LdaImmediateSetsNegativeFlagOnHighBitSet) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x80);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.ZFlag());
}

TEST_F(CPU6502LoadStoreTest, LdaImmediateDoesNotAffectCarryFlag) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x00);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.CFlag());
}

TEST_F(CPU6502LoadStoreTest, TwoTicksCompleteLdaImmediate) {
    ram.write(0x0000, 0xA9);
    ram.write(0x0001, 0x42);
    cpu.reset();

    for (int i = 0; i < 4; ++i) cpu.tick(); // T0
    for (int i = 0; i < 4; ++i) cpu.tick(); // T1

    EXPECT_EQ(cpu.A(), 0x42);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, LdaZeroPageLoadsValueFromMemory) {
    ram.write(0x0000, 0xA5); // LDA zp
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x77);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x77);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, ThreeTicksCompleteLdaZeroPage) {
    ram.write(0x0000, 0xA5);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x77);
    cpu.reset();

    for (int i = 0; i < 4; ++i) cpu.tick(); // T0
    for (int i = 0; i < 4; ++i) cpu.tick(); // T1
    EXPECT_EQ(cpu.A(), 0x00); // not yet committed

    for (int i = 0; i < 4; ++i) cpu.tick(); // T2

    EXPECT_EQ(cpu.A(), 0x77);
}

TEST_F(CPU6502LoadStoreTest, StaZeroPageWritesAccumulatorToMemory) {
    ram.write(0x0000, 0x85); // STA zp
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.A(0x99);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0020), 0x99);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502LoadStoreTest, StaZeroPageDoesNotAffectFlags) {
    ram.write(0x0000, 0x85);
    ram.write(0x0001, 0x20);
    cpu.reset();
    cpu.A(0x00); // would set Z if STA touched flags like an ALU op
    cpu.ZFlag(false);
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502LoadStoreTest, StaAbsoluteYWritesAccumulatorToIndexedAddress) {
    ram.write(0x0000, 0x99); // STA abs,Y
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base = 0x0200
    cpu.reset();
    cpu.A(0x55);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(ram.read(0x0205), 0x55);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LoadStoreTest, FiveTicksCompleteStaAbsoluteYRegardlessOfPageCross) {
    ram.write(0x0000, 0x99); // STA abs,Y
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x00); // base = 0x00FF; +Y crosses into page 1
    cpu.reset();
    cpu.A(0x33);
    cpu.Y(0x01); // 0x00FF + 1 = 0x0100: page crossed

    for (int step = 0; step < 5; ++step) {
        for (int i = 0; i < 4; ++i) cpu.tick();
    }

    EXPECT_EQ(ram.read(0x0100), 0x33);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502LoadStoreTest, StaAbsoluteYWithNoPageCrossStillTakesFiveTicks) {
    ram.write(0x0000, 0x99);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base = 0x0200, no page cross
    cpu.reset();
    cpu.A(0x11);
    cpu.Y(0x01);

    for (int step = 0; step < 4; ++step) {
        for (int i = 0; i < 4; ++i) cpu.tick();
    }
    EXPECT_EQ(ram.read(0x0201), 0x00); // not yet written after 4 cycles

    for (int i = 0; i < 4; ++i) cpu.tick(); // 5th cycle

    EXPECT_EQ(ram.read(0x0201), 0x11);
}
```

Add the new test file to `test/CMakeLists.txt`'s `add_executable(sys6_tests ...)` list, alongside the other `cpu/*_test.cpp` entries (e.g. right after `cpu/cpu6502_inc_dec_test.cpp`):

```cmake
    cpu/cpu6502_inc_dec_test.cpp
    cpu/cpu6502_load_store_test.cpp
    cpu/cpu6502_addressing_test.cpp
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502LoadStoreTest`
Expected: build fails (or all tests fail) because opcodes `0xA9`/`0xA5`/`0x85`/`0x99` aren't dispatched yet, so `A()` stays 0 and memory is never written.

- [ ] **Step 3: Implement LDA/STA**

In `src/cpu/cpu6502.h`, add the four new private method declarations at the end of the private section, immediately before the closing `};` of the class (after the existing `commitImpliedIncDec();` declaration):

```cpp
    void captureImpliedIncDec();
    void commitImpliedIncDec();

    // Load/store family (LDA/STA): LDA reuses the ALU's combinational path
    // (OR with a=0 passes the fetched byte through unchanged) so it shares
    // the same aluZero()/aluNegative() flag helpers as every other opcode;
    // C and V are real 6502 behavior left untouched. STA touches no flags
    // and needs no ALU involvement at all.
    void captureLoadImmediate();
    void commitLoadImmediate();
    void captureLoadZeroPage();
    void commitLoadZeroPage();
    void captureStoreZeroPage();
    void commitStoreZeroPage();
    void captureStoreAbsoluteY();
    void commitStoreAbsoluteY();
};
```

In `src/cpu/cpu6502.cpp`, add the opcode constants immediately after the existing `const uint8_t cOpDEY = 0x88;` line and before `const auto cCFlagOffset = 0;`:

```cpp
const uint8_t cOpDEY = 0x88;

const uint8_t cOpLDAImmediate = 0xA9;
const uint8_t cOpLDAZeroPage = 0xA5;
const uint8_t cOpSTAZeroPage = 0x85;
const uint8_t cOpSTAAbsoluteY = 0x99;

const auto cCFlagOffset = 0;
```

In `onClockHigh()`, add a new case group immediately before `default: break; // unimplemented opcode: nothing to capture`:

```cpp
    case cOpINX:
    case cOpDEX:
    case cOpINY:
    case cOpDEY:
        captureImpliedIncDec();
        break;
    case cOpLDAImmediate:
        captureLoadImmediate();
        break;
    case cOpLDAZeroPage:
        captureLoadZeroPage();
        break;
    case cOpSTAZeroPage:
        captureStoreZeroPage();
        break;
    case cOpSTAAbsoluteY:
        captureStoreAbsoluteY();
        break;
    default:
        break; // unimplemented opcode: nothing to capture
    }
}
```

In `onClockLow()`, add the matching case group immediately before `default: // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.`:

```cpp
    case cOpINX:
    case cOpDEX:
    case cOpINY:
    case cOpDEY:
        commitImpliedIncDec();
        break;
    case cOpLDAImmediate:
        commitLoadImmediate();
        break;
    case cOpLDAZeroPage:
        commitLoadZeroPage();
        break;
    case cOpSTAZeroPage:
        commitStoreZeroPage();
        break;
    case cOpSTAAbsoluteY:
        commitStoreAbsoluteY();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cpuStep = CpuStep::T0;
        break;
    }
}
```

At the end of the file (after `commitImpliedIncDec()`), append the eight new function bodies:

```cpp
void CPU6502::captureLoadImmediate() {
    m_aluA = 0;
    m_aluB = m_bus.read(m_PC);
    m_aluFunction = AluFunction::OR;
}

void CPU6502::commitLoadImmediate() {
    m_PC++;
    A(m_aluOutput.value);
    ZFlag(aluZero(m_aluOutput.value));
    NFlag(aluNegative(m_aluOutput.value));
    m_cpuStep = CpuStep::T0;
}

void CPU6502::captureLoadZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_aluA = 0;
        m_aluB = m_bus.read(m_addrLatch);
        m_aluFunction = AluFunction::OR;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::commitLoadZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        A(m_aluOutput.value);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::captureStoreZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        break; // idle: the store itself happens in commit, where m_A is authoritative
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::commitStoreZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_bus.write(m_addrLatch, m_A);
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::captureStoreAbsoluteY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base = static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        // STA absolute,Y timing is fixed at 5 cycles on real hardware (a
        // store can't shortcut the extra cycle the way a read can), so
        // .pageCrossed is deliberately not consulted -- see
        // captureRmwAbsoluteX for the same reasoning applied to RMW.
        EffectiveAddress resolved = indexedAddress(base, m_Y);
        m_effAddr = resolved.address;
        break;
    }
    case CpuStep::T3:
        break; // idle: stand-in for the real hardware's wrong-address read before the index fixup
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}

void CPU6502::commitStoreAbsoluteY() {
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
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_bus.write(m_effAddr, m_A);
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502LoadStoreTest`
Expected: PASS (all `CPU6502LoadStoreTest.*` tests green).

Then run the full suite to check for regressions: `ctest --test-dir build --output-on-failure`
Expected: PASS (no other test broken).

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_load_store_test.cpp test/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: add minimal LDA/STA opcodes (immediate/zero page/absolute,Y)

Prerequisite for the upcoming Fibonacci e2e test: this codebase had no way
to write a computed value into memory at all until now.
EOF
)"
```

---

## Task 2: Relative conditional branches (BEQ/BNE/BCS/BCC/BPL/BMI/BVS/BVC)

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_branch_test.cpp` (new)
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `m_bus`, `m_PC`, `m_effAddr`, `m_pageCrossed`, `CFlag()`, `ZFlag()`, `NFlag()`, `VFlag()`, the `EffectiveAddress` struct, and the existing `indexedAddress()` pattern as a model (not called directly).
- Produces: a new static helper `EffectiveAddress relativeAddress(uint16_t base, int8_t offset)`; opcodes `0x10`/`0x30`/`0x50`/`0x70`/`0x90`/`0xB0`/`0xD0`/`0xF0` become executable.

- [ ] **Step 1: Write the failing tests**

Create `test/cpu/cpu6502_branch_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502BranchTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502BranchTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502BranchTest, BeqBranchesWhenZeroFlagSet) {
    ram.write(0x0000, 0xF0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BeqDoesNotBranchWhenZeroFlagClear) {
    ram.write(0x0000, 0xF0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BneBranchesWhenZeroFlagClear) {
    ram.write(0x0000, 0xD0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BneDoesNotBranchWhenZeroFlagSet) {
    ram.write(0x0000, 0xD0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BcsBranchesWhenCarryFlagSet) {
    ram.write(0x0000, 0xB0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BcsDoesNotBranchWhenCarryFlagClear) {
    ram.write(0x0000, 0xB0);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BccBranchesWhenCarryFlagClear) {
    ram.write(0x0000, 0x90);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BccDoesNotBranchWhenCarryFlagSet) {
    ram.write(0x0000, 0x90);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BmiBranchesWhenNegativeFlagSet) {
    ram.write(0x0000, 0x30);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BmiDoesNotBranchWhenNegativeFlagClear) {
    ram.write(0x0000, 0x30);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BplBranchesWhenNegativeFlagClear) {
    ram.write(0x0000, 0x10);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BplDoesNotBranchWhenNegativeFlagSet) {
    ram.write(0x0000, 0x10);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.NFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BvsBranchesWhenOverflowFlagSet) {
    ram.write(0x0000, 0x70);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BvsDoesNotBranchWhenOverflowFlagClear) {
    ram.write(0x0000, 0x70);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, BvcBranchesWhenOverflowFlagClear) {
    ram.write(0x0000, 0x50);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, BvcDoesNotBranchWhenOverflowFlagSet) {
    ram.write(0x0000, 0x50);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.VFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, NegativeOffsetBranchesBackward) {
    ram.write(0x0010, 0xF0); // BEQ
    ram.write(0x0011, static_cast<uint8_t>(-5));
    cpu.reset();
    cpu.PC(0x0010);
    cpu.ZFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x000D); // 0x0012 - 5
}

TEST_F(CPU6502BranchTest, NotTakenBranchCompletesInTwoCycles) {
    ram.write(0x0000, 0xF0); // BEQ
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(false);

    for (int i = 0; i < 4; ++i) cpu.tick(); // T0
    for (int i = 0; i < 4; ++i) cpu.tick(); // T1

    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502BranchTest, TakenSamePageBranchCompletesInThreeCycles) {
    ram.write(0x0000, 0xF0); // BEQ
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.ZFlag(true);

    for (int i = 0; i < 4; ++i) cpu.tick(); // T0
    for (int i = 0; i < 4; ++i) cpu.tick(); // T1: operand consumed, target not yet committed
    EXPECT_EQ(cpu.PC(), 0x0002);

    for (int i = 0; i < 4; ++i) cpu.tick(); // T2: target committed (no page cross)

    EXPECT_EQ(cpu.PC(), 0x0007);
}

TEST_F(CPU6502BranchTest, TakenPageCrossedBranchCompletesInFourCycles) {
    ram.write(0x00FD, 0xF0); // BEQ at 0x00FD
    ram.write(0x00FE, 0x05); // operand +5 from 0x00FF -> target 0x0104, crosses page
    cpu.reset();
    cpu.PC(0x00FD);
    cpu.ZFlag(true);

    for (int i = 0; i < 4; ++i) cpu.tick(); // T0
    for (int i = 0; i < 4; ++i) cpu.tick(); // T1
    for (int i = 0; i < 4; ++i) cpu.tick(); // T2: page crossed, target not yet committed
    EXPECT_EQ(cpu.PC(), 0x00FF);

    for (int i = 0; i < 4; ++i) cpu.tick(); // T3: target committed

    EXPECT_EQ(cpu.PC(), 0x0104);
}
```

Add `cpu/cpu6502_branch_test.cpp` to `test/CMakeLists.txt`'s source list, next to the file added in Task 1:

```cmake
    cpu/cpu6502_load_store_test.cpp
    cpu/cpu6502_branch_test.cpp
    cpu/cpu6502_addressing_test.cpp
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502BranchTest`
Expected: FAIL — branch opcodes aren't dispatched yet, so PC never advances past the operand fetch as expected.

- [ ] **Step 3: Implement relative branches**

In `src/cpu/cpu6502.h`, add the new static helper next to `indexedAddress`:

```cpp
    static EffectiveAddress indexedAddress(uint16_t base, uint8_t index);
    static EffectiveAddress relativeAddress(uint16_t base, int8_t offset);
```

Add two new private members near the other capture-state members (after `uint8_t m_IR = 0;`):

```cpp
    uint8_t m_IR = 0;
    int8_t m_branchOffset = 0;
    bool m_branchTaken = false;
```

Add the new capture/commit declarations at the end of the private section, after the load/store declarations added in Task 1 (before the closing `};`):

```cpp
    void captureStoreAbsoluteY();
    void commitStoreAbsoluteY();

    // Relative branches (BEQ/BNE/BCS/BCC/BPL/BMI/BVS/BVC): one shared pair
    // for all 8, deciding the condition and target address from m_IR, the
    // same way applyBinaryAluOp() decides its operation from m_IR.
    void captureBranch();
    void commitBranch();
};
```

In `src/cpu/cpu6502.cpp`, add the opcode constants after the load/store constants added in Task 1:

```cpp
const uint8_t cOpSTAAbsoluteY = 0x99;

const uint8_t cOpBPL = 0x10;
const uint8_t cOpBMI = 0x30;
const uint8_t cOpBVC = 0x50;
const uint8_t cOpBVS = 0x70;
const uint8_t cOpBCC = 0x90;
const uint8_t cOpBCS = 0xB0;
const uint8_t cOpBNE = 0xD0;
const uint8_t cOpBEQ = 0xF0;
```

Add a branch-condition helper to the existing anonymous namespace that already holds `aluZero`/`aluNegative`/`aluOverflow` (append `evaluateBranchCondition` right after `aluOverflow`):

```cpp
bool aluOverflow(uint8_t a, uint8_t b, uint8_t d) { return ((~(a ^ b)) & (a ^ d) & 0x80) != 0; }

bool evaluateBranchCondition(uint8_t opcode, bool cFlag, bool zFlag, bool nFlag, bool vFlag) {
    switch (opcode) {
    case cOpBPL:
        return !nFlag;
    case cOpBMI:
        return nFlag;
    case cOpBVC:
        return !vFlag;
    case cOpBVS:
        return vFlag;
    case cOpBCC:
        return !cFlag;
    case cOpBCS:
        return cFlag;
    case cOpBNE:
        return !zFlag;
    case cOpBEQ:
        return zFlag;
    default:
        return false; // unreachable: only called for the 8 branch opcodes
    }
}
} // namespace
```

Add the `relativeAddress` static method next to `indexedAddress`'s definition:

```cpp
EffectiveAddress CPU6502::relativeAddress(uint16_t base, int8_t offset) {
    auto address = static_cast<uint16_t>(base + offset);
    bool pageCrossed = (base & 0xFF00) != (address & 0xFF00);
    return EffectiveAddress{address, pageCrossed};
}
```

In `onClockHigh()`, add a new case group immediately before `default: break; // unimplemented opcode: nothing to capture` (after the load/store cases added in Task 1):

```cpp
    case cOpSTAAbsoluteY:
        captureStoreAbsoluteY();
        break;
    case cOpBPL:
    case cOpBMI:
    case cOpBVC:
    case cOpBVS:
    case cOpBCC:
    case cOpBCS:
    case cOpBNE:
    case cOpBEQ:
        captureBranch();
        break;
    default:
        break; // unimplemented opcode: nothing to capture
    }
}
```

In `onClockLow()`, add the matching case group immediately before the final `default:` block (after the load/store cases added in Task 1):

```cpp
    case cOpSTAAbsoluteY:
        commitStoreAbsoluteY();
        break;
    case cOpBPL:
    case cOpBMI:
    case cOpBVC:
    case cOpBVS:
    case cOpBCC:
    case cOpBCS:
    case cOpBNE:
    case cOpBEQ:
        commitBranch();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cpuStep = CpuStep::T0;
        break;
    }
}
```

At the end of the file, append `captureBranch()`/`commitBranch()`:

```cpp
void CPU6502::captureBranch() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_branchOffset = static_cast<int8_t>(m_bus.read(m_PC));
        m_branchTaken = evaluateBranchCondition(m_IR, CFlag(), ZFlag(), NFlag(), VFlag());
        break;
    case CpuStep::T2: {
        EffectiveAddress resolved = relativeAddress(m_PC, m_branchOffset);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        break; // idle: stand-in for the dummy read while the high byte is fixed up
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}

void CPU6502::commitBranch() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = m_branchTaken ? CpuStep::T2 : CpuStep::T0;
        break;
    case CpuStep::T2:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T3;
        } else {
            m_PC = m_effAddr;
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T3:
        m_PC = m_effAddr;
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502BranchTest`
Expected: PASS.

Then: `ctest --test-dir build --output-on-failure`
Expected: PASS (no regressions).

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_branch_test.cpp test/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: implement relative conditional branches (BEQ/BNE/BCS/BCC/BPL/BMI/BVS/BVC)

Models real 6502 timing: 2 cycles not taken, 3 taken same-page, 4 taken
page-crossed, reusing the same EffectiveAddress/pageCrossed pattern the
indexed addressing modes already use.
EOF
)"
```

---

## Task 3: BRK (full interrupt semantics), `halted()`, and `run()` driver helper

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_brk_test.cpp` (new)
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `m_bus`, `m_PC`, `m_SP`, `cBRKVector`, `m_addrLatch`, `BFlag(bool)`, `P()`, `IFlag(bool)`, `executeInstruction()`. Uses `cOpBPL`/branch opcodes from Task 2 in one test (an infinite self-loop to prove `run()`'s instruction cap).
- Produces: `bool CPU6502::halted() const`; `bool CPU6502::run(size_t maxInstructions)`; opcode `0x00` (BRK) becomes executable.

- [ ] **Step 1: Write the failing tests**

Create `test/cpu/cpu6502_brk_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502BrkTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502BrkTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502BrkTest, BrkSetsHaltedAfterExecuting) {
    ram.write(0x0000, 0x00); // BRK
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();

    EXPECT_FALSE(cpu.halted());

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.halted());
}

TEST_F(CPU6502BrkTest, BrkPushesReturnAddressAndStatusOntoStack) {
    ram.write(0x0200, 0x00); // BRK
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();
    cpu.PC(0x0200);
    cpu.SP(0xFF);
    cpu.P(0x20); // only the always-set bit 5; I and B both clear

    cpu.executeInstruction();

    // BRK reads a padding byte after its opcode, so the pushed return
    // address is PC + 2 = 0x0202.
    EXPECT_EQ(ram.read(0x01FF), 0x02); // PCH
    EXPECT_EQ(ram.read(0x01FE), 0x02); // PCL
    EXPECT_EQ(ram.read(0x01FD), 0x30); // P with B (0x10) and bit5 (0x20) set
    EXPECT_EQ(cpu.SP(), 0xFC);
}

TEST_F(CPU6502BrkTest, BrkSetsInterruptDisableFlag) {
    ram.write(0x0000, 0x00);
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();
    cpu.IFlag(false);

    cpu.executeInstruction();

    EXPECT_TRUE(cpu.IFlag());
}

TEST_F(CPU6502BrkTest, BrkLoadsPCFromBrkVector) {
    ram.write(0x0000, 0x00);
    ram.write(0xFFFE, 0x34);
    ram.write(0xFFFF, 0x12);
    cpu.reset();

    cpu.executeInstruction();

    EXPECT_EQ(cpu.PC(), 0x1234);
}

TEST_F(CPU6502BrkTest, SevenTicksCompleteBrk) {
    ram.write(0x0000, 0x00);
    ram.write(0xFFFE, 0x34);
    ram.write(0xFFFF, 0x12);
    cpu.reset();

    for (int step = 0; step < 7; ++step) {
        for (int i = 0; i < 4; ++i) cpu.tick();
    }

    EXPECT_EQ(cpu.PC(), 0x1234);
    EXPECT_TRUE(cpu.halted());
}

TEST_F(CPU6502BrkTest, ResetClearsHaltedFlagAfterBrk) {
    ram.write(0x0000, 0x00); // BRK
    cpu.reset();
    cpu.executeInstruction();
    ASSERT_TRUE(cpu.halted());

    cpu.reset();

    EXPECT_FALSE(cpu.halted());
}

TEST_F(CPU6502BrkTest, RunExecutesUntilHaltedAndReturnsTrue) {
    ram.write(0x0000, 0xE8); // INX
    ram.write(0x0001, 0xE8); // INX
    ram.write(0x0002, 0x00); // BRK
    ram.write(0xFFFE, 0x00);
    ram.write(0xFFFF, 0x90);
    cpu.reset();

    bool halted = cpu.run(100);

    EXPECT_TRUE(halted);
    EXPECT_EQ(cpu.X(), 2);
    EXPECT_TRUE(cpu.halted());
}

TEST_F(CPU6502BrkTest, RunReturnsFalseWhenInstructionCapReachedWithoutHalting) {
    ram.write(0x0000, 0x10); // BPL -2: unconditional infinite loop (N flag stays clear)
    ram.write(0x0001, static_cast<uint8_t>(-2));
    cpu.reset();

    bool halted = cpu.run(10);

    EXPECT_FALSE(halted);
    EXPECT_FALSE(cpu.halted());
}
```

Add `cpu/cpu6502_brk_test.cpp` to `test/CMakeLists.txt`'s source list, next to the branch test:

```cmake
    cpu/cpu6502_branch_test.cpp
    cpu/cpu6502_brk_test.cpp
    cpu/cpu6502_addressing_test.cpp
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502BrkTest`
Expected: build fails (`halted()`/`run()` don't exist yet) or, once stubbed, tests fail because BRK isn't dispatched.

- [ ] **Step 3: Implement BRK, `halted()`, and `run()`**

In `src/cpu/cpu6502.h`, add the two new public methods after `void NFlag(bool val);` and before `protected:`:

```cpp
    void NFlag(bool val);

    //--------------------------------------
    // Halt / run control

    bool halted() const;
    bool run(size_t maxInstructions);

protected:
```

Add `#include <cstddef>` to the top-of-file include block (needed for `size_t`):

```cpp
#include "alu.h"
#include "cpu.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
```

Add the new member `m_halted` next to `m_branchOffset`/`m_branchTaken` (added in Task 2):

```cpp
    uint8_t m_IR = 0;
    int8_t m_branchOffset = 0;
    bool m_branchTaken = false;
    bool m_halted = false;
```

Add the two new private handler declarations at the end of the private section, after `commitBranch();` (before the closing `};`):

```cpp
    void captureBranch();
    void commitBranch();

    // BRK (full interrupt semantics): pushes PC+2 and P (with B forced to
    // 1) onto the stack, sets the I flag, and loads PC from cBRKVector.
    // m_halted is the actual "catch a BRK" mechanism a driver uses to stop
    // issuing further instructions -- see run().
    void captureBRK();
    void commitBRK();
};
```

In `src/cpu/cpu6502.cpp`, add the `run()` and `halted()` accessor bodies right after `void CPU6502::SP(uint8_t val) { m_SP = val; }`:

```cpp
void CPU6502::SP(uint8_t val) { m_SP = val; }

bool CPU6502::halted() const { return m_halted; }

bool CPU6502::run(size_t maxInstructions) {
    for (size_t i = 0; i < maxInstructions && !m_halted; ++i) {
        executeInstruction();
    }
    return m_halted;
}
```

In `CPU6502::reset()`, add `m_halted = false;` as the first line:

```cpp
void CPU6502::reset() {
    m_halted = false;
    A(0);
    X(0);
```

Add the opcode constant right after the file's other opcode constants — place it with the other single-byte opcodes, immediately before `const uint8_t cOpADCImmediate = 0x69;` at the top of the constant block:

```cpp
const uint8_t cOpBRK = 0x00;

const uint8_t cOpADCImmediate = 0x69;
```

In `onClockHigh()`, add a case for BRK immediately before `default: break; // unimplemented opcode: nothing to capture` (after the branch cases added in Task 2):

```cpp
    case cOpBEQ:
        captureBranch();
        break;
    case cOpBRK:
        captureBRK();
        break;
    default:
        break; // unimplemented opcode: nothing to capture
    }
}
```

In `onClockLow()`, add the matching case immediately before the final `default:` block (after the branch cases added in Task 2):

```cpp
    case cOpBEQ:
        commitBranch();
        break;
    case cOpBRK:
        commitBRK();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cpuStep = CpuStep::T0;
        break;
    }
}
```

At the end of the file, append `captureBRK()`/`commitBRK()`:

```cpp
void CPU6502::captureBRK() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        static_cast<void>(m_bus.read(m_PC)); // dummy read: padding byte after BRK, discarded
        break;
    case CpuStep::T2:
    case CpuStep::T3:
    case CpuStep::T4:
        break; // idle: the pushes happen in commit, where m_SP is authoritative
    case CpuStep::T5:
        m_addrLatch = m_bus.read(cBRKVector);
        break;
    case CpuStep::T6:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(static_cast<uint16_t>(cBRKVector + 1))) << 8;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T6.
    }
}

void CPU6502::commitBRK() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), static_cast<uint8_t>(m_PC >> 8));
        m_SP--;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), static_cast<uint8_t>(m_PC & 0xFF));
        m_SP--;
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        BFlag(true);
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), P());
        m_SP--;
        IFlag(true);
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        m_cpuStep = CpuStep::T6;
        break;
    case CpuStep::T6:
        m_PC = m_addrLatch;
        m_halted = true;
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T6.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502BrkTest`
Expected: PASS.

Then: `ctest --test-dir build --output-on-failure`
Expected: PASS (no regressions).

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_brk_test.cpp test/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: implement BRK with full interrupt semantics, halted(), and run()

BRK pushes PC+2 and P (B forced set), sets I, and jumps through
cBRKVector -- then sets a halted flag a driver loop can check via the new
run(maxInstructions) helper, which is how a caller "catches" the BRK to
stop processing without needing an actual interrupt handler at the vector.
EOF
)"
```

---

## Task 4: Hex-string program loader

**Files:**
- Create: `src/utils/program_loader.h`
- Create: `src/utils/program_loader.cpp`
- Test: `test/utils/program_loader_test.cpp` (new)
- Modify: `CMakeLists.txt` (register the new source file for the `sys6` executable)
- Modify: `test/CMakeLists.txt` (register the new source and test files)

**Interfaces:**
- Consumes: `MemoryDevice` (`src/memory/memory_device.h`) — `write(uint16_t offset, uint8_t val)`.
- Produces: `void loadProgram(MemoryDevice &device, uint16_t startAddress, const std::string &hex)`, throwing `std::invalid_argument` on malformed input. Used directly by Task 5.

- [ ] **Step 1: Write the failing tests**

Create `src/utils/program_loader.h`:

```cpp
#pragma once

#include "memory/memory_device.h"

#include <cstdint>
#include <string>

void loadProgram(MemoryDevice &device, uint16_t startAddress, const std::string &hex);
```

Create `test/utils/program_loader_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "memory/ram.h"
#include "utils/program_loader.h"

#include <stdexcept>

TEST(ProgramLoader, LoadsWhitespaceSeparatedHexIntoRam) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "A9 05 8D 00 02");

    EXPECT_EQ(ram.read(0), 0xA9);
    EXPECT_EQ(ram.read(1), 0x05);
    EXPECT_EQ(ram.read(2), 0x8D);
    EXPECT_EQ(ram.read(3), 0x00);
    EXPECT_EQ(ram.read(4), 0x02);
}

TEST(ProgramLoader, LoadsUnseparatedHexIntoRam) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "A9058D0002");

    EXPECT_EQ(ram.read(0), 0xA9);
    EXPECT_EQ(ram.read(1), 0x05);
    EXPECT_EQ(ram.read(2), 0x8D);
    EXPECT_EQ(ram.read(3), 0x00);
    EXPECT_EQ(ram.read(4), 0x02);
}

TEST(ProgramLoader, LoadsLowercaseHex) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "a9 ff");

    EXPECT_EQ(ram.read(0), 0xA9);
    EXPECT_EQ(ram.read(1), 0xFF);
}

TEST(ProgramLoader, WritesStartingAtGivenAddress) {
    RAM ram(16);

    loadProgram(ram, 0x0005, "AA BB");

    EXPECT_EQ(ram.read(5), 0xAA);
    EXPECT_EQ(ram.read(6), 0xBB);
}

TEST(ProgramLoader, ThrowsOnOddLengthHex) {
    RAM ram(16);

    EXPECT_THROW(loadProgram(ram, 0x0000, "A9B"), std::invalid_argument);
}

TEST(ProgramLoader, ThrowsOnNonHexCharacter) {
    RAM ram(16);

    EXPECT_THROW(loadProgram(ram, 0x0000, "ZZ"), std::invalid_argument);
}

TEST(ProgramLoader, EmptyStringWritesNothing) {
    RAM ram(16);

    loadProgram(ram, 0x0000, "");

    EXPECT_EQ(ram.read(0), 0x00);
}
```

Register the new files. In `CMakeLists.txt`, add `src/utils/program_loader.cpp` to `add_executable(sys6 ...)`, next to `src/utils/log.cpp`:

```cmake
add_executable(sys6
    src/main.cpp
    src/cpu/alu.cpp
    src/cpu/cpu6502.cpp
    src/memory/ram.cpp
    src/memory/rom.cpp
    src/memory/bus.cpp
    src/utils/log.cpp
    src/utils/program_loader.cpp
)
```

In `test/CMakeLists.txt`, add the test file and the new source file to `add_executable(sys6_tests ...)`:

```cmake
add_executable(sys6_tests
    cpu/alu_test.cpp
    cpu/cpu6502_adc_test.cpp
    cpu/cpu6502_sbc_test.cpp
    cpu/cpu6502_and_test.cpp
    cpu/cpu6502_ora_test.cpp
    cpu/cpu6502_eor_test.cpp
    cpu/cpu6502_cmp_test.cpp
    cpu/cpu6502_cpx_cpy_test.cpp
    cpu/cpu6502_shift_test.cpp
    cpu/cpu6502_inc_dec_test.cpp
    cpu/cpu6502_load_store_test.cpp
    cpu/cpu6502_branch_test.cpp
    cpu/cpu6502_brk_test.cpp
    cpu/cpu6502_addressing_test.cpp
    cpu/cpu6502_clock_test.cpp
    cpu/cpu6502_test.cpp
    memory/ram_test.cpp
    memory/rom_test.cpp
    memory/bus_test.cpp
    utils/program_loader_test.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/alu.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/cpu6502.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/ram.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/rom.cpp
    ${CMAKE_SOURCE_DIR}/src/memory/bus.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/log.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/program_loader.cpp
)
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake -S . -B build && cmake --build build`
Expected: link failure — `test/utils/program_loader_test.cpp` references `loadProgram`, which has no definition yet (`program_loader.cpp` doesn't exist).

- [ ] **Step 3: Implement `loadProgram`**

Create `src/utils/program_loader.cpp`:

```cpp
#include "program_loader.h"

#include <cctype>
#include <stdexcept>

namespace {
int hexDigitValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    throw std::invalid_argument("loadProgram: invalid hex character");
}
} // namespace

void loadProgram(MemoryDevice &device, uint16_t startAddress, const std::string &hex) {
    std::string digits;
    for (char c : hex) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            digits.push_back(c);
        }
    }

    if (digits.size() % 2 != 0) {
        throw std::invalid_argument("loadProgram: hex string has odd digit count");
    }

    uint16_t address = startAddress;
    for (size_t i = 0; i < digits.size(); i += 2) {
        auto byte = static_cast<uint8_t>((hexDigitValue(digits[i]) << 4) | hexDigitValue(digits[i + 1]));
        device.write(address, byte);
        address = static_cast<uint16_t>(address + 1);
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R ProgramLoader`
Expected: PASS.

Then: `ctest --test-dir build --output-on-failure`
Expected: PASS (no regressions).

- [ ] **Step 5: Commit**

```bash
git add src/utils/program_loader.h src/utils/program_loader.cpp test/utils/program_loader_test.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: add loadProgram() hex-string utility for loading programs into memory
EOF
)"
```

---

## Task 5: Fibonacci end-to-end test

**Files:**
- Test: `test/cpu/cpu6502_fibonacci_e2e_test.cpp` (new)
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `loadProgram()` (Task 4), `CPU6502::reset()`, `CPU6502::run(size_t)` (Task 3), `RAM::read(uint16_t)`.
- Produces: nothing new — this is a pure integration test proving Tasks 1-4 work together.

The hand-assembled program (see `docs/superpowers/specs/2026-08-08-relative-branches-and-utilities-design.md` for the derivation and hand-trace) is:

```
Addr   Bytes        Instruction
0000   A9 00        LDA #$00
0002   85 F0        STA $F0        ; a = 0
0004   A9 01        LDA #$01
0006   85 F1        STA $F1        ; b = 1
0008   A9 0A        LDA #$0A
000A   85 F3        STA $F3        ; counter = 10
000C   A5 F0        LDA $F0        <- LOOP
000E   99 00 02     STA $0200,Y    ; output[Y] = a
0011   65 F1        ADC $F1        ; A = a + b = newB
0013   85 F2        STA $F2        ; scratch = newB
0015   A5 F1        LDA $F1        ; A = b
0017   85 F0        STA $F0        ; a = old b
0019   A5 F2        LDA $F2        ; A = newB
001B   85 F1        STA $F1        ; b = newB
001D   C8           INY
001E   C6 F3        DEC $F3        ; counter--
0020   D0 EA        BNE LOOP       ; offset -22 = 0x000C - 0x0022
0022   00           BRK
```

This produces 0, 1, 1, 2, 3, 5, 8, 13, 21, 34 at `$0200-$0209`.

- [ ] **Step 1: Write the failing test**

Create `test/cpu/cpu6502_fibonacci_e2e_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"
#include "utils/program_loader.h"

TEST(CPU6502FibonacciE2E, ComputesAndStoresFirstTenFibonacciNumbers) {
    RAM ram(0x10000);
    Bus bus;
    bus.attach(0x0000, 0xFFFF, ram);
    CPU6502 cpu(bus);

    const std::string program =
        "A9 00 85 F0"       // LDA #$00 ; STA $F0        (a = 0)
        "A9 01 85 F1"       // LDA #$01 ; STA $F1        (b = 1)
        "A9 0A 85 F3"       // LDA #$0A ; STA $F3        (counter = 10)
        "A5 F0"             // LOOP: LDA $F0             (A = a)
        "99 00 02"          //   STA $0200,Y             (output[Y] = a)
        "65 F1"             //   ADC $F1                 (A = a + b)
        "85 F2"             //   STA $F2                 (scratch = newB)
        "A5 F1 85 F0"       //   LDA $F1 ; STA $F0        (a = old b)
        "A5 F2 85 F1"       //   LDA $F2 ; STA $F1        (b = newB)
        "C8"                //   INY
        "C6 F3"             //   DEC $F3                 (counter--)
        "D0 EA"             //   BNE LOOP
        "00";               // BRK

    loadProgram(ram, 0x0000, program);
    cpu.reset();

    ASSERT_TRUE(cpu.run(10000));

    const uint8_t expected[10] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
    for (uint16_t i = 0; i < 10; ++i) {
        EXPECT_EQ(ram.read(static_cast<uint16_t>(0x0200 + i)), expected[i]) << "index " << i;
    }
}
```

Add `cpu/cpu6502_fibonacci_e2e_test.cpp` to `test/CMakeLists.txt`'s source list, next to the BRK test:

```cmake
    cpu/cpu6502_brk_test.cpp
    cpu/cpu6502_fibonacci_e2e_test.cpp
    cpu/cpu6502_addressing_test.cpp
```

- [ ] **Step 2: Run test to verify it fails or passes on first try**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R CPU6502FibonacciE2E`

This test depends only on functionality already implemented and verified in Tasks 1-4 (it introduces no new production code), so it is expected to **pass immediately** if the earlier tasks are correct. If it fails, do not modify `cpu6502.cpp`/`cpu6502.h`/`program_loader.cpp` to special-case this test — treat a failure here as a signal that an earlier task's implementation or the hand-assembled program's hand-traced addresses/offsets have a bug, and re-derive from the design spec's hand-trace before changing anything.

- [ ] **Step 3: N/A**

(No implementation step: this task is pure verification. Skip to Step 4.)

- [ ] **Step 4: Run the full suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS — every test from every task, plus this e2e test, all green.

- [ ] **Step 5: Commit**

```bash
git add test/cpu/cpu6502_fibonacci_e2e_test.cpp test/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test: add Fibonacci e2e test exercising branches, BRK, and the loader together

Loads a hand-assembled program via loadProgram(), runs it via
CPU6502::run() to completion (BRK), and asserts the first 10 Fibonacci
numbers land in memory at $0200-$0209.
EOF
)"
```

---

## Final Verification

After all 5 tasks are committed:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: full green suite, including `CPU6502LoadStoreTest.*`, `CPU6502BranchTest.*`, `CPU6502BrkTest.*`, `ProgramLoader.*`, and `CPU6502FibonacciE2E.ComputesAndStoresFirstTenFibonacciNumbers`.
