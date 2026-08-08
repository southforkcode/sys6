# Reset sequence accuracy — design

Date: 2026-08-08

## Goal

Replace `CPU6502::reset()`'s synchronous "slam registers to fixed values"
implementation with a cycle-accurate 7-cycle reset sequence matching real
6502 hardware: `PC` loaded from `cResetVector`, `SP` decremented by 3 from
whatever it already held (not force-set), and `A`/`X`/`Y`/`D`/`B`/bit 5 left
untouched — only `I` is unconditionally forced. This is the first of four
independent fixes from a 6502-expert review of this codebase; the other
three (unimplemented-opcode trap, IRQ/NMI, decimal mode) are separate specs.

## Context

`CPU6502` (`src/cpu/cpu6502.{h,cpp}`) models a cycle-accurate 4-phase clock
(`ClockPhase::Low/LowToHigh/High/HighToLow`) driving a `CpuStep` state
machine (`T0`..`T6`). Every opcode family has a `capture*()`/`commit*()`
handler pair, dispatched via `switch (m_IR)` in `onClockHigh()`/
`onClockLow()`, with `m_cpuStep == T0` special-cased ahead of that switch to
handle opcode fetch. `executeInstruction()` is a convenience wrapper that
calls `tick()` in a loop until back at `T0`/`Low`, so a caller gets
synchronous "run one instruction" semantics while every tick along the way
is individually observable (bus reads/writes, trace logging) to a caller
who instead drives `tick()` manually — see `cpu6502_brk_test.cpp`'s
`SevenTicksCompleteBrk`, which calls `tick()` 28 times directly instead of
`executeInstruction()`.

`reset()` today is the one exception to this model: a synchronous function
with no bus activity and nothing `tick()`-observable, that hardcodes
`PC(0)`, `SP(0xff)`, `A(0)`, `X(0)`, `Y(0)`, `DFlag(false)`, `BFlag(true)`,
and bit 5. `cResetVector = 0xfffc` is already declared (alongside
`cBRKVector`, which *is* used by `BRK`) but never read anywhere.

None of `m_A`, `m_X`, `m_Y`, `m_PC`, `m_SP` have a default member
initializer in `cpu6502.h` (unlike `m_addrLatch`, `m_halted`, etc., which
do) — they hold an indeterminate value until first written, which today is
masked by `reset()` unconditionally overwriting all five.

## End-state semantics

| State | New behavior | Real-hardware justification |
|---|---|---|
| `PC` | Loaded from `cResetVector` (`$FFFC` low byte, `$FFFD` high byte) | The actual purpose of a reset vector |
| `SP` | Decremented by 3 from its current value | Real RESET performs 3 phantom stack "pushes" — reads, not writes (R/W held high) — that only decrement S; it is never force-set |
| `I` | Forced to `1` | Real hardware forces this; already correct today, unchanged |
| `D` | Left untouched | Real NMOS 6502 RESET does not clear D — every real-world 6502 boot ROM starts with `CLD` defensively for exactly this reason. Forcing it to 0 today masks that behavior. |
| `A`, `X`, `Y` | Left untouched | Real RESET never touches the data registers — only `PC`, `SP`, and `I` are affected. A warm reset should preserve program state. |
| `B`, bit 5 | Left untouched, not force-set | These bits have no physical storage on real silicon; this codebase already models that correctly for `BRK`/`PHP` ("no physical B flip-flop — synthesized on every push"). Reset's phantom stack cycles never write `P` anywhere (writes are suppressed), so there is no push event to synthesize a B value from. Forcing `BFlag(true)`/bit 5 in `reset()` today is inconsistent with that existing model. |
| `C`, `Z`, `V`, `N` | Left untouched (already the case today; unchanged) | Real RESET does not touch these |
| `halted()` | Cleared at the start of the sequence (`T0` commit) | Not a real hardware concept — this emulator's own driver-loop signal. Reset is what un-halts the CPU, so clearing it as soon as reset begins is the natural point. |

Supporting fix: add `= 0` default member initializers to `m_A`, `m_X`,
`m_Y`, `m_PC`, `m_SP` in `cpu6502.h`. This closes a latent indeterminate-read
gap and is what makes "cold boot lands on `SP == 0xFD`" (0x00 - 3, wrapping)
a well-defined, reproducible outcome rather than an accident of whatever
`reset()` used to hardcode.

