# Remaining ALU-shaped opcodes — design spec

## Context

`ADC` is fully implemented across all eight of its addressing modes
(`docs/superpowers/specs/2026-08-08-cycle-stepped-alu-design.md`,
`docs/superpowers/specs/2026-08-08-adc-addressing-modes-design.md`). Both of
those specs deferred the rest of the ALU-shaped instruction set as
"follow-on work, once the architecture is proven": `SBC, AND, ORA, EOR, ASL,
LSR, ROL, ROR, CMP, CPX, CPY, INC, DEC, INX, DEX, INY, DEY`. This spec covers
all of it.

Implementing these opcode-by-opcode the way `ADC` was built — one
`captureXxx`/`commitXxx` method pair per opcode per addressing mode — would
mean roughly 70 near-identical method pairs (six binary ALU ops × eight
shared addressing modes, plus the compare/shift/increment families). This
spec instead factors the addressing-mode machinery so it is opcode-agnostic:
each addressing-mode *shape* gets one capture/commit pair, shared by every
opcode that uses it, parameterized by which ALU operation and which
register/memory target apply. `ADC`'s existing capture/commit methods are
rewritten onto this shared machinery — no behavior changes, verified by the
existing ADC tests staying green throughout.

## Goals

- Implement all 17 remaining ALU-shaped mnemonics, across every addressing
  mode they support on real 6502 hardware, matching real cycle counts.
- Factor addressing-mode resolution + bus access into shared, opcode-agnostic
  capture/commit functions, so adding an opcode is "add a dispatch entry
  mapping opcode → (addressing mode, ALU op, target)" rather than "write a
  new capture/commit pair."
- Extend `ALU` with one pure function per new operation, unit-tested the same
  way `adc` already is.
- Extend `docs/6502-manual.md` with every new opcode row.

## Non-goals

- Decimal (BCD) mode. Still binary-only, still not consulted (`DFlag()`
  stays unread by every op in this pass).
- Interrupts, `BRK`, branches, stack ops, jumps, transfers (`TAX` etc.),
  flag-only ops (`CLC` etc.) — none of those are ALU-shaped; out of scope.
- A general data-driven opcode dispatch table keyed by opcode byte alone.
  This pass still dispatches via a `switch (m_IR)`, just with many case
  labels sharing one handler body instead of one handler per opcode. A
  metadata-table-driven dispatcher remains future work, as the prior specs
  already noted.
- Reproducing the 6502's genuine RMW "dummy write" of the unmodified value
  before the real write. Same divergence policy as the existing "no dummy
  read" decision for indexed addressing: the cycle is spent idle instead,
  total cycle count still matches hardware. Documented in the manual like
  the existing divergence note.

## Architecture

### ALU: new pure functions

All new methods return the existing `AluResult` shape; callers read only the
fields that are semantically meaningful for that op (e.g. `CMP`'s caller
never reads `.overflow`).

```cpp
AluResult sbc(uint8_t acc, uint8_t operand, bool carryIn) const;
AluResult bitwiseAnd(uint8_t acc, uint8_t operand) const;
AluResult bitwiseOr(uint8_t acc, uint8_t operand) const;
AluResult bitwiseXor(uint8_t acc, uint8_t operand) const;
AluResult cmp(uint8_t reg, uint8_t operand) const;
AluResult asl(uint8_t value) const;
AluResult lsr(uint8_t value) const;
AluResult rol(uint8_t value, bool carryIn) const;
AluResult ror(uint8_t value, bool carryIn) const;
AluResult increment(uint8_t value) const;
AluResult decrement(uint8_t value) const;
```

- `sbc(a, b, c)` = `adc(a, ~b, c)`. This is not a coincidence to paper over —
  it's how the real 6502 ALU works: subtract is add-with-inverted-operand,
  and `SBC`'s `carryIn` doubles as "not borrow," so the identity is exact for
  binary mode. `sbc` is implemented as a one-line delegation to `adc`.
- `cmp(reg, operand)` = `adc(reg, ~operand, /*carryIn=*/true)`. Same
  identity, fixed `carryIn=true` (no incoming borrow for a compare).
  `.carry` comes out `true` exactly when `reg >= operand`, `.zero` when
  equal, `.negative` from bit 7 of `reg - operand` — the correct 6502 `CMP`
  flag semantics, for free.
- `bitwiseAnd`/`bitwiseOr`/`bitwiseXor`: `.value = acc <op> operand`,
  `.zero`/`.negative` from `.value`. `.carry` and `.overflow` are set to
  `false` and never read by any caller (logical ops never touch `C`/`V`).
- `asl(value)`: `.value = (value << 1) & 0xFF`, `.carry = ` bit 7 of the
  *input*, `.zero`/`.negative` from `.value`.
- `rol(value, carryIn)`: same as `asl` but bit 0 of the result comes from
  `carryIn` instead of always `0`.
- `lsr(value)`: `.value = value >> 1`, `.carry = ` bit 0 of the input,
  `.zero`/`.negative` from `.value` (bit 7 is always `0` after an `LSR`, so
  `.negative` is always `false` — real hardware behavior, not special-cased).
