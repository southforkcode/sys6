# Cycle-stepped CPU core + ALU unit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn `CPU6502` into a true cycle-stepped state machine and introduce `ALU` as its first internal unit, proven end-to-end with `ADC #immediate` (0x69, 2 cycles) and `ADC absolute` (0x6D, 4 cycles).

**Architecture:** `CPU6502::tick()` advances exactly one clock cycle; `m_cycle == 0` always means "about to fetch the next opcode" and doubles as the completion sentinel `executeInstruction()`'s wrapper loop watches. Opcode-specific behavior is dispatched from `m_IR` (the latched opcode) to small per-opcode handler methods that switch on `m_cycle`. `ALU` is a separate, stateless class that only computes — `CPU6502` calls it and applies the result to its own registers/flags.

**Tech Stack:** C++17, CMake + CTest, GoogleTest (already fetched via `FetchContent`), clang-format/clang-tidy (LLVM style, 4-space indent, `.clang-format`/`.clang-tidy` already configured).

## Global Constraints

- C++17, existing `.clang-format` (LLVM style, 4-space indent, 100 column limit) and `.clang-tidy` (`bugprone-*, modernize-*, performance-*, readability-*`) apply to every new/modified file.
- Binary mode only for ADC — `DFlag()` is not consulted. (Spec non-goal.)
- Only `ADC #immediate` and `ADC absolute` are implemented this pass. No other ALU ops, no other addressing modes, no opcode dispatch table. (Spec non-goals.)
- No manufactured "dummy" bus-read cycles. A cycle only touches the bus when it has real work to do.
- `executeInstruction()` must keep working exactly as before for existing callers/tests — it still completes one full instruction per call.
- **Deviation from the spec's stated latch list:** the spec mentions a general `m_dataLatch` ("last byte read from the bus"). Neither `ADC #immediate` nor `ADC absolute` actually needs a byte to persist unconsumed across a cycle boundary — the immediate operand is consumed straight into the ALU call, and the absolute-mode address bytes are folded directly into `m_addrLatch`. Adding an unused member would just be dead code (and risks a `-Wunused-private-field` warning), so `m_dataLatch` is **not** added in this plan. Add it in a future pass when an opcode actually needs to hold a fetched byte across a cycle it doesn't immediately consume it in.

---

## Task 1: `ALU` unit — `adc`

