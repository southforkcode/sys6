# Relative branches, BRK halt, and program loader — design

Date: 2026-08-08

## Goal

Add the 8 relative conditional branch instructions to `CPU6502`, give BRK real
interrupt semantics that a driver can use to stop instruction processing,
add a utility to load a program into memory from a hex-string, and add a
`run()` driver helper — then prove all of it end-to-end with a hand-assembled
program that computes and stores the first 10 Fibonacci numbers.

## Context

`CPU6502` (`src/cpu/cpu6502.{h,cpp}`) models a cycle-accurate 4-phase clock
(`ClockPhase::Low/LowToHigh/High/HighToLow`) driving a `CpuStep` state machine
(`T0`..`T6`). Each opcode family has a `capture*()`/`commit*()` handler pair,
dispatched via `switch (m_IR)` in `onClockHigh()`/`onClockLow()`. Addressing
modes that can cross a page boundary (e.g. ADC absolute,X) already use an
`indexedAddress(base, index)` helper returning `EffectiveAddress{address,
pageCrossed}`, and defer the extra cycle by checking `m_pageCrossed` in the
commit handler. No branch or BRK opcodes exist yet. `cBRKVector = 0xfffe` is
already defined but unused. There is no stack push/pull machinery yet (no
JSR/PHA/RTS). `Bus`/`RAM`/`ROM` implement/use `MemoryDevice`
(`src/memory/memory_device.h`).

## Relative branches (BEQ/BNE/BCS/BCC/BPL/BMI/BVS/BVC)

One shared `captureBranch()`/`commitBranch()` pair, added as a new case in
the existing `switch (m_IR)` dispatch, mirroring how `applyBinaryAluOp()`
already switches on `m_IR` inside a single shared function rather than having
8 near-identical per-opcode functions.

Opcodes and conditions:

| Mnemonic | Opcode | Condition |
|---|---|---|
| BPL | 0x10 | N == 0 |
| BMI | 0x30 | N == 1 |
| BVC | 0x50 | V == 0 |
| BVS | 0x70 | V == 1 |
| BCC | 0x90 | C == 0 |
| BCS | 0xB0 | C == 1 |
| BNE | 0xD0 | Z == 0 |
| BEQ | 0xF0 | Z == 1 |

Cycle model (matches real 6502 timing: 2 cycles not taken, 3 taken same-page,
4 taken page-crossed), reusing the `indexedAddress()`/`pageCrossed` pattern:

- **T1** (capture): read the signed offset operand at `m_PC`, evaluate the
  branch condition from `m_IR` against the current flags, store into new
  members `int8_t m_branchOffset` and `bool m_branchTaken`.
  (commit): `m_PC++`. If `!m_branchTaken`, `m_cpuStep = CpuStep::T0`
  (instruction done, 2 cycles total). Otherwise `m_cpuStep = CpuStep::T2`.
- **T2** (capture, only reached if taken): compute the branch target via a
  new `relativeAddress(uint16_t pcAfterOperand, int8_t offset)` helper,
  returning `EffectiveAddress` (same shape as `indexedAddress()`, signed
  offset instead of unsigned index). Store into the existing `m_effAddr` /
  `m_pageCrossed` members.
  (commit): if `!m_pageCrossed`, `m_PC = m_effAddr` and `m_cpuStep =
  CpuStep::T0` (3 cycles total). Otherwise `m_cpuStep = CpuStep::T3`.
- **T3** (only reached if page crossed):
  (commit): `m_PC = m_effAddr`, `m_cpuStep = CpuStep::T0` (4 cycles total).

## BRK (opcode 0x00) — full interrupt semantics

7 cycles (`T0`..`T6`, the same range already used by RMW absolute,X),
following real 6502 microcode:

- **T1**: dummy-read and discard the padding byte after the opcode
  (`m_bus.read(m_PC)`); `m_PC++`.
- **T2**: push PC high byte to `0x0100 + m_SP`, `m_SP--`.
- **T3**: push PC low byte to `0x0100 + m_SP`, `m_SP--`.
- **T4**: `BFlag(true)` (bit 5 is already forced to 1 in this codebase's
  `P()` at reset), push `P()` to `0x0100 + m_SP`, `m_SP--`; `IFlag(true)`.
