# ALU Function-Select Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Collapse `ALU`'s twelve per-mnemonic public methods into a single `execute(A, B, F, CI) -> (D, CO)` entry point, moving 6502-mnemonic-specific operand wiring and flag derivation into `CPU6502`, with zero observable behavior change.

**Architecture:** `ALU` becomes a pure 4-input/2-output combinational function over 8 primitives (`ADD, AND, OR, XOR, SHL, SHR, ROL, ROR`). `CPU6502` keeps its existing 12-value `AluOp` enum as a decode-time concept, translating each mnemonic into an `AluFunction` plus operand conditioning (e.g. `SBC`/`CMP` invert `B` before the shared adder), and derives `Z`/`N`/`V` itself from the ALU's actual input/output latches instead of reading them off the result struct.

**Tech Stack:** C++17, CMake, GoogleTest/CTest.

## Global Constraints

- No change to `CPU6502`'s public API.
- Every existing CPU-level opcode test must pass **unchanged** (no assertion edits) — this is the regression backstop for the whole refactor.
- Full spec: `docs/superpowers/specs/2026-08-08-alu-function-select-refactor-design.md`.
- Build: `cmake --build build`. Test: `ctest --test-dir build --output-on-failure`. Baseline today: 219/219 passing.

---

### Task 1: Add `ALU::execute()` and `AluFunction` alongside the existing methods

Purely additive — the existing per-mnemonic methods (`adc`, `sbc`, `bitwiseAnd`, etc.) and the existing `AluResult` fields (`zero`, `overflow`, `negative`) stay untouched in this task. Nothing else in the codebase calls `execute()` yet, so this task cannot regress anything.

**Files:**
- Modify: `src/cpu/alu.h`
- Modify: `src/cpu/alu.cpp`
- Modify: `test/cpu/alu_test.cpp`

**Interfaces:**
- Produces: `enum class AluFunction : uint8_t { ADD, AND, OR, XOR, SHL, SHR, ROL, ROR };` and `AluResult ALU::execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const` — both consumed by Task 2.

- [ ] **Step 1: Write the new tests**

Append to the end of `test/cpu/alu_test.cpp` (after the existing `DecrementSetsZeroFlag` test, before EOF):

```cpp
TEST(ALUTest, ExecuteAddAddsTwoValuesWithNoCarryIn) {
    ALU alu;
    AluResult result = alu.execute(0x10, 0x05, AluFunction::ADD, false);

    EXPECT_EQ(result.value, 0x15);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteAddIncludesCarryInWhenSet) {
    ALU alu;
    AluResult result = alu.execute(0x10, 0x05, AluFunction::ADD, true);

    EXPECT_EQ(result.value, 0x16);
}

TEST(ALUTest, ExecuteAddSetsCarryOnUnsignedOverflowToZero) {
    ALU alu;
    AluResult result = alu.execute(0xFF, 0x01, AluFunction::ADD, false);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteAndCombinesOperands) {
    ALU alu;
    AluResult result = alu.execute(0xF0, 0x3C, AluFunction::AND, false);

    EXPECT_EQ(result.value, 0x30);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteOrCombinesOperands) {
    ALU alu;
    AluResult result = alu.execute(0xF0, 0x0C, AluFunction::OR, false);

    EXPECT_EQ(result.value, 0xFC);
}

TEST(ALUTest, ExecuteXorCombinesOperands) {
    ALU alu;
    AluResult result = alu.execute(0xFF, 0x0F, AluFunction::XOR, false);

    EXPECT_EQ(result.value, 0xF0);
}

TEST(ALUTest, ExecuteShlShiftsLeftAndCarriesOutBit7OfB) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x81, AluFunction::SHL, false);

    EXPECT_EQ(result.value, 0x02);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteShlIgnoresA) {
    ALU alu;
    AluResult result = alu.execute(0xFF, 0x40, AluFunction::SHL, false);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteShrShiftsRightAndCarriesOutBit0OfB) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x01, AluFunction::SHR, false);

    EXPECT_EQ(result.value, 0x00);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteRolBringsInCarryAtBit0AndCarriesOutBit7) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x80, AluFunction::ROL, true);

    EXPECT_EQ(result.value, 0x01);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteRolClearsBit0WhenCarryInFalse) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x01, AluFunction::ROL, false);

    EXPECT_EQ(result.value, 0x02);
    EXPECT_FALSE(result.carry);
}

TEST(ALUTest, ExecuteRorBringsInCarryAtBit7AndCarriesOutBit0) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x01, AluFunction::ROR, true);

    EXPECT_EQ(result.value, 0x80);
    EXPECT_TRUE(result.carry);
}

TEST(ALUTest, ExecuteRorClearsBit7WhenCarryInFalse) {
    ALU alu;
    AluResult result = alu.execute(0x00, 0x02, AluFunction::ROR, false);

    EXPECT_EQ(result.value, 0x01);
    EXPECT_FALSE(result.carry);
}
```