**Files:**
- Create: `src/cpu/alu.h`
- Create: `src/cpu/alu.cpp`
- Test: `test/cpu/alu_test.cpp`
- Modify: `CMakeLists.txt:12-19` (add `src/cpu/alu.cpp` to the `sys6` executable's sources)
- Modify: `test/CMakeLists.txt:3-13` (add `cpu/alu_test.cpp` and `${CMAKE_SOURCE_DIR}/src/cpu/alu.cpp` to `sys6_tests`)

**Interfaces:**
- Produces: `struct AluResult { uint8_t value; bool carry; bool zero; bool overflow; bool negative; };` and `class ALU { public: AluResult adc(uint8_t a, uint8_t operand, bool carryIn) const; };`, both in `src/cpu/alu.h`. Later tasks call `ALU::adc` from `CPU6502`.

- [ ] **Step 1: Write the failing tests**

Create `test/cpu/alu_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "cpu/alu.h"

TEST(ALUTest, AdcAddsTwoValuesWithNoCarryIn) {
    ALU alu;
    AluResult r = alu.adc(0x10, 0x05, false);

    EXPECT_EQ(r.value, 0x15);
    EXPECT_FALSE(r.carry);
    EXPECT_FALSE(r.zero);
    EXPECT_FALSE(r.overflow);
    EXPECT_FALSE(r.negative);
}

TEST(ALUTest, AdcAddsCarryInWhenSet) {
    ALU alu;
    AluResult r = alu.adc(0x10, 0x05, true);

    EXPECT_EQ(r.value, 0x16);
}

TEST(ALUTest, AdcSetsCarryAndZeroOnUnsignedOverflowToZero) {
    ALU alu;
    AluResult r = alu.adc(0xFF, 0x01, false);

    EXPECT_EQ(r.value, 0x00);
    EXPECT_TRUE(r.carry);
    EXPECT_TRUE(r.zero);
}

TEST(ALUTest, AdcClearsZeroFlagWhenResultIsNonZero) {
    ALU alu;
    AluResult r = alu.adc(0x01, 0x00, false);

    EXPECT_FALSE(r.zero);
}

TEST(ALUTest, AdcSetsNegativeFlagWhenBit7OfResultIsSet) {
    ALU alu;
    AluResult r = alu.adc(0x50, 0x50, false);

    EXPECT_EQ(r.value, 0xA0);
    EXPECT_TRUE(r.negative);
}

TEST(ALUTest, AdcSetsOverflowWhenTwoPositivesOverflowToNegative) {
    ALU alu;
    AluResult r = alu.adc(0x7F, 0x01, false);

    EXPECT_EQ(r.value, 0x80);
    EXPECT_TRUE(r.overflow);
    EXPECT_TRUE(r.negative);
}

TEST(ALUTest, AdcClearsOverflowWhenOperandsHaveDifferentSigns) {
    ALU alu;
    AluResult r = alu.adc(0x50, 0xFF, false);

    EXPECT_EQ(r.value, 0x4F);
    EXPECT_TRUE(r.carry);
    EXPECT_FALSE(r.overflow);
}

TEST(ALUTest, AdcSetsOverflowWhenTwoNegativesOverflowToPositive) {
    ALU alu;
    AluResult r = alu.adc(0x80, 0x80, false);

    EXPECT_EQ(r.value, 0x00);
    EXPECT_TRUE(r.carry);
    EXPECT_TRUE(r.overflow);
    EXPECT_FALSE(r.negative);
}
```

Add the new files to `test/CMakeLists.txt` now (needed just to compile this test):

```cmake
add_executable(sys6_tests
    cpu/alu_test.cpp
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

Also add `src/cpu/alu.cpp` to the main `sys6` target in `CMakeLists.txt:12-19`:

```cmake
add_executable(sys6
    src/main.cpp
    src/cpu/alu.cpp
    src/cpu/cpu6502.cpp
    src/memory/ram.cpp
    src/memory/rom.cpp
    src/memory/bus.cpp
    src/utils/log.cpp
)
```

Create an empty `src/cpu/alu.h` and `src/cpu/alu.cpp` for now (just enough to fail to compile meaningfully — actually, skip straight to Step 2, the build failure IS the failing test here since `alu.h`/`ALU`/`AluResult` don't exist yet).

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests`
Expected: FAIL — compile error, `cpu/alu.h` not found (or `ALU`/`AluResult` undeclared).

- [ ] **Step 3: Write the minimal implementation**

Create `src/cpu/alu.h`:

```cpp
#pragma once

#include <cstdint>

struct AluResult {
    uint8_t value;
    bool carry;
    bool zero;
    bool overflow;
    bool negative;
};

class ALU {
public:
    AluResult adc(uint8_t a, uint8_t operand, bool carryIn) const;
};
```

Create `src/cpu/alu.cpp`:

```cpp
#include "alu.h"

AluResult ALU::adc(uint8_t a, uint8_t operand, bool carryIn) const {
    int sum = static_cast<int>(a) + static_cast<int>(operand) + (carryIn ? 1 : 0);
    uint8_t value = static_cast<uint8_t>(sum & 0xFF);

    AluResult result;
    result.value = value;
    result.carry = sum > 0xFF;
    result.overflow = ((~(a ^ operand)) & (a ^ value) & 0x80) != 0;
    result.zero = value == 0;
    result.negative = (value & 0x80) != 0;
    return result;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ./build/test/sys6_tests --gtest_filter=ALUTest.*`
Expected: PASS, 8 tests.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/alu.h src/cpu/alu.cpp test/cpu/alu_test.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "feat: add ALU unit with adc()"
```

---

## Task 2: `CPU6502` cycle-stepping skeleton

**Files:**
- Modify: `src/cpu/cpu6502.h:1-63`
- Modify: `src/cpu/cpu6502.cpp:1-101`
- Test: `test/cpu/cpu6502_test.cpp` (append new tests; existing tests must keep passing unmodified)

**Interfaces:**
- Consumes: nothing new from Task 1 yet (ALU isn't wired in until Task 3).
- Produces: `void CPU6502::tick()` (public) — advances one clock cycle. Protected state: `uint8_t m_cycle`, `uint8_t m_IR`, `uint16_t m_addrLatch`, all default-initialized to `0`. `executeInstruction()` keeps its existing public signature but is now implemented as a `tick()`-driven loop. Later tasks add `case` branches to the `switch (m_IR)` inside `tick()` and add private handler methods alongside it.

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_test.cpp` (inside the file, after the existing tests, before the final closing — i.e. just add these as new `TEST_F` blocks using the existing `CPU6502Test` fixture):

```cpp
TEST_F(CPU6502Test, TickPerformsOpcodeFetchOnFirstCall) {
    ram.write(0x0000, 0x42);
    cpu.reset();

    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502Test, TickCompletesUnimplementedOpcodeAsOneCycleNoOp) {
    ram.write(0x0000, 0x42);
    ram.write(0x0001, 0x99);
    cpu.reset();

    cpu.tick(); // cycle 0 of first instruction: fetch 0x42, PC -> 1
    cpu.tick(); // cycle 1 of first instruction: unimplemented, no-op, completes

    EXPECT_EQ(cpu.PC(), 0x0001);

    cpu.tick(); // cycle 0 of second instruction: fetch 0x99, PC -> 2

    EXPECT_EQ(cpu.PC(), 0x0002);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests`
Expected: FAIL — compile error, `CPU6502` has no member `tick`.

- [ ] **Step 3: Write the minimal implementation**

In `src/cpu/cpu6502.h`, add the `tick()` declaration to the public section (right after `executeInstruction()` on line 15):

```cpp
    void reset() override;
    void executeInstruction() override;
    void tick();
```

Add new protected state members after `std::bitset<8> m_pFlags;` (line 61):

```cpp
    std::bitset<8> m_pFlags;

    uint8_t m_cycle = 0;
    uint16_t m_addrLatch = 0;
    uint8_t m_IR = 0;
```

Add a private section at the end of the class (after the protected block, before the closing `};`) for the opcode dispatch switch's future handler methods — empty for now except a comment, since Task 2 has no opcode handlers yet:

```cpp
private:
    // Per-opcode cycle handlers are added here as opcodes are implemented.
```

In `src/cpu/cpu6502.cpp`, replace the existing `executeInstruction()` (lines 89-100) with:

```cpp
void CPU6502::executeInstruction() {
    do {
        tick();
    } while (m_cycle != 0);
}

void CPU6502::tick() {
    if (m_cycle == 0) {
        uint8_t opcode = m_bus.read(m_PC);
        m_IR = opcode;
        m_PC++;

        if (m_tracing && m_logger) {
            std::ostringstream oss;
            oss << "Fetched opcode 0x" << std::hex << std::uppercase << static_cast<int>(opcode)
                << " at PC 0x" << (m_PC - 1);
            m_logger->trace(oss.str());
        }

        m_cycle = 1;
        return;
    }

    switch (m_IR) {
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cycle = 0;
        break;
    }
}
```

(The tracing log line moves from the old `executeInstruction()` body into `tick()`'s cycle-0 branch, preserving the existing trace-on-fetch behavior with the same message format — `m_PC - 1` because `m_PC` has already been incremented by this point.)

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ./build/test/sys6_tests --gtest_filter=CPU6502*`
Expected: PASS, all existing `CPU6502Test`/`CPU6502Smoke` tests plus the 2 new ones.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_test.cpp
git commit -m "feat: make CPU6502 a cycle-stepped state machine via tick()"
```

---

## Task 3: Wire `ADC #immediate` (0x69)

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Create: `test/cpu/cpu6502_adc_test.cpp`
- Modify: `test/CMakeLists.txt` (add `cpu/cpu6502_adc_test.cpp`)

**Interfaces:**
- Consumes: `ALU::adc(uint8_t a, uint8_t operand, bool carryIn) const -> AluResult` (Task 1); `tick()`/`m_cycle`/`m_IR` skeleton (Task 2).
- Produces: private `CPU6502::applyAdc(uint8_t operand)` and `CPU6502::tickADCImmediate()`, both reused/extended by Task 4.

- [ ] **Step 1: Write the failing tests**

Create `test/cpu/cpu6502_adc_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502AdcTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502 cpu{bus};

    CPU6502AdcTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502AdcTest, FirstTickAdvancesPCPastOpcodeOnly) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();

    cpu.tick();

    EXPECT_EQ(cpu.PC(), 0x0001);
}

TEST_F(CPU6502AdcTest, AdcImmediateAddsOperandToAccumulator) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcImmediateIncludesIncomingCarry) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(true);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x16);
}

TEST_F(CPU6502AdcTest, AdcImmediateSetsCarryAndZeroOnUnsignedOverflow) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x01);
    cpu.reset();
    cpu.A(0xFF);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x00);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.ZFlag());
}

TEST_F(CPU6502AdcTest, AdcImmediateSetsOverflowAndNegativeOnSignedOverflow) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x01);
    cpu.reset();
    cpu.A(0x7F);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x80);
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST_F(CPU6502AdcTest, AdcImmediateTakesExactlyTwoTicksToComplete) {
    ram.write(0x0000, 0x69);
    ram.write(0x0001, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.tick(); // cycle 0: fetch opcode only

    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // cycle 1: fetch operand, apply ALU, complete

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}
```

Add `cpu/cpu6502_adc_test.cpp` to `test/CMakeLists.txt`'s `sys6_tests` sources (alongside `cpu/cpu6502_test.cpp`).

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests`
Expected: FAIL — either a compile error (new test file references only existing API, so it should compile) or, once compiling, test failures because `0x69` currently falls into the unimplemented-opcode `default` case, so `A()` never changes from its initial value and the "takes exactly two ticks" test's first assertion (`A() == 0x10`) may pass by accident but the post-executeInstruction assertions (`A() == 0x15`, etc.) fail.

- [ ] **Step 3: Write the minimal implementation**

In `src/cpu/cpu6502.h`, add the include and `ALU` member. At the top, alongside the existing includes:

```cpp
#pragma once

#include "alu.h"
#include "cpu.h"

#include <bitset>
#include <cstdint>
```

Add `ALU m_alu;` to the protected member block, alongside the other new state:

```cpp
    uint8_t m_cycle = 0;
    uint16_t m_addrLatch = 0;
    uint8_t m_IR = 0;
    ALU m_alu;
```

Fill in the private section with the new handler declarations:

```cpp
private:
    void applyAdc(uint8_t operand);
    void tickADCImmediate();
```

In `src/cpu/cpu6502.cpp`, add an opcode constant near the top (alongside the existing `cNMIVector`/`cResetVector`/`cBRKVector` constants):

```cpp
const uint8_t cOpADCImmediate = 0x69;
```

Add the `case` to `tick()`'s switch:

```cpp
    switch (m_IR) {
    case cOpADCImmediate:
        tickADCImmediate();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cycle = 0;
        break;
    }
```

Add the two new methods (e.g. after `tick()`):

```cpp
void CPU6502::applyAdc(uint8_t operand) {
    AluResult result = m_alu.adc(m_A, operand, CFlag());
    A(result.value);
    CFlag(result.carry);
    ZFlag(result.zero);
    VFlag(result.overflow);
    NFlag(result.negative);
}

void CPU6502::tickADCImmediate() {
    uint8_t operand = m_bus.read(m_PC);
    m_PC++;
    applyAdc(operand);
    m_cycle = 0;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ./build/test/sys6_tests --gtest_filter=CPU6502AdcTest.*`
Expected: PASS, all 6 tests. Then run the full suite to confirm no regressions: `ctest --test-dir build --output-on-failure`.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp test/CMakeLists.txt
git commit -m "feat: wire ADC #immediate through the ALU unit"
```

---

## Task 4: Wire `ADC absolute` (0x6D)

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Modify: `test/cpu/cpu6502_adc_test.cpp` (append tests)

**Interfaces:**
- Consumes: `CPU6502::applyAdc(uint8_t operand)` (Task 3), `m_addrLatch` (Task 2).
- Produces: private `CPU6502::tickADCAbsolute()`.

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_adc_test.cpp`:

```cpp
TEST_F(CPU6502AdcTest, AdcAbsoluteAddsOperandFromMemory) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x00); // address low byte
    ram.write(0x0002, 0x02); // address high byte -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteAssemblesAddressLowByteFirst) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x34); // address low byte
    ram.write(0x0002, 0x12); // address high byte -> effective address 0x1234
    ram.write(0x1234, 0x01);
    cpu.reset();
    cpu.A(0x01);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x02);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteLeavesAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x6D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);

    cpu.tick(); // cycle 0: fetch opcode
    EXPECT_EQ(cpu.PC(), 0x0001);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // cycle 1: fetch address low byte
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // cycle 2: fetch address high byte
    EXPECT_EQ(cpu.PC(), 0x0003);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick(); // cycle 3: read operand from memory, apply ALU

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests && ./build/test/sys6_tests --gtest_filter=CPU6502AdcTest.*Absolute*`
Expected: FAIL — `0x6D` falls into the unimplemented-opcode `default` case, so `A()` never changes and `PC()` only advances by 1 (the opcode fetch), not 3.

- [ ] **Step 3: Write the minimal implementation**

In `src/cpu/cpu6502.h`, add the declaration to the private section:

```cpp
private:
    void applyAdc(uint8_t operand);
    void tickADCImmediate();
    void tickADCAbsolute();