- **T5**: read new PC low byte from `cBRKVector` (0xFFFE).
- **T6**: read new PC high byte from `cBRKVector + 1` (0xFFFF), combine into
  `m_PC`; set `m_halted = true`.

`m_halted` is the actual "catch a BRK" mechanism the task asked for: BRK
still performs the real push + vector-jump so stack/flag state is correct,
but setting the flag means a driver loop knows to stop rather than trying to
execute whatever (if anything) lives at the vector. New public accessor:
`bool halted() const`.

Stack pushes/pops in this codebase don't exist yet anywhere else; this is
the first use of `0x0100 + m_SP` addressing. `m_SP` is `uint8_t`, so
`m_SP--` wraps naturally at the page-1 boundary the way real 6502 stack
pointer wraparound does.

## Minimal load/store additions (LDA/STA) — discovered while designing the e2e test

The Fibonacci e2e program needs to write computed values into memory, but
this codebase has no opcode that can move a value into memory at all: only
ALU ops (which read memory into a computation), shifts, INC/DEC (which
nudge a memory cell by ±1, not assign an arbitrary value), and
INX/DEX/INY/DEY exist. Without a store instruction, no program can write
its results anywhere. This is a genuine prerequisite gap, not scope creep
for its own sake — confirmed with the user before proceeding.

Four new opcodes, added to `CPU6502` following the exact same
family/capture/commit pattern as everything else in this file:

| Mnemonic | Mode | Opcode | Cycles |
|---|---|---|---|
| LDA | Immediate | 0xA9 | 2 |
| LDA | Zero Page | 0xA5 | 3 |
| STA | Zero Page | 0x85 | 3 |
| STA | Absolute,Y | 0x99 | 5 (fixed, like RMW absolute,X — a store can't shortcut the extra cycle the way a read can) |

LDA is implemented as a "load family" that reuses the ALU's combinational
path rather than introducing a separate code path: `m_aluA = 0`,
`m_aluB = <fetched byte>`, `m_aluFunction = AluFunction::OR` makes
`m_aluOutput.value` equal the fetched byte (`b | 0 == b`) while still going
through the same `aluZero()`/`aluNegative()` flag helpers every other
opcode uses. This mirrors how INC/DEC already route through `AluFunction::ADD`
with `a = 0x01`/`0xFF` instead of having bespoke increment/decrement logic.
Real LDA does not affect the C or V flags, so only Z and N are set from
`m_aluOutput` in commit — C is deliberately left untouched.

STA does not affect any flags (real 6502 behavior) and needs no ALU
involvement; capture stages are idle placeholders (matching the existing
idle-capture convention used by e.g. `captureRmwZeroPage`), and the actual
`m_bus.write()` happens in the final commit step.

**Why no CLC was needed:** the e2e program's only comparison-like operation
for its loop counter is `DEC` on a dedicated zero-page counter cell, not
`CPY`/`CMP`/`CPX` — and per the existing `commitUnaryAluFlags()` comment,
INC/DEC deliberately leave C untouched. Since none of LDA/STA/DEC/INY touch
C, and the Fibonacci values in this program never exceed 255 (so ADC's own
carry-out is always false), the carry flag never needs an explicit reset
between iterations. This was verified by hand-tracing the full 10-iteration
program before finalizing it (see the E2E section below) — an explicit
`CLC` opcode was considered and is not needed for this program, so it's not
part of this change.

## `run()` driver helper

```cpp
bool CPU6502::run(size_t maxInstructions);
```

Calls `executeInstruction()` in a loop until `halted()` is true or
`maxInstructions` is exhausted. Returns whether it halted (`true`) vs. hit
the instruction cap without halting (`false`). The cap exists so a test
whose program never executes BRK fails fast with an assertion instead of
hanging.

## Hex-string program loader

New files `src/utils/program_loader.h` / `.cpp` (a general utility, not
CPU- or Bus-specific):

```cpp
void loadProgram(MemoryDevice &device, uint16_t startAddress, const std::string &hex);
```

Strips whitespace from `hex`, then parses the remaining characters two at a
time as hex byte pairs (case-insensitive), writing them sequentially into
`device` starting at `startAddress` via `device.write()`. Throws
`std::invalid_argument` if the stripped string has odd length or contains a
non-hex-digit character.

Operates directly against a `MemoryDevice` rather than a `Bus`: in the e2e
test (and typical unit tests), a single `RAM` spans the full address space
and is mapped 1:1 to bus addresses starting at 0, so device offsets and bus
addresses coincide. No `Bus` indirection is needed for this utility.

## E2E test: first 10 Fibonacci numbers

A hand-assembled 6502 program, provided to the test as a hex string and
loaded via `loadProgram`, that:

1. Initializes zero-page cells `$F0` (`a`, running previous term) to 0,
   `$F1` (`b`, running current term) to 1, and `$F3` (loop counter) to 10.
2. Each iteration: loads `a` from `$F0` into `A`, stores it to `$0200,Y`
   (`Y` is the 0-based output index), computes `newB = a + b` via
   `ADC $F1`, stashes it in scratch cell `$F2`, then shuffles
   `$F0 = old b`, `$F1 = newB`.
3. Increments `Y`, decrements the `$F3` counter, and loops via `BNE` while
   the counter is nonzero — this is what exercises the new branch
   instructions end-to-end, not just in isolation.
4. Ends with `BRK`.

This produces the sequence 0, 1, 1, 2, 3, 5, 8, 13, 21, 34 at `$0200..$0209`
(verified by hand-tracing all 10 iterations while designing this spec).
Zero-page addresses `$F0-$F3` are used (not `$10-$13`) specifically because
the program's own machine code occupies addresses `$0000-$0022`, and using
low zero-page addresses would have the program overwrite its own
instructions mid-execution.

Test body:

```cpp
loadProgram(ram, 0x0000, hexString);
cpu.reset();
ASSERT_TRUE(cpu.run(10000));
// assert ram bytes at 0x0200-0x0209 == {0,1,1,2,3,5,8,13,21,34}
```

## Unit test coverage

- `test/cpu/cpu6502_branch_test.cpp`: each of the 8 branches — condition
  true/false, taken/not-taken cycle counts, same-page vs. page-crossed
  timing (3 vs. 4 cycles), forward and backward offsets.
- `test/cpu/cpu6502_brk_test.cpp`: stack contents after BRK (pushed PCH,
  PCL, P with B set), `SP` decremented by 3, `IFlag()` set, `PC` loaded from
  `cBRKVector`, `halted()` becomes true; also covers `run()` stopping on
  `halted()` and returning `false` when its instruction cap is exhausted.
- `test/cpu/cpu6502_load_store_test.cpp`: LDA immediate/zero page (value,
  Z/N flags, C left untouched, cycle counts), STA zero page/absolute,Y
  (memory write correctness, no flags touched, fixed 5-cycle timing for
  absolute,Y regardless of page crossing).
- `test/utils/program_loader_test.cpp`: valid hex loads correctly into a
  `RAM`, whitespace-separated and unseparated input both work, odd-length
  and non-hex-character input throw `std::invalid_argument`.
- `test/cpu/cpu6502_fibonacci_e2e_test.cpp`: the Fibonacci program described
  above, asserting on both the halted return value and the resulting memory
  contents.

## Out of scope

- CLC/SEC and other flag-control opcodes — not needed by the e2e program
  (see the load/store section above for why) and not otherwise requested.
- Any further load/store addressing modes or registers (LDX/LDY/STX/STY,
  other LDA/STA addressing modes) beyond the four opcodes listed above.
- NMI/IRQ hardware-pin interrupt handling (only the BRK software interrupt
  path is implemented).
- RTI (return from interrupt) — not needed since the e2e program never
  resumes after BRK; can follow in a later change once PHA/PLA/JSR/RTS
  establish more stack-pull precedent.
- Loading a program through `Bus` address translation (multi-device
  programs) — the loader targets a single `MemoryDevice` directly.
