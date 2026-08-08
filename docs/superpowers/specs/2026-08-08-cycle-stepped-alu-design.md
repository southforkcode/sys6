# Cycle-stepped CPU core with an ALU unit — design spec

## Context

The next step after the memory/bus subsystem was originally framed as "implement
the basic ALU operations of the 6502." During design, that goal turned out to
imply a much larger architectural decision: `CPU6502::executeInstruction()`
currently does an entire instruction (opcode fetch + PC advance) in one call,
with no internal notion of clock cycles, internal registers/latches, or
separately organized functional units. Real 6502 hardware executes every
instruction over multiple clock cycles, routing data between internal
registers and a physically distinct ALU over internal buses.

This spec covers making `CPU6502` a true cycle-stepped state machine and
introducing `ALU` as its first internal unit, proven out with two real
opcodes: `ADC #immediate` and `ADC absolute`. It intentionally does **not**
cover the rest of the ALU-shaped instruction set (SBC, AND, ORA, EOR, shifts,
compares, INC/DEC, ...) or other addressing modes — those follow the same
pattern established here as fast-follow work, once the architecture is
proven.

## Goals

- `CPU6502` advances one clock cycle at a time via a new `tick()` method,
  matching how the real hardware works.
- ALU computation is organized as its own unit (`ALU` class), decoupled from
  CPU register/bus access — pure input-in, result-out.
- `ADC #immediate` (2 cycles) and `ADC absolute` (4 cycles) work end-to-end
  through this architecture with cycle-accurate timing.
- Existing public API and tests keep working: `executeInstruction()` still
  runs a full instruction per call.

## Non-goals

- Decimal (BCD) mode for ADC — binary mode only. `DFlag()` is not consulted.
- Any ALU op other than ADC, and any addressing mode other than immediate
  and absolute.
- Full addressing-mode matrix, opcode dispatch table, or the remaining
  ALU-shaped instructions (SBC, AND, ORA, EOR, ASL, LSR, ROL, ROR, CMP, CPX,
  CPY, INC, DEC, INX, DEX, INY, DEY). These are separate follow-on phases.
- Cycle-accurate "dummy read" cycles (e.g. page-crossing penalties). Deferred
  until an opcode that actually needs one is implemented.
- Interrupt handling, mid-instruction interruption, or any change to
  `reset()`.

## Architecture

### `tick()` and instruction-level state

`CPU6502` gains:

```cpp
void tick(); // advances exactly one clock cycle
```

Internal state (new protected members on `CPU6502`):

- `uint8_t m_cycle` — which cycle of the current instruction is executing.
  `m_cycle == 0` means "ready to fetch the next opcode," and is also the
  sentinel `executeInstruction()` uses to know the instruction is complete.
- `uint8_t m_IR` — instruction register: the opcode currently executing,
  latched at cycle 0.
- `uint8_t m_dataLatch` — last byte read from the bus.
- `uint16_t m_addrLatch` — effective address assembled across cycles. This
  simplifies the real chip's separate ADL/ADH latches into one field; that
  simplification is fine as long as nothing outside `CPU6502` depends on the
  low/high bytes being separately observable.

`tick()` behavior:

1. If `m_cycle == 0`: fetch the byte at `PC` into `m_IR`, increment `PC`, set
   `m_cycle = 1`, return. This step is identical for every opcode — T0 of
   every 6502 instruction is an opcode fetch.
2. Otherwise: dispatch on `m_IR` to an opcode-specific handler, which
   switches on `m_cycle` and does the right thing for that cycle, advancing
   `m_cycle` each call. On the instruction's final cycle, the handler sets
   `m_cycle = 0` to signal completion.
3. If `m_IR` doesn't match an implemented opcode: fall to a `default` case
   that immediately sets `m_cycle = 0` (a 1-cycle no-op), marked with a
   `// TODO` comment. This keeps existing tests that use arbitrary opcode
   bytes (e.g. `0x42`) passing unchanged — see "Compatibility" below.

`executeInstruction()` becomes:

```cpp
void CPU6502::executeInstruction() {
    do {
        tick();
    } while (m_cycle != 0);
}
```

It still completes a full instruction (including the opcode fetch) in one
call, so all existing callers (`main.cpp`, existing tests) are unaffected.

### Bus access policy

A cycle only touches the bus when there's real work to do for that cycle.
Real hardware sometimes issues "dummy" bus reads purely for timing (e.g. the
extra cycle on page-crossing indexed addressing) — neither opcode in this
pass needs one, so that pattern isn't introduced yet. When a future opcode
needs it, it can be added to that opcode's handler without changing this
architecture.