- [ ] **Step 2: Run tests to verify they fail (compile error)**

Run: `cmake --build build`
Expected: FAIL — compile error in `test/cpu/alu_test.cpp`, `AluFunction` and `ALU::execute` are undeclared.

- [ ] **Step 3: Add `AluFunction` and declare `execute()` in `alu.h`**

Replace the full contents of `src/cpu/alu.h` with:

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

enum class AluFunction : uint8_t { ADD, AND, OR, XOR, SHL, SHR, ROL, ROR };

class ALU {
public:
    [[nodiscard]] AluResult adc(uint8_t acc, uint8_t operand, bool carryIn) const;
    [[nodiscard]] AluResult sbc(uint8_t acc, uint8_t operand, bool carryIn) const;
    [[nodiscard]] AluResult bitwiseAnd(uint8_t acc, uint8_t operand) const;
    [[nodiscard]] AluResult bitwiseOr(uint8_t acc, uint8_t operand) const;
    [[nodiscard]] AluResult bitwiseXor(uint8_t acc, uint8_t operand) const;
    [[nodiscard]] AluResult cmp(uint8_t reg, uint8_t operand) const;
    [[nodiscard]] AluResult asl(uint8_t value) const;
    [[nodiscard]] AluResult lsr(uint8_t value) const;
    [[nodiscard]] AluResult rol(uint8_t value, bool carryIn) const;
    [[nodiscard]] AluResult ror(uint8_t value, bool carryIn) const;
    [[nodiscard]] AluResult increment(uint8_t value) const;
    [[nodiscard]] AluResult decrement(uint8_t value) const;

    [[nodiscard]] AluResult execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const;
};
```

(Only the `AluFunction` enum and the `execute()` declaration are new; everything else in the file is unchanged from its current content.)

- [ ] **Step 4: Implement `execute()` in `alu.cpp`**

Add this method definition to `src/cpu/alu.cpp`, after the existing `decrement` definition (i.e. at the end of the file):

```cpp
AluResult ALU::execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const {
    switch (function) {
    case AluFunction::ADD: {
        int sum = static_cast<int>(a) + static_cast<int>(b) + (carryIn ? 1 : 0);
        return AluResult{static_cast<uint8_t>(sum & 0xFF), sum > 0xFF, false, false, false};
    }
    case AluFunction::AND:
        return AluResult{static_cast<uint8_t>(a & b), false, false, false, false};
    case AluFunction::OR:
        return AluResult{static_cast<uint8_t>(a | b), false, false, false, false};
    case AluFunction::XOR:
        return AluResult{static_cast<uint8_t>(a ^ b), false, false, false, false};
    case AluFunction::SHL:
        return AluResult{static_cast<uint8_t>(b << 1), (b & 0x80) != 0, false, false, false};
    case AluFunction::SHR:
        return AluResult{static_cast<uint8_t>(b >> 1), (b & 0x01) != 0, false, false, false};
    case AluFunction::ROL:
        return AluResult{static_cast<uint8_t>((b << 1) | (carryIn ? 0x01 : 0x00)), (b & 0x80) != 0, false, false,
                          false};
    case AluFunction::ROR:
        return AluResult{static_cast<uint8_t>((b >> 1) | (carryIn ? 0x80 : 0x00)), (b & 0x01) != 0, false, false,
                          false};
    }
    return AluResult{0, false, false, false, false}; // unreachable: all enumerators handled above
}
```

Note: `AluResult` still has 5 fields at this point in the plan (Task 3 shrinks it to 2), so `execute()`'s returns fill `zero`/`overflow`/`negative` with `false` placeholders that nothing reads yet — this is temporary and removed in Task 3.

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R ALUTest`
Expected: PASS — all `ALUTest.*` tests pass, including the 13 new `Execute*` ones (50 total: 37 existing + 13 new).