```

In `src/cpu/cpu6502.cpp`, add the opcode constant next to `cOpADCImmediate`:

```cpp
const uint8_t cOpADCAbsolute = 0x6D;
```

Add the `case` to `tick()`'s switch:

```cpp
    switch (m_IR) {
    case cOpADCImmediate:
        tickADCImmediate();
        break;
    case cOpADCAbsolute:
        tickADCAbsolute();
        break;
    default:
        // TODO: remaining opcodes are not yet implemented; treat as a 1-cycle no-op.
        m_cycle = 0;
        break;
    }
```

Add the new method (after `tickADCImmediate()`):

```cpp
void CPU6502::tickADCAbsolute() {
    switch (m_cycle) {
    case 1:
        m_addrLatch = m_bus.read(m_PC);
        m_PC++;
        m_cycle = 2;
        break;
    case 2:
        m_addrLatch |= static_cast<uint16_t>(m_bus.read(m_PC)) << 8;
        m_PC++;
        m_cycle = 3;
        break;
    case 3: {
        uint8_t operand = m_bus.read(m_addrLatch);
        applyAdc(operand);
        m_cycle = 0;
        break;
    }
    default:
        // Unreachable: this handler is only invoked while m_cycle is 1, 2, or 3.
        m_cycle = 0;
        break;
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ./build/test/sys6_tests --gtest_filter=CPU6502AdcTest.*`
Expected: PASS, all 9 tests. Then run the full suite: `ctest --test-dir build --output-on-failure`.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: wire ADC absolute through the ALU unit"
```

---

## Task 5: Full verification pass

**Files:** none (no code changes expected; this task only runs verification and fixes anything it surfaces)

**Interfaces:** none — this task consumes the complete feature from Tasks 1-4 and confirms it end-to-end.

- [ ] **Step 1: Run the full test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS, all suites (`sys6_tests` covering ALU, CPU6502, RAM, ROM, Bus).

- [ ] **Step 2: Run format**

Run: `cmake --build build --target format`
Expected: exits cleanly. Check `git status`/`git diff` afterward — if `clang-format` changed anything, that's expected (this plan's code blocks were hand-formatted to match `.clang-format` but the tool is authoritative).

- [ ] **Step 3: Run lint**

Run: `cmake --build build --target lint`
Expected: reports findings to console (report-only, doesn't fail the build). Review output; if it flags something introduced by this plan (not pre-existing), fix it.

- [ ] **Step 4: Re-run tests if format/lint changed anything**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS. Only needed if Step 2 or Step 3 modified files.

- [ ] **Step 5: Commit if anything changed**

```bash
git add -A
git status
```

If `format` or lint fixes changed tracked files, commit them:

```bash
git commit -m "style: apply clang-format"
```

If nothing changed, skip this commit — Tasks 1-4 already captured all the work.
