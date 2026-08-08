# ALU function-select refactor — design spec

## Context

`ALU` (`src/cpu/alu.h`/`.cpp`) currently exposes one public method per 6502
mnemonic (`adc`, `sbc`, `bitwiseAnd`, `bitwiseOr`, `bitwiseXor`, `cmp`, `asl`,
`lsr`, `rol`, `ror`, `increment`, `decrement`), each returning an `AluResult`
with `value`, `carry`, `zero`, `overflow`, `negative`. `CPU6502` layers a
12-value `AluOp` enum on top and, every `tick()`, switches on `m_aluOp` to
call the matching `ALU` method — modeling the ALU as always-on combinational
hardware selected by a function-select line, per the existing design
philosophy documented in `docs/superpowers/specs/2026-08-08-cycle-stepped-alu-design.md`
and `docs/superpowers/specs/2026-08-08-remaining-alu-opcodes-design.md`.

This spec pushes that hardware model one step further: instead of exposing a
distinct method per mnemonic, `ALU` exposes a single entry point taking the
inputs a real ALU chip would have — two operands, a function-select line, and
carry-in — and producing the two outputs a real ALU chip would have: result
and carry-out. Higher-level distinctions (which 6502 mnemonic this is, which
flags it touches, whether the result gets written back) move entirely into
`CPU6502`, which already owns "the wiring" by design.

## Goals

- `ALU` exposes exactly one public method:
  `AluResult execute(uint8_t a, uint8_t b, AluFunction function, bool carryIn) const`.
- `AluFunction` is a small set of true combinational primitives —
  `ADD, AND, OR, XOR, SHL, SHR, ROL, ROR` (8 values) — with no 6502-specific
  mnemonics. `ALU` has no knowledge that `SBC` or `CMP` exist.
- `AluResult` shrinks to `{ uint8_t value; bool carry; }` (`D`/`CO`). `zero`,
  `overflow`, `negative` are removed from the struct.
- `CPU6502` derives `Z`/`N`/`V` generically from the ALU's actual output and
  input latches, rather than reading them off `AluResult`.
- `CPU6502`'s existing 12-value `AluOp` enum (`ADC, SBC, AND, ORA, EOR, CMP,
  ASL, LSR, ROL, ROR, INC, DEC`) is kept as a decode-time concept — the
  parameter type for `beginBinaryAluOp`/`beginUnaryAluOp` — but is no longer
  stored as a member or switched on inside `tick()`.
- No observable behavior change: every existing CPU-level opcode test keeps
  passing unchanged. This refactor only changes where logic lives, not what
  the 6502 does.

## Non-goals

- No new opcodes, addressing modes, or flag behavior.
- No change to `CPU6502`'s public API (`A()`, `X()`, flags, `tick()`,
  `executeInstruction()`, etc.).
- Decimal (BCD) mode remains out of scope, as in prior specs.
- No change to the capture/commit addressing-mode machinery
  (`captureReadAbsolute`, `commitRmwZeroPage`, etc.) beyond what
  `commitBinaryAluResult`/`commitUnaryAluFlags` need internally.

## Architecture

### `ALU`: single entry point, 8 primitives

`src/cpu/alu.h`:

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

`src/cpu/alu.cpp` — one switch, one case per primitive. `AND`/`OR`/`XOR`
combine `a` and `b`; `ADD` sums them with `carryIn`; the shift/rotate
primitives operate on `b` only (`a` is unused for those, matching the
existing "unary ops land in `m_aluB`" convention — see below), with `carryIn`
feeding the vacated bit for `ROL`/`ROR`:

```cpp
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

No SUB primitive: subtraction is add-with-inverted-operand, wired by the
caller (see below), matching real 6502 hardware where `SBC` and `CMP` reuse
the same adder as `ADC`.

### `CPU6502`: `AluOp` → `AluFunction` wiring

`AluOp` (`src/cpu/cpu6502.h`) is unchanged as a type — still `enum class
AluOp : uint8_t { ADC, SBC, AND, ORA, EOR, CMP, ASL, LSR, ROL, ROR, INC, DEC
};` — but is no longer stored. The member list changes from:

```cpp
ALU m_alu;
AluOp m_aluOp = AluOp::ADC;
uint8_t m_aluA = 0;
uint8_t m_aluB = 0;
bool m_aluCarryIn = false;
AluResult m_aluOutput{};
```

to:

```cpp
ALU m_alu;
uint8_t m_aluA = 0;
uint8_t m_aluB = 0;
AluFunction m_aluFunction = AluFunction::ADD;
bool m_aluCarryIn = false;
AluResult m_aluOutput{};
```

`beginBinaryAluOp`/`beginUnaryAluOp` still take an `AluOp` parameter (the
mnemonic being decoded) but now translate it into `AluFunction` plus operand
conditioning, instead of just latching it verbatim:

| `AluOp` | `AluFunction` | `B` conditioning | `CI` |
|---|---|---|---|
| `ADC` | `ADD` | operand | `CFlag()` |
| `SBC` | `ADD` | `~operand` | `CFlag()` |
| `CMP`/`CPX`/`CPY` (all map to `AluOp::CMP`) | `ADD` | `~operand` | `true` (forced — a compare never borrows in) |
| `AND`/`ORA`/`EOR` | `AND`/`OR`/`XOR` | operand | unused |
| `ASL`/`LSR` | `SHL`/`SHR` | value (unary convention: lands in `m_aluB`) | unused |
| `ROL`/`ROR` | `ROL`/`ROR` | value | `CFlag()` |
| `INC`/`DEC` | `ADD` | value (in `m_aluB`), `m_aluA := 0x01`/`0xFF` | `false` |

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

`applyBinaryAluOp`/`applyUnaryAluOp` (the `switch (m_IR)` dispatchers that
decide *which* `AluOp` and operands apply for a given opcode byte) are
unchanged — they still call `beginBinaryAluOp`/`beginUnaryAluOp` exactly as
today.

### `tick()`: one call instead of a 12-case switch

```cpp
void CPU6502::tick() {
    // The ALU is always-on combinational logic: it recomputes from whatever
    // is currently in its input latches on every tick, whether or not the
    // executing opcode is using the result this cycle. m_aluFunction is the
    // function-select line (F) choosing which combinational path drives D/CO.
    m_aluOutput = m_alu.execute(m_aluA, m_aluB, m_aluFunction, m_aluCarryIn);

    m_clockPhase = nextClockPhase(m_clockPhase);
    // ...unchanged clock-phase dispatch below
}
```

### Flag derivation moves into `CPU6502`

Three free functions, added to the existing anonymous namespace at the top
of `cpu6502.cpp` (alongside `nextClockPhase`):

```cpp
bool aluZero(uint8_t d) { return d == 0; }
bool aluNegative(uint8_t d) { return (d & 0x80) != 0; }
bool aluOverflow(uint8_t a, uint8_t b, uint8_t d) {
    return ((~(a ^ b)) & (a ^ d) & 0x80) != 0;
}
```

`aluOverflow` is the standard signed-overflow formula, applied to whatever
actually reached the ALU's `A`/`B` latches. For `SBC`, `B` is already
`~operand` by the time this runs; substituting into the formula algebraically
reduces to `(a ^ operand) & (a ^ d) & 0x80` — the textbook `SBC`-overflow
formula — so no `SBC`-specific case is needed. `commitBinaryAluResult()` and
`commitUnaryAluFlags()` call these in place of reading
`m_aluOutput.zero`/`.overflow`/`.negative`, e.g. the `ADC`/`SBC` branch of
`commitBinaryAluResult()` becomes:

```cpp
A(m_aluOutput.value);
CFlag(m_aluOutput.carry);
ZFlag(aluZero(m_aluOutput.value));
VFlag(aluOverflow(m_aluA, m_aluB, m_aluOutput.value));
NFlag(aluNegative(m_aluOutput.value));
```

and correspondingly for the `AND`/`ORA`/`EOR` branch (`Z`/`N` only), the
`CMP`/`CPX`/`CPY` branch (`C`/`Z`/`N`), and both branches of
`commitUnaryAluFlags()` and the tail of `commitImpliedIncDec()`.

## Compatibility

- `CPU6502`'s public API is unchanged.
- Every existing CPU-level opcode test (`cpu6502_adc_test.cpp`,
  `cpu6502_sbc_test.cpp`, `cpu6502_shift_test.cpp`, etc.) must keep passing
  **without assertion changes** — they are the regression backstop proving
  the wiring translation preserves observable behavior.
- `alu_test.cpp` requires a full rewrite: it currently tests
  `adc`/`sbc`/`bitwiseAnd`/etc. directly with `.zero`/`.overflow`/`.negative`
  assertions, none of which compile against the new `AluResult`/`ALU`
  shape. Tests get reorganized around the 8 `AluFunction` primitives — what
  were `adc`'s carry/overflow/boundary cases become `execute(..., ADD, ...)`
  cases; `SBC`/`CMP`-specific behavior (the operand inversion, `CMP`'s forced
  carry-in) is no longer expressible as an `ALU`-level test since that logic
  now lives in `CPU6502::beginBinaryAluOp` — that behavior is instead covered
  by the existing CPU-level `cpu6502_sbc_test.cpp`/`cpu6502_cmp_test.cpp`.

## Testing plan

- Rewrite `test/cpu/alu_test.cpp`: one `TEST(ALUTest, ...)` group per
  `AluFunction`, preserving the existing rigor (normal case, zero-result,
  negative/sign-bit, carry/overflow boundary for `ADD`; carry-out and
  zero/negative-adjacent boundary cases for the shift/rotate primitives; a
  carry-in-propagation case for `ROL`/`ROR`).
- Run the full existing CPU-level test suite unchanged and confirm 100%
  green — this is the primary correctness signal for the refactor, not new
  tests.
- No new CPU-level test files; this is a pure refactor of existing, already
  covered behavior.

## Follow-on work (not this pass)

- A metadata-table-driven opcode dispatcher (already deferred by the prior
  spec) is unaffected by this change and remains future work.
- Decimal (BCD) mode remains out of scope, as in prior specs.