## T-state sequence

Mirrors `captureBRK`/`commitBRK`'s shape, since real RESET is the interrupt
microcode with writes suppressed and a different vector:

| Step | Capture (high) | Commit (low) |
|---|---|---|
| T0 | dummy read at current `m_PC`, discarded | `m_halted = false`; → T1 |
| T1 | dummy read at current `m_PC`, discarded | → T2 |
| T2 | idle | `m_SP--` (phantom push #1, no bus write) → T3 |
| T3 | idle | `m_SP--` (#2) → T4 |
| T4 | idle | `m_SP--` (#3); `IFlag(true)` → T5 |
| T5 | `m_addrLatch = m_bus.read(cResetVector)` | → T6 |
| T6 | `m_addrLatch \|= m_bus.read(cResetVector + 1) << 8` | `m_PC = m_addrLatch` → T0 |

7 total cycles, matching real 6502 RESET timing.

## Dispatch mechanism

Add a private `bool m_resetActive = false` member. In `onClockHigh()` and
`onClockLow()`, check `m_resetActive` *before* the existing
`m_cpuStep == CpuStep::T0` opcode-fetch check, routing to new
`captureReset()`/`commitReset()` instead of falling into the `m_IR` switch.
`commitReset()`'s `T6` case clears `m_resetActive` on the way back to `T0`.

Public `reset()` becomes:

```cpp
void CPU6502::reset() {
    m_resetActive = true;
    m_cpuStep = CpuStep::T0;
    do {
        tick();
    } while (m_cpuStep != CpuStep::T0 || m_clockPhase != ClockPhase::Low);
}
```

— the same arm-then-pump-to-completion pattern `executeInstruction()`
already uses (`m_resetActive` doesn't need to appear in the loop condition
itself: `commitReset()`'s `T6` case clears it in the same tick it sets
`m_cpuStep` back to `T0`, so the existing `cpuStep`/`clockPhase`
disambiguation trick `executeInstruction()` already relies on works
unchanged here). Every existing call site (`cpu.reset()`, synchronous,
fully settled on return) keeps working unchanged. A driver that wants to
trace or single-step the reset sequence can call `tick()` manually instead
of `reset()`, the same way they already can mid-instruction.

## Testing

- Rewrite `ResetSetsRegistersAndFlagsToPowerOnState`
  (`test/cpu/cpu6502_test.cpp`): on a freshly constructed CPU with
  `cResetVector` pointing at `0x1234` in RAM, after `reset()`: `PC() ==
  0x1234`, `SP() == 0xFD`, `IFlag() == true`, `A()/X()/Y() == 0` (cold-boot
  default, not reset-enforced), `DFlag() == false` (default-initialized
  bitset, not reset-enforced).
- New test: warm reset preserves state — set `A`/`X`/`Y` to nonzero values,
  set `D` true, set `SP` to an arbitrary value (e.g. `0x80`), call
  `reset()`, assert `A`/`X`/`Y`/`D` are unchanged and `SP() == 0x7D` (`0x80
  - 3`).
- New test: `SP` wraps correctly decrementing near `0x00` (e.g. starting
  `SP == 0x01`, expect `0xFE` after reset).
- New test mirroring `SevenTicksCompleteBrk`: exactly 7 `tick()`-pairs
  (28 `tick()` calls) complete the reset sequence when driven manually,
  landing on the vector-loaded `PC` and `halted() == false`.
- New test: `B` flag and bit 5 are left untouched by `reset()` — set
  `P()` to a value with B/bit 5 clear beforehand, confirm `reset()` doesn't
  force them to 1 (contrasting with `BRK`/`PHP`, which still do, via their
  own existing push-time synthesis, unaffected by this change).
- Existing `BrkSetsHaltedAfterExecuting`-style tests that call
  `cpu.reset()` then `cpu.PC(...)` manually continue to work unchanged
  (`reset()` still returns fully settled; tests just override `PC` after,
  as before).

## Out of scope

- IRQ/NMI hardware interrupt lines — separate spec.
- Unimplemented-opcode trap — separate spec.
- Decimal (BCD) mode — separate spec.
- Modeling the exact historical address-bus values during `T0`/`T1`'s dummy
  reads with full hardware fidelity beyond "read at current PC, discard" —
  matches this codebase's existing precedent for other discarded reads
  (e.g. `captureBRK`'s `T1` padding-byte read).