- `ror(value, carryIn)`: same as `lsr` but bit 7 of the result comes from
  `carryIn`.
- `increment`/`decrement(value)`: `.value = (value ± 1) & 0xFF`,
  `.zero`/`.negative` from `.value`. `.carry`/`.overflow` unused (6502
  `INC`/`DEC`/`INX`/`DEX`/`INY`/`DEY` never touch `C` or `V`).

### CPU6502: ALU operation selector

`tick()` currently hardcodes `m_alu.adc(...)` as the always-on combinational
recompute. It gains an operation selector so the same "recompute every tick
from current latches" pattern covers every op:

```cpp
enum class AluOp : uint8_t { ADC, SBC, AND, ORA, EOR, CMP, ASL, LSR, ROL, ROR, INC, DEC };
```

`m_aluOp` (new member, defaults to `AluOp::ADC`) selects which `ALU` method
`tick()` calls. Binary ops (`ADC`/`SBC`/`AND`/`ORA`/`EOR`/`CMP`) read
`m_aluA` and `m_aluB`; unary ops (`ASL`/`LSR`/`ROL`/`ROR`/`INC`/`DEC`) read
only `m_aluB` (the convention: the value being transformed always lands in
`m_aluB`, whether it came from a register or memory — `m_aluA` is simply
unused for unary ops, not repurposed).

`loadAluInputs(operand)` (currently ADC-only, implicitly sourcing `m_aluA`
from `m_A`) is replaced by two explicit entry points:

```cpp
void beginBinaryAluOp(AluOp op, uint8_t regValue, uint8_t operand);
void beginUnaryAluOp(AluOp op, uint8_t value);
```

Both set `m_aluCarryIn = CFlag()` (harmless for ops that ignore it) and leave
committing to the caller.

### CPU6502: opcode → (mode, op, target) dispatch

Two families of shared capture/commit functions, replacing per-opcode ones:

**Binary-ALU family** (`ADC`, `SBC`, `AND`, `ORA`, `EOR`, `CMP`, `CPX`,
`CPY`) — read-only, writes at most a register, never memory. One
capture/commit pair per addressing-mode *shape* (`Immediate`, `ZeroPage`,
`ZeroPageX`, `Absolute`, `AbsoluteX`, `AbsoluteY`, `IndirectX`,
`IndirectY` — exactly `ADC`'s existing eight, renamed from `captureADCFoo` to
`captureReadFoo`/`commitReadFoo` since they're no longer ADC-specific). Each
shared capture function ends by calling a new opcode-dispatching helper
instead of the old `loadAluInputs`:

```cpp
void applyBinaryAluOp(uint8_t operand); // switches on m_IR -> beginBinaryAluOp(op, regValue, operand)
```

and each shared commit function calls a new opcode-dispatching helper
instead of the old `commitAluResult`:

```cpp
void commitBinaryAluResult(); // switches on m_IR -> writes register (or not) + the right flag subset
```

`commitBinaryAluResult` covers three flag-commit shapes, chosen by `m_IR`:

- `ADC`/`SBC`: write `A`; set `C`, `Z`, `V`, `N`.
- `AND`/`ORA`/`EOR`: write `A`; set `Z`, `N` only (`C`, `V` untouched — real
  6502 behavior, not an oversight).
- `CMP`/`CPX`/`CPY`: write nothing; set `C`, `Z`, `N` only.

`CPX`/`CPY` reuse the `Immediate`/`ZeroPage`/`Absolute` shapes only (their
only addressing modes); `applyBinaryAluOp` sources `regValue` from `m_X`/
`m_Y` instead of `m_A` for those opcodes.

**Read-modify-write family** (`ASL`, `LSR`, `ROL`, `ROR`, `INC`, `DEC`) —
reads a value, transforms it, writes it back (to `A` for shift/rotate
`Accumulator` mode, to memory for every other mode). One capture/commit pair
per addressing-mode shape (`Accumulator`, `ZeroPage`, `ZeroPageX`,
`Absolute`, `AbsoluteX`), each ending in:

```cpp
void applyUnaryAluOp(uint8_t value);  // switches on m_IR -> beginUnaryAluOp(op, value)
void commitUnaryAluResult();          // switches on m_IR -> writes A or bus.write(addr, value) + C?/Z/N
```

`commitUnaryAluResult` needs the target address for memory-mode writes,
which is already sitting in `m_addrLatch` (fixed modes) or `m_effAddr`
(`AbsoluteX`) by the time commit runs — same latches the read-only family
already uses to *read*, now also used as the write-back address. Flag
handling: `ASL`/`LSR`/`ROL`/`ROR` set `C`, `Z`, `N`; `INC`/`DEC` set `Z`, `N`
only.

`AbsoluteX` for this family is a **fixed** 7 cycles — real 6502 RMW
instructions always spend the extra cycle, unlike the read-only family where
it's conditional on an actual page crossing. `indexedAddress()` is still
called for the address arithmetic (wrapping is identical); its
`.pageCrossed` field is simply not consulted for timing here.

**Implied-register family** (`INX`, `DEX`, `INY`, `DEY`) — 2 cycles, no bus
access beyond the opcode fetch, operates directly on `X` or `Y`:

```cpp
void captureImpliedIncDec(); // switches on m_IR -> beginUnaryAluOp(INC|DEC, m_X or m_Y)
void commitImpliedIncDec();  // switches on m_IR -> writes X or Y + Z, N; T0
```

### New opcode constants

Standard NMOS 6502 opcode bytes (unchanged since the original 1975 part;
these are not implementation choices):

| Mnemonic | Imm | ZP | ZP,X | Abs | Abs,X | Abs,Y | (Ind,X) | (Ind),Y | Acc |
|---|---|---|---|---|---|---|---|---|---|
| SBC | E9 | E5 | F5 | ED | FD | F9 | E1 | F1 | |
| AND | 29 | 25 | 35 | 2D | 3D | 39 | 21 | 31 | |
| ORA | 09 | 05 | 15 | 0D | 1D | 19 | 01 | 11 | |
| EOR | 49 | 45 | 55 | 4D | 5D | 59 | 41 | 51 | |
| CMP | C9 | C5 | D5 | CD | DD | D9 | C1 | D1 | |
| CPX | E0 | E4 | | EC | | | | | |
| CPY | C0 | C4 | | CC | | | | | |
| ASL | | 06 | 16 | 0E | 1E | | | | 0A |
| LSR | | 46 | 56 | 4E | 5E | | | | 4A |
| ROL | | 26 | 36 | 2E | 3E | | | | 2A |
| ROR | | 66 | 76 | 6E | 7E | | | | 6A |
| INC | | E6 | F6 | EE | FE | | | | |
| DEC | | C6 | D6 | CE | DE | | | | |

`INX=E8`, `DEX=CA`, `INY=C8`, `DEY=88`.

### `CpuStep` widened to `T6`

The RMW `AbsoluteX` shape needs 7 cycles (`T0`..`T6`); `CpuStep` gains one
more enumerator.

## Compatibility

- Public API unchanged.
- `ADC`'s behavior and existing tests (`cpu6502_adc_test.cpp`) are the
  regression backstop for the refactor: every existing ADC test must stay
  green after `captureADCFoo`/`commitADCFoo` are rewritten onto the shared
  `captureReadFoo`/`commitReadFoo` machinery, with no assertions changed.
- The unimplemented-opcode fallback (1-cycle no-op) is unchanged and still
  covers every opcode not listed above (branches, jumps, stack ops, flag
  ops, transfers, `BRK`, `NOP`, ...).

## Testing plan

- `alu_test.cpp`: new `TEST`s for every new `ALU` method, matching the
  existing style — normal case, edge/boundary case (e.g. `asl(0x80)` carries
  out and zeros), and for `rol`/`ror` a carry-in-propagation case.
- `cpu6502_adc_test.cpp`: unchanged assertions; passing unchanged after the
  refactor is the regression proof for the shared addressing-mode machinery.
- New test files, one per opcode family, mirroring `cpu6502_adc_test.cpp`'s
  style but not its exhaustive per-clock-phase treatment (that rigor proved
  the *architecture* once; it doesn't need re-proving per opcode). Each new
  opcode/mode combination gets: a correctness test (right value, right
  flags, right `PC` advance) and a cycle-count test (right number of ticks
  to complete, including the page-crossing/RMW-fixed-extra-cycle cases where
  applicable):
  - `cpu6502_sbc_test.cpp`, `cpu6502_and_test.cpp`, `cpu6502_ora_test.cpp`,
    `cpu6502_eor_test.cpp`, `cpu6502_cmp_test.cpp` — one file per opcode,
    all eight addressing modes each, since each is a distinct mnemonic with
    its own flag-semantics tests (e.g. `CMP` never touches `A`).
  - `cpu6502_cpx_cpy_test.cpp` — both compare-register opcodes, three modes
    each.
  - `cpu6502_shift_test.cpp` — `ASL`/`LSR`/`ROL`/`ROR`, all five modes each,
    including a carry-in-propagation test per rotate.
  - `cpu6502_inc_dec_test.cpp` — `INC`/`DEC`, four modes each, plus
    `INX`/`DEX`/`INY`/`DEY`.

All new test files added to `test/CMakeLists.txt`.

`docs/6502-manual.md` gains one row per new opcode/mode, plus a note on the
RMW `AbsoluteX` rows calling out the fixed-7-cycles divergence from the
read-only family's conditional timing, and extending the existing
"Divergences from real hardware" section with the dummy-write omission.

## Follow-on work (not this pass)

- Decimal (BCD) mode for `ADC`/`SBC`.
- Branches, stack ops, jumps, transfers, flag ops, `BRK`/interrupts — the
  rest of the 6502 instruction set, none of it ALU-shaped.
- A metadata-table-driven opcode dispatcher, once this pass's `switch`
  (now covering ~70 opcode/mode combinations across two shared handler
  families) is judged unwieldy enough to be worth it.
