# Stack operations (PHA/PLA/PHP/PLP) — design spec

## Context

`JSR`/`RTS` (`docs/superpowers/specs/2026-08-08-relative-branches-and-utilities-design.md`
and follow-on commits) and `BRK` already push/pull through the `0x0100 +
m_SP` stack page, so the push/pull mechanics themselves are proven. What's
missing is direct stack access: `PHA`/`PLA` move a byte between `A` and the
stack; `PHP`/`PLP` do the same for the status register `P`. This spec adds
all four.

## Goals

- Implement `PHA` (`0x48`), `PLA` (`0x68`), `PHP` (`0x08`), `PLP` (`0x28`),
  matching real 6502 cycle counts (3 for the pushes, 4 for the pulls).
- `PHP` forces the B flag to 1 in the pushed byte, exactly like `BRK`
  already does — same real-hardware reason (there's no physical B flip-flop;
  it's synthesized on every push).
- `PLA` sets `Z`/`N` from the pulled byte. `PLP` overwrites the whole status
  register from the pulled byte, `BFlag` included — `PLP` restores exactly
  what was pushed, unlike `PLA` there is no separate flag computation.
- Extend `docs/6502-manual.md` with all four opcode rows.

## Non-goals

- The unused bit 5 of `P`. This codebase has no `cUnusedFlagOffset` and
  never has (`P()` is a bare `m_pFlags.to_ulong()`); `PHP`/`PLP` don't
  introduce one either. Real hardware always reads bit 5 back as 1, but
  nothing in this emulator inspects that bit, so modeling it would be
  speculative work with no observable effect — consistent with `BRK`
  forcing only `B`, not bit 5.
- Any other addressing mode. All four stack opcodes are Implied-only on
  real hardware.

## Architecture

Two shared capture/commit pairs, the same "one pair per shape" convention
`captureImpliedFlagOp`/`captureImpliedTransfer` already use — `PHA`/`PHP`
share a push pair, `PLA`/`PLP` share a pull pair, each deciding the exact
byte from `m_IR`.

### Push family (PHA/PHP): 3 cycles

Real 6502 timing: T0 fetch opcode, T1 a dummy read of the next byte
(discarded — internal-operation cycle, same convention as `captureBRK`'s
T1 and `captureRTS`'s T1/T2), T2 the actual push. `m_SP` is only
authoritative inside commit, so the push itself happens in `commitImpliedPush`
the same way `commitJSR`/`commitBRK` push in commit rather than capture.

```cpp
void captureImpliedPush() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        static_cast<void>(m_bus.read(m_PC)); // dummy read, discarded
        break;
    case CpuStep::T2:
        break; // idle: the push happens in commit, where m_SP is authoritative
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T2.
    }
}

void commitImpliedPush() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2: {
        uint8_t value = m_IR == cOpPHP ? (BFlag(true), P()) : A();
        m_bus.write(static_cast<uint16_t>(0x0100 + m_SP), value);
        m_SP--;
        m_cpuStep = CpuStep::T0;
        break;
    }
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T2.
    }
}
```

(The `BFlag(true), P()` comma form is a sketch, not a mandate — the real
implementation may just branch on `m_IR` explicitly the way
`commitImpliedFlagOp` does. The point is: `PHP` forces `B` before reading
`P()`, mutating live state exactly like `commitBRK`'s T4 does.)

### Pull family (PLA/PLP): 4 cycles

Real 6502 timing: T0 fetch opcode, T1 dummy read (discarded), T2 internal
`S` increment, T3 the actual pull — the same four-cycle shape `captureRTS`/
`commitRTS` already use for their first pull, just stopping after one byte
instead of two.

```cpp
void captureImpliedPull() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        static_cast<void>(m_bus.read(m_PC)); // dummy read, discarded
        break;
    case CpuStep::T2:
        break; // idle: S increment happens in commit, where m_SP is authoritative
    case CpuStep::T3:
        m_addrLatch = m_bus.read(static_cast<uint16_t>(0x0100 + m_SP));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}

void commitImpliedPull() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_SP++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3: {
        auto pulled = static_cast<uint8_t>(m_addrLatch);
        if (m_IR == cOpPLA) {
            m_A = pulled;
            ZFlag(aluZero(pulled));
            NFlag(aluNegative(pulled));
        } else {
            P(pulled);
        }
        m_cpuStep = CpuStep::T0;
        break;
    }
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T3.
    }
}
```

Capture's T3 read depends on `m_SP` already being post-increment — true by
construction, since commit's T2 (which increments `m_SP`) always runs
before capture's T3 (the next tick), exactly the same dependency
`captureRTS`'s T3/T4 already have on `commitRTS`'s T2/T3.

## Testing

New `test/cpu/cpu6502_stack_test.cpp`, mirroring
`test/cpu/cpu6502_jsr_rts_test.cpp`'s structure:

- `PhaPushesAccumulatorOntoStack` / `PhaDecrementsStackPointer`
- `PlaPullsAccumulatorFromStack` / `PlaIncrementsStackPointer`
- `PlaSetsZeroFlagWhenPulledValueIsZero` / `PlaSetsNegativeFlagWhenPulledValueIsNegative`
- `PhpPushesStatusRegisterWithBFlagForced`
- `PlpRestoresStatusRegisterExactly` (including `B`)
- `PhaThreeTicksCompleteInstruction` / `PlaFourTicksCompleteInstruction` (cycle-count checks, same shape as the existing `JsrSixTicksCompleteInstruction`)
- `PhaThenPlaRoundTripsAccumulator` (round-trip, mirroring `JsrThenRtsRoundTripsBackToCallSite`)