- [ ] **Step 6: Run the full suite to confirm no regressions**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 232` (baseline 219 + 13 new).

- [ ] **Step 7: Commit**

```bash
git add src/cpu/alu.h src/cpu/alu.cpp test/cpu/alu_test.cpp
git commit -m "Add ALU::execute() function-select entry point alongside existing methods"
```

---

### Task 2: Migrate `CPU6502` to drive the ALU through `execute()`

Rewires `tick()`, `beginBinaryAluOp`, `beginUnaryAluOp`, and the three commit functions to use `m_alu.execute(...)` and CPU-side flag derivation instead of the per-mnemonic methods and `AluResult`'s `zero`/`overflow`/`negative` fields. After this task, `ALU`'s old per-mnemonic methods are unused dead code (removed in Task 3) but still present, so the build stays green throughout.

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`

**Interfaces:**
- Consumes: `AluFunction` enum and `ALU::execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const` from Task 1.
- Produces: no new public interface — `CPU6502`'s public API is unchanged. This task's correctness is verified entirely by the existing CPU-level test suite.

- [ ] **Step 1: Update the ALU-related members in `cpu6502.h`**

In `src/cpu/cpu6502.h`, replace lines 89-98:

```cpp
    // The ALU is a combinational unit: it has no state of its own, but real
    // hardware wires its inputs and output through latches rather than
    // passing them as ephemeral call arguments. These members are that
    // wiring, made explicit.
    ALU m_alu;
    AluOp m_aluOp = AluOp::ADC;
    uint8_t m_aluA = 0;
    uint8_t m_aluB = 0;
    bool m_aluCarryIn = false;
    AluResult m_aluOutput{};
```

with:

```cpp
    // The ALU is a combinational unit: it has no state of its own, but real
    // hardware wires its inputs and output through latches rather than
    // passing them as ephemeral call arguments. These members are that
    // wiring, made explicit: A/B are the operand latches, m_aluFunction is
    // the function-select line (F), m_aluCarryIn is CI, and m_aluOutput
    // holds D/CO.
    ALU m_alu;
    uint8_t m_aluA = 0;
    uint8_t m_aluB = 0;
    AluFunction m_aluFunction = AluFunction::ADD;
    bool m_aluCarryIn = false;
    AluResult m_aluOutput{};
```

`enum class AluOp : uint8_t { ADC, SBC, AND, ORA, EOR, CMP, ASL, LSR, ROL, ROR, INC, DEC };` at line 13 is unchanged — it's still the parameter type for `beginBinaryAluOp`/`beginUnaryAluOp`, just no longer stored as a member.

- [ ] **Step 2: Add flag-derivation free functions to `cpu6502.cpp`**

In `src/cpu/cpu6502.cpp`, find the anonymous namespace containing `nextClockPhase` (immediately before `void CPU6502::tick()`). Add these three functions inside that same anonymous namespace, before the closing `} // namespace`:

```cpp
bool aluZero(uint8_t d) { return d == 0; }
bool aluNegative(uint8_t d) { return (d & 0x80) != 0; }
bool aluOverflow(uint8_t a, uint8_t b, uint8_t d) { return ((~(a ^ b)) & (a ^ d) & 0x80) != 0; }
```

- [ ] **Step 3: Replace `tick()`'s 12-case switch with a single `execute()` call**

In `src/cpu/cpu6502.cpp`, replace lines 216-258:

```cpp
void CPU6502::tick() {
    // The ALU is always-on combinational logic: it recomputes from whatever
    // is currently in its input latches on every tick, whether or not the
    // executing opcode is using the result this cycle. m_aluOp is the
    // function-select line choosing which operation that recompute performs.
    switch (m_aluOp) {
    case AluOp::ADC:
        m_aluOutput = m_alu.adc(m_aluA, m_aluB, m_aluCarryIn);
        break;
    case AluOp::SBC:
        m_aluOutput = m_alu.sbc(m_aluA, m_aluB, m_aluCarryIn);
        break;
    case AluOp::AND:
        m_aluOutput = m_alu.bitwiseAnd(m_aluA, m_aluB);
        break;
    case AluOp::ORA:
        m_aluOutput = m_alu.bitwiseOr(m_aluA, m_aluB);
        break;
    case AluOp::EOR:
        m_aluOutput = m_alu.bitwiseXor(m_aluA, m_aluB);
        break;
    case AluOp::CMP:
        m_aluOutput = m_alu.cmp(m_aluA, m_aluB);
        break;
    case AluOp::ASL:
        m_aluOutput = m_alu.asl(m_aluB);
        break;
    case AluOp::LSR:
        m_aluOutput = m_alu.lsr(m_aluB);
        break;
    case AluOp::ROL:
        m_aluOutput = m_alu.rol(m_aluB, m_aluCarryIn);
        break;
    case AluOp::ROR:
        m_aluOutput = m_alu.ror(m_aluB, m_aluCarryIn);
        break;
    case AluOp::INC:
        m_aluOutput = m_alu.increment(m_aluB);
        break;
    case AluOp::DEC:
        m_aluOutput = m_alu.decrement(m_aluB);
        break;
    }

    m_clockPhase = nextClockPhase(m_clockPhase);
```

with:

```cpp
void CPU6502::tick() {
    // The ALU is always-on combinational logic: it recomputes from whatever
    // is currently in its input latches on every tick, whether or not the
    // executing opcode is using the result this cycle. m_aluFunction is the
    // function-select line (F) choosing which combinational path drives D/CO.
    m_aluOutput = m_alu.execute(m_aluA, m_aluB, m_aluFunction, m_aluCarryIn);

    m_clockPhase = nextClockPhase(m_clockPhase);
```

(The rest of `tick()` — the clock-phase switch below — is unchanged.)

- [ ] **Step 4: Rewrite `beginBinaryAluOp`**

Replace (originally at lines 555-560):

```cpp
void CPU6502::beginBinaryAluOp(AluOp aluOp, uint8_t regValue, uint8_t operand) {
    m_aluOp = aluOp;
    m_aluA = regValue;
    m_aluB = operand;
    m_aluCarryIn = CFlag();
}
```

with:

```cpp
void CPU6502::beginBinaryAluOp(AluOp aluOp, uint8_t regValue, uint8_t operand) {
    m_aluA = regValue;
    m_aluB = operand;
    m_aluCarryIn = CFlag();
    switch (aluOp) {
    case AluOp::ADC:
        m_aluFunction = AluFunction::ADD;
        break;
    case AluOp::SBC:
        m_aluFunction = AluFunction::ADD;
        m_aluB = static_cast<uint8_t>(~operand);
        break;
    case AluOp::AND:
        m_aluFunction = AluFunction::AND;
        break;
    case AluOp::ORA:
        m_aluFunction = AluFunction::OR;
        break;
    case AluOp::EOR:
        m_aluFunction = AluFunction::XOR;
        break;
    case AluOp::CMP:
        m_aluFunction = AluFunction::ADD;
        m_aluB = static_cast<uint8_t>(~operand);
        m_aluCarryIn = true; // compare never borrows in
        break;
    default:
        break; // unreachable: only binary ops routed here
    }
}
```

- [ ] **Step 5: Rewrite `beginUnaryAluOp`**

Replace (originally at lines 1033-1037):

```cpp
void CPU6502::beginUnaryAluOp(AluOp aluOp, uint8_t value) {
    m_aluOp = aluOp;
    m_aluB = value;
    m_aluCarryIn = CFlag();
}
```

with:

```cpp
void CPU6502::beginUnaryAluOp(AluOp aluOp, uint8_t value) {
    m_aluB = value;
    switch (aluOp) {
    case AluOp::ASL:
        m_aluFunction = AluFunction::SHL;
        m_aluCarryIn = false; // unused by SHL
        break;
    case AluOp::LSR:
        m_aluFunction = AluFunction::SHR;
        m_aluCarryIn = false; // unused by SHR
        break;
    case AluOp::ROL:
        m_aluFunction = AluFunction::ROL;
        m_aluCarryIn = CFlag();
        break;
    case AluOp::ROR:
        m_aluFunction = AluFunction::ROR;
        m_aluCarryIn = CFlag();
        break;
    case AluOp::INC:
        m_aluFunction = AluFunction::ADD;
        m_aluA = 0x01;
        m_aluCarryIn = false;
        break;
    case AluOp::DEC:
        m_aluFunction = AluFunction::ADD;
        m_aluA = 0xFF;
        m_aluCarryIn = false;
        break;
    default:
        break; // unreachable: only unary ops routed here
    }
}
```

- [ ] **Step 6: Update `commitBinaryAluResult` to derive flags generically**

In `src/cpu/cpu6502.cpp`, within `commitBinaryAluResult()` (originally lines 639-716), replace the `ADC`/`SBC` branch body:

```cpp
        // ADC/SBC: write A; C, Z, V, N all follow the arithmetic result.
        A(m_aluOutput.value);
        CFlag(m_aluOutput.carry);
        ZFlag(m_aluOutput.zero);
        VFlag(m_aluOutput.overflow);
        NFlag(m_aluOutput.negative);
        break;
```

with:

```cpp
        // ADC/SBC: write A; C, Z, V, N all follow the arithmetic result.
        A(m_aluOutput.value);
        CFlag(m_aluOutput.carry);
        ZFlag(aluZero(m_aluOutput.value));
        VFlag(aluOverflow(m_aluA, m_aluB, m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
```

Replace the `AND`/`ORA`/`EOR` branch body:

```cpp
        // AND/ORA/EOR: write A; only Z, N follow. C and V are real 6502
        // behavior left untouched by logical ops, not an oversight.
        A(m_aluOutput.value);
        ZFlag(m_aluOutput.zero);
        NFlag(m_aluOutput.negative);
        break;
```

with:

```cpp
        // AND/ORA/EOR: write A; only Z, N follow. C and V are real 6502
        // behavior left untouched by logical ops, not an oversight.
        A(m_aluOutput.value);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
```

Replace the `CMP`/`CPX`/`CPY` branch body:

```cpp
        // CMP/CPX/CPY: write nothing back; only C, Z, N follow.
        CFlag(m_aluOutput.carry);
        ZFlag(m_aluOutput.zero);
        NFlag(m_aluOutput.negative);
        break;
```

with:

```cpp
        // CMP/CPX/CPY: write nothing back; only C, Z, N follow.
        CFlag(m_aluOutput.carry);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
```

- [ ] **Step 7: Update `commitUnaryAluFlags` to derive flags generically**

In `src/cpu/cpu6502.cpp`, within `commitUnaryAluFlags()` (originally lines 1086-1129), replace the shift/rotate branch body:

```cpp
        // ASL/LSR/ROL/ROR: C, Z, N follow the shift result. V untouched.
        CFlag(m_aluOutput.carry);
        ZFlag(m_aluOutput.zero);
        NFlag(m_aluOutput.negative);
        break;
```

with:

```cpp
        // ASL/LSR/ROL/ROR: C, Z, N follow the shift result. V untouched.
        CFlag(m_aluOutput.carry);
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
```

Replace the `INC`/`DEC` branch body:

```cpp
        // INC/DEC: only Z, N follow. C and V are real 6502 behavior left
        // untouched, not an oversight.
        ZFlag(m_aluOutput.zero);
        NFlag(m_aluOutput.negative);
        break;
```

with:

```cpp
        // INC/DEC: only Z, N follow. C and V are real 6502 behavior left
        // untouched, not an oversight.
        ZFlag(aluZero(m_aluOutput.value));
        NFlag(aluNegative(m_aluOutput.value));
        break;
```

