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

1. Computes the Fibonacci sequence 0, 1, 1, 2, 3, 5, 8, 13, 21, 34 using `A`
   and a zero-page temp for the running previous/current values.
2. Stores each of the 10 values to `$0200..$0209` via an indexed store
   (`STA $0200,Y` with `Y` as the loop counter).
3. Loops using `INY` / `CPY #10` / `BNE` — this is what exercises the new
   branch instructions end-to-end, not just in isolation.
4. Ends with `BRK`.

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
  `cBRKVector`, `halted()` becomes true.
- `test/utils/program_loader_test.cpp`: valid hex loads correctly into a
  `RAM`, whitespace-separated and unseparated input both work, odd-length
  and non-hex-character input throw `std::invalid_argument`.
- `test/cpu/cpu6502_fibonacci_e2e_test.cpp`: the Fibonacci program described
  above, asserting on both the halted return value and the resulting memory
  contents.

## Out of scope

- NMI/IRQ hardware-pin interrupt handling (only the BRK software interrupt
  path is implemented).
- RTI (return from interrupt) — not needed since the e2e program never
  resumes after BRK; can follow in a later change once PHA/PLA/JSR/RTS
  establish more stack-pull precedent.
- Loading a program through `Bus` address translation (multi-device
  programs) — the loader targets a single `MemoryDevice` directly.
