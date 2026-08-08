# Clock phase model for CPU6502 — design spec

## Context

`CPU6502::tick()` currently advances one full T-state (`CpuStep` T0→T1→…)
per call, and does all of that T-state's work — bus reads, ALU invocation,
register/flag writeback, `CpuStep` advance — synchronously in one shot. The
ALU (`ALU::adc`) is only invoked when an opcode handler explicitly calls
`tickAlu()`.

Real clocked hardware doesn't work this way: a physical clock has a
continuous high/low waveform, sequential logic (registers, latches) only
commits on stable levels of that waveform, and combinational logic (like an
ALU) is never "invoked" — it's wired directly to its inputs and is always
producing an output, which downstream registers simply choose to latch (or
not) at the right moment. This spec introduces an explicit `ClockPhase`
state machine into `CPU6502` to model that distinction, building on the
cycle-stepped foundation from
`docs/superpowers/specs/2026-08-08-cycle-stepped-alu-design.md`.

## Goals

- Model the clock itself as a 4-phase cycle: `Low`, `LowToHigh`, `High`,
  `HighToLow`. `tick()` advances exactly one phase per call.
- Sequential logic (register/flag writes, `PC` increments, `CpuStep`
  advances) commits only on arrival at a stable phase (`High` or `Low`),
  never during a transition phase.
- The `ALU` is modeled as always-on combinational logic: `CPU6502`
  unconditionally recomputes `m_aluOutput` from its current input latches on
  every single `tick()` call, regardless of phase or opcode. Nothing about
  invoking the ALU is opcode-specific anymore — only *loading its inputs*
  and *latching its output* are.
- `ADC #immediate` and `ADC absolute` keep working end-to-end, now expressed
  as capture (on `High`) / commit (on `Low`) halves per cycle instead of one
  monolithic per-cycle action.
- `executeInstruction()` and the public register/flag API are unaffected —
  still one call per full instruction, no signature changes.

## Non-goals

- Real 6502 φ1/φ2 electrical fidelity (e.g. address-bus-setup-before-data-
  valid sub-timing, actual non-overlapping clock generation). This is a
  clean two-commit-point simulation model, not a transistor-level
  recreation.
- Any new opcode, addressing mode, or ALU operation beyond what already
  exists (`ADC #immediate`, `ADC absolute`).
- A shared/system-wide clock object driving multiple chips. `ClockPhase`
  stays a `CPU6502`-internal concept; a system clock is future scope if
  other clocked units (e.g. a PPU) are ever added.
- Decimal (BCD) mode, interrupts, or `reset()` behavior changes — unchanged
  from the prior spec's non-goals.

## Architecture

### `ClockPhase`

New enum in `cpu6502.h`, alongside `CpuStep`:

```cpp
enum class ClockPhase : uint8_t { Low, LowToHigh, High, HighToLow };
```

New protected member: `ClockPhase m_clockPhase = ClockPhase::Low;`

`tick()` advances `m_clockPhase` cyclically by exactly one step every call:
`Low → LowToHigh → High → HighToLow → Low → …`. A full clock cycle — what a
single `tick()` call used to represent — now takes **4** `tick()` calls.
`CpuStep` still means "which T-state of the instruction," and only advances
once per full clock cycle (i.e. once per 4 `tick()` calls), on arrival at
`Low`.

`executeInstruction()` needs no changes:

```cpp
void CPU6502::executeInstruction() {
    do {
        tick();
    } while (m_cpuStep != CpuStep::T0);
}
```

It still loops purely on `m_cpuStep`, so it transparently calls `tick()` 4x
more per instruction than before without any logic change.

### `tick()` structure

```cpp
void CPU6502::tick() {
    m_aluOutput = m_alu.adc(m_aluA, m_aluB, m_aluCarryIn);

    m_clockPhase = nextPhase(m_clockPhase);

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
```

The ALU recompute at the top is unconditional and opcode-agnostic — it runs
on all 4 phases, every `tick()` call, whether or not the current cycle's
opcode handler uses the ALU that cycle. This is what makes it "always on":
`CPU6502` never asks the ALU to compute, it just always has a fresh answer
sitting in `m_aluOutput` for whatever `m_aluA`/`m_aluB`/`m_aluCarryIn`
currently hold.