- [ ] **Step 8: Update `commitImpliedIncDec`'s flag tail**

In `src/cpu/cpu6502.cpp`, within `commitImpliedIncDec()` (originally lines 1322-1357), replace the unconditional tail:

```cpp
    ZFlag(m_aluOutput.zero);
    NFlag(m_aluOutput.negative);
    m_cpuStep = CpuStep::T0;
```

with:

```cpp
    ZFlag(aluZero(m_aluOutput.value));
    NFlag(aluNegative(m_aluOutput.value));
    m_cpuStep = CpuStep::T0;
```

- [ ] **Step 9: Build and run the full test suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 232` — same 232 tests as after Task 1, **zero assertion changes** in any CPU-level test file. If any CPU-level test fails, that's a wiring bug in Steps 4-8 (most likely a missed `B` inversion or a forgotten `CI` override for `CMP`) — fix it before proceeding; do not edit the test's assertions to match.

- [ ] **Step 10: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp
git commit -m "Migrate CPU6502 to drive the ALU through execute(A, B, F, CI)"
```

---

### Task 3: Remove the old per-mnemonic `ALU` methods and shrink `AluResult`

Deletes now-dead code: the 12 old `ALU` methods, `AluResult`'s `zero`/`overflow`/`negative` fields, and the old-style tests in `alu_test.cpp` that exercise them (they no longer compile once the methods are gone).

**Files:**
- Modify: `src/cpu/alu.h`
- Modify: `src/cpu/alu.cpp`
- Modify: `test/cpu/alu_test.cpp`

**Interfaces:**
- Consumes: nothing new — this task only removes code that Tasks 1-2 made unreachable.
- Produces: final `AluResult { uint8_t value; bool carry; }` and `ALU` with only `execute()` — this is the shape the design spec describes as the end state.

- [ ] **Step 1: Shrink `AluResult` and remove the old methods from `alu.h`**

Replace the full contents of `src/cpu/alu.h` with:

```cpp
#pragma once

#include <cstdint>

struct AluResult {
    uint8_t value; // D
    bool carry;    // CO
};

enum class AluFunction : uint8_t { ADD, AND, OR, XOR, SHL, SHR, ROL, ROR };

class ALU {
public:
    [[nodiscard]] AluResult execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const;
};
```

- [ ] **Step 2: Remove the old methods and simplify `execute()`'s returns in `alu.cpp`**

Replace the full contents of `src/cpu/alu.cpp` with:

```cpp
#include "alu.h"

AluResult ALU::execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const {
    switch (function) {
    case AluFunction::ADD: {
        int sum = static_cast<int>(a) + static_cast<int>(b) + (carryIn ? 1 : 0);
        return AluResult{static_cast<uint8_t>(sum & 0xFF), sum > 0xFF};
    }
    case AluFunction::AND:
        return AluResult{static_cast<uint8_t>(a & b), false};
    case AluFunction::OR:
        return AluResult{static_cast<uint8_t>(a | b), false};
    case AluFunction::XOR:
        return AluResult{static_cast<uint8_t>(a ^ b), false};
    case AluFunction::SHL:
        return AluResult{static_cast<uint8_t>(b << 1), (b & 0x80) != 0};
    case AluFunction::SHR:
        return AluResult{static_cast<uint8_t>(b >> 1), (b & 0x01) != 0};
    case AluFunction::ROL:
        return AluResult{static_cast<uint8_t>((b << 1) | (carryIn ? 0x01 : 0x00)), (b & 0x80) != 0};
    case AluFunction::ROR:
        return AluResult{static_cast<uint8_t>((b >> 1) | (carryIn ? 0x80 : 0x00)), (b & 0x01) != 0};
    }
    return AluResult{0, false}; // unreachable: all enumerators handled above
}
```

- [ ] **Step 3: Remove the old-style tests from `alu_test.cpp`**