### `ALU` unit

New files `src/cpu/alu.h` / `src/cpu/alu.cpp`:

```cpp
struct AluResult {
    uint8_t value;
    bool carry;
    bool zero;
    bool overflow;
    bool negative;
};

class ALU {
public:
    AluResult adc(uint8_t a, uint8_t operand, bool carryIn);
};
```

`ALU` has no knowledge of `CPU6502`, registers, or the bus — it's a pure
computation unit. `CPU6502` owns one as a member (`ALU m_alu;`) and, on the
cycle where an ALU operation is needed, calls it and applies the result to
its own registers/flags:

```cpp
AluResult r = m_alu.adc(m_A, operand, CFlag());
A(r.value);
CFlag(r.carry);
ZFlag(r.zero);
VFlag(r.overflow);
NFlag(r.negative);
```

`adc` flag semantics (binary mode only):
- `sum = a + operand + (carryIn ? 1 : 0)` computed in a wider integer type.
- `value = sum & 0xFF`.
- `carry = sum > 0xFF`.
- `overflow = (~(a ^ operand) & (a ^ value) & 0x80) != 0` — standard signed
  overflow check, using `a` (the pre-op accumulator) and `operand`, not the
  post-op accumulator.
- `zero = value == 0`.
- `negative = (value & 0x80) != 0`.

### Opcode cycle tables

**`ADC #immediate` (`0x69`, 2 cycles total)**

| Cycle | Action |
|---|---|
| 1 (T0) | generic opcode fetch (see above) |
| 2 (T1) | fetch operand byte at `PC`, `PC++`; run `ALU.adc(A, operand, C)`; apply result to `A`/flags; `m_cycle = 0` |

**`ADC absolute` (`0x6D`, 4 cycles total)**

| Cycle | Action |
|---|---|
| 1 (T0) | generic opcode fetch |
| 2 (T1) | fetch address low byte at `PC`, `PC++`; store into `m_addrLatch` low byte |
| 3 (T2) | fetch address high byte at `PC`, `PC++`; combine into `m_addrLatch` |
| 4 (T3) | read operand byte from `m_addrLatch` via bus; run `ALU.adc(A, operand, C)`; apply result to `A`/flags; `m_cycle = 0` |

Both cycle counts match real 6502 timing for these opcode/addressing-mode
combinations.

## Compatibility

Existing tests in `test/cpu/cpu6502_test.cpp` that call `executeInstruction()`
with arbitrary opcode bytes (`ExecuteInstructionFetchesOpcodeAndAdvancesPC`,
`ExecuteInstructionReadsFromCurrentPC`) continue to pass unchanged: those
opcodes (`0x42`, `0x99`) fall to the unimplemented-opcode `default` case,
which behaves as a 1-cycle no-op, so `executeInstruction()`'s loop still
exits after fetch and `PC` still advances by exactly 1.

## Testing plan

Two new test files:

- `test/cpu/alu_test.cpp` — direct unit tests of `ALU::adc`, no `CPU6502`
  involved: normal addition, carry-out on overflow past `0xFF`, zero flag,
  negative flag, signed overflow (e.g. `0x7F + 1`), and carry-in
  propagation (e.g. `0x00 + 0x00` with `carryIn = true` produces `1`, not
  `0`).
- `test/cpu/cpu6502_adc_test.cpp` — cycle-level tests calling `tick()`
  directly: after cycle 1, `PC` has advanced by 1 and the internal `m_IR`
  holds the opcode (verified indirectly through behavior, not by exposing
  `m_IR`); for absolute mode, intermediate cycles 2 and 3 leave `A`
  unchanged; after the final cycle of each opcode, `A` and flags reflect the
  ALU result and the instruction is complete (verified by confirming the
  next `tick()` performs a fresh opcode fetch). Also confirms
  `executeInstruction()` still completes each opcode fully in one call, for
  both addressing modes.

Both files get added to `test/CMakeLists.txt` alongside the existing entries,
along with the new `src/cpu/alu.cpp` source.

## Follow-on work (not this pass)

- Remaining ALU-shaped ops (SBC, AND, ORA, EOR, ASL, LSR, ROL, ROR, CMP,
  CPX, CPY, INC, DEC, INX, DEX, INY, DEY), each following the same
  ALU-unit-plus-cycle-table pattern.
- Remaining addressing modes for ADC and the rest, including indexed and
  indirect-indexed modes and their page-crossing cycle-count quirks.
- Decimal (BCD) mode for ADC/SBC.
- A general opcode dispatch table, once enough opcode/addressing-mode
  combinations exist to make the current per-opcode `switch` unwieldy.