`onClockHigh()` and `onClockLow()` dispatch on `m_IR` and `m_cpuStep`, the
same way `tick()`'s body does today, but each opcode's per-cycle handler is
now split into a capture half (runs from `onClockHigh()`) and a commit half
(runs from `onClockLow()`):

- **Capture (`High`)**: read from the bus into internal latches — opcode
  byte → `m_IR`, address bytes → `m_addrLatch`, operand byte → the ALU
  input latches (`m_aluA`, `m_aluB`, `m_aluCarryIn`).
- **Commit (`Low`)**: sequential-logic writes that depend on this cycle's
  settled state — `PC++` (only on cycles that read from `PC`), latching
  `m_aluOutput` into `A`/flags (only on cycles that used the ALU), and
  advancing `m_cpuStep` to the next T-state.

### Per-opcode capture/commit tables

**`ADC #immediate` (`0x69`, unchanged 2 T-states, now 8 `tick()` calls)**

| T-state | High (capture) | Low (commit) |
|---|---|---|
| T0 | read opcode at `PC` → `m_IR` | `PC++`; `m_cpuStep = T1` |
| T1 | read operand at `PC` → `m_aluA=A`, `m_aluB=operand`, `m_aluCarryIn=CFlag()` | `PC++`; latch `m_aluOutput` → `A`/flags; `m_cpuStep = T0` |

**`ADC absolute` (`0x6D`, unchanged 4 T-states, now 16 `tick()` calls)**

| T-state | High (capture) | Low (commit) |
|---|---|---|
| T0 | read opcode at `PC` → `m_IR` | `PC++`; `m_cpuStep = T1` |
| T1 | read low addr byte at `PC` | store into `m_addrLatch` low byte; `PC++`; `m_cpuStep = T2` |
| T2 | read high addr byte at `PC` | combine into `m_addrLatch` high byte; `PC++`; `m_cpuStep = T3` |
| T3 | read operand at `m_addrLatch` → ALU input latches | latch `m_aluOutput` → `A`/flags; `m_cpuStep = T0` (no `PC++`: address came from `m_addrLatch`, not `PC`) |

The generic opcode-fetch T0 is identical for both opcodes (as it is today)
and for the unimplemented-opcode fallback — the fallback's capture half is a
no-op, and its commit half is just `m_cpuStep = T0`.

## Compatibility

- Public API (`executeInstruction()`, register/flag getters/setters,
  `reset()`) is unchanged.
- Any caller that treats `tick()` as "one full clock cycle" breaks — within
  this codebase, that's only the test suite (see below). `main.cpp` uses
  `executeInstruction()`, not `tick()` directly, so it's unaffected.

## Testing plan

`test/cpu/cpu6502_test.cpp` and `test/cpu/cpu6502_adc_test.cpp` need
updating wherever they call `tick()` directly and assert on state
immediately after, since one `tick()` call is now a quarter of a clock
cycle:

- Existing assertions that expected one `tick()` call to complete a fetch or
  a cycle's work move to "after 4 `tick()` calls" (one full clock cycle).
- New tests assert that intermediate phases (`LowToHigh`, `High`,
  `HighToLow`) within a cycle do **not** commit sequential state — e.g. `PC`
  and `A` stay unchanged through the first 3 `tick()` calls of a cycle and
  only change on the 4th (`Low`).
- A new direct test on the always-on ALU behavior: loading
  `m_aluA`/`m_aluB`/`m_aluCarryIn` and calling `tick()` produces a fresh
  `m_aluOutput` even without an opcode driving it — this needs a test seam
  (e.g. a test-only accessor or a friend test fixture) since these are
  currently private members; the implementation plan should specify exactly
  how this gets exposed for testing without polluting the public API.
- `executeInstruction()`-level tests (`ExecuteInstructionFetchesOpcode...`,
  the ADC end-to-end tests) need no behavioral changes, only re-verification
  that they still pass unchanged given they don't call `tick()` directly.

## Follow-on work (not this pass)

- Extending the capture/commit split to future opcodes/addressing modes as
  they're implemented, following the same pattern established here.
- A shared system-level clock object, if/when additional clocked units
  beyond `CPU6502` are introduced.
- Revisiting whether `ClockPhase` should also gate *reads* from the bus
  (i.e. modeling that the bus itself only has valid data during certain
  phases), currently out of scope since there's only one bus master.