Delete every `TEST(ALUTest, ...)` block in `test/cpu/alu_test.cpp` that calls one of the removed methods (`adc`, `sbc`, `bitwiseAnd`, `bitwiseOr`, `bitwiseXor`, `cmp`, `asl`, `lsr`, `rol`, `ror`, `increment`, `decrement`) or reads `.zero`/`.overflow`/`.negative`. Concretely, delete these 37 tests (everything between the `#include` block and the first `Execute*` test added in Task 1):

`AdcAddsTwoValuesWithNoCarryIn`, `AdcAddsCarryInWhenSet`, `AdcSetsCarryAndZeroOnUnsignedOverflowToZero`, `AdcClearsZeroFlagWhenResultIsNonZero`, `AdcSetsNegativeFlagWhenBit7OfResultIsSet`, `AdcSetsOverflowWhenTwoPositivesOverflowToNegative`, `AdcClearsOverflowWhenOperandsHaveDifferentSigns`, `AdcSetsOverflowWhenTwoNegativesOverflowToPositive`, `SbcSubtractsOperandWhenCarryInSet`, `SbcBorrowsWhenCarryInClear`, `SbcClearsCarryOnUnsignedUnderflow`, `SbcSetsOverflowOnSignedUnderflow`, `BitwiseAndCombinesOperandsAndIgnoresCarry`, `BitwiseAndSetsZeroWhenNoBitsOverlap`, `BitwiseAndSetsNegativeWhenBit7Survives`, `BitwiseOrCombinesOperands`, `BitwiseOrSetsZeroWhenBothOperandsAreZero`, `BitwiseXorCombinesOperands`, `BitwiseXorSetsZeroWhenOperandsMatch`, `CmpSetsCarryWhenRegGreaterThanOperand`, `CmpSetsZeroAndCarryWhenEqual`, `CmpClearsCarryWhenRegLessThanOperand`, `AslShiftsLeftAndCarriesOutBit7`, `AslSetsZeroWhenResultIsZero`, `AslSetsNegativeWhenBit7OfResultIsSet`, `RolShiftsLeftAndBringsInCarry`, `RolCarriesOutBit7`, `LsrShiftsRightAndCarriesOutBit0`, `LsrNeverSetsNegative`, `RorShiftsRightAndBringsInCarryAtBit7`, `RorCarriesOutBit0`, `IncrementAddsOne`, `IncrementWrapsToZeroAndSetsZeroFlag`, `IncrementSetsNegativeOnSignedOverflow`, `DecrementSubtractsOne`, `DecrementWrapsBelowZero`, `DecrementSetsZeroFlag`.

After deletion, `test/cpu/alu_test.cpp` should contain only the `#include` block followed directly by the 13 `Execute*` tests added in Task 1, with no old-style test remaining (verify with `grep -c "^TEST(ALUTest" test/cpu/alu_test.cpp`, expected: `13`).

- [ ] **Step 4: Build and run the full test suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 195` (232 minus the 37 deleted tests). No CPU-level test file changes since Task 2 — this step is the final confirmation that removing the dead ALU surface didn't break anything.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/alu.h src/cpu/alu.cpp test/cpu/alu_test.cpp
git commit -m "Remove ALU's old per-mnemonic methods now that CPU6502 uses execute()"
```

---

## Self-Review Notes

- **Spec coverage:** every item in the design spec's Goals section maps to a task — `execute()`/`AluFunction`/shrunk `AluResult` (Tasks 1 & 3), `AluOp`→`AluFunction` wiring table (Task 2 Steps 4-5), CPU-side flag derivation (Task 2 Steps 2, 6-8), no `CPU6502` public API change and full regression suite green (Task 2 Step 9, Task 3 Step 4).
- **Placeholder scan:** no TBD/TODO; every step has literal code or an exact command plus expected output.
- **Type consistency:** `AluFunction`, `AluResult{value, carry}`, and `ALU::execute(uint8_t, uint8_t, AluFunction, bool)` are introduced in Task 1 and used identically (same names, same order) in Tasks 2 and 3. `m_aluFunction`/`m_aluA`/`m_aluB`/`m_aluCarryIn`/`m_aluOutput` introduced in Task 2 Step 1 are the only members Steps 3-8 reference.
