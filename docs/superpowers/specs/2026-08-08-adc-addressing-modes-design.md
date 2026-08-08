# ADC remaining addressing modes — design spec

## Context

`ADC` currently supports two of its eight addressing modes: `#immediate`
(`0x69`) and `absolute` (`0x6D`), built on the cycle-stepped/clock-phase
architecture from
`docs/superpowers/specs/2026-08-08-cycle-stepped-alu-design.md` and
`docs/superpowers/specs/2026-08-08-clock-phase-model-design.md`. Both of
those specs explicitly deferred "remaining addressing modes for ADC and the
rest, including indexed and indirect-indexed modes and their page-crossing
cycle-count quirks" as follow-on work. This spec covers that follow-on: the
six remaining ADC addressing modes.

## Goals

- Implement all six remaining ADC addressing modes: `zero page` (`0x65`),
  `zero page,X` (`0x75`), `absolute,X` (`0x7D`), `absolute,Y` (`0x79`),
  `(indirect,X)` (`0x61`), `(indirect),Y` (`0x71`).
- Match real 6502 cycle counts, including the +1-cycle penalty when indexed
  addressing (`absolute,X`, `absolute,Y`, `(indirect),Y`) crosses a page
  boundary.
- Factor the address-resolution logic (indexed-address math, page-crossing
  detection) so it is reusable by future ALU-shaped opcodes (SBC, AND, ORA,
  EOR, CMP, ...) that share these same addressing modes — only the final ALU
  call differs per opcode.
- Introduce `docs/6502-manual.md`, a living reference of every opcode this
  emulator implements, its addressing mode, byte length, and cycle count
  *as implemented* — including backfilling the two opcodes that already
  exist (`ADC #immediate`, `ADC absolute`). This becomes the standing
  reference for every future opcode, and the place divergences from real
  hardware timing are documented.

## Non-goals

- Modeling the real 6502's "dummy read" at the wrong (uncorrected) address
  during page-crossing fixups, or the equivalent dummy read of the
  unindexed base address in `zero page,X` / `(indirect,X)`. This emulator
  intentionally does not reproduce that hardware quirk: where real hardware
  would issue a throwaway bus read, this implementation spends the same
  cycle idle instead. Total cycle counts still match real hardware; the
  bus-access trace does not. `docs/6502-manual.md` documents this divergence
  explicitly.
- Any ALU op other than ADC (SBC, AND, ORA, EOR, CMP, ...) — those are
  separate follow-on phases, per the prior specs' non-goals.
- A general opcode dispatch table. With one ALU op implemented, a full
  data-driven (addressing-mode × operation) table is premature; the prior
  spec explicitly deferred this until more opcode/mode combinations exist.
  This pass factors only the address-resolution math, not opcode dispatch.
- Decimal (BCD) mode, interrupts, or `reset()` changes — unchanged from
  prior specs.

## Architecture

### New state

```cpp
enum class CpuStep : uint8_t { T0, T1, T2, T3, T4, T5 }; // was T0..T3
```

Extended to six states to fit `(indirect,X)`'s 6-cycle timing.

New protected members on `CPU6502`:

- `uint16_t m_effAddr` — the final resolved effective address for
  indexed/indirect modes. Kept separate from `m_addrLatch`, which continues
  to hold whatever intermediate address (a base address, or a zero-page
  pointer-table address) is being read from *this* cycle. Keeping these
  distinct avoids overwriting a pointer address that's still needed for a
  second byte read (e.g. `(indirect,X)`'s pointer-high-byte fetch).
- `bool m_pageCrossed` — set when an indexed-address computation crosses a
  page boundary; consulted one cycle later to decide whether the next cycle
  is a real read or an idle fixup cycle.

### Shared address-resolution helper

```cpp
struct EffectiveAddress {
    uint16_t address;
    bool pageCrossed;
};

EffectiveAddress indexedAddress(uint16_t base, uint8_t index) const;
```

Pure computation, no bus/register access: `address = base + index` (as
`uint16_t`, wrapping at 0xFFFF the same way the real 16-bit address bus
does), `pageCrossed = (base & 0xFF00) != (address & 0xFF00)`. This is the
piece every future indexed ALU opcode reuses — it has no ADC-specific
knowledge. Each opcode's capture/commit pair calls this, then falls into
the existing `loadAluInputs(operand)` / `commitAluResult()` pair (already
opcode-agnostic) to apply whatever the addressing mode fetched.

Per-opcode capture/commit functions stay named and dispatched the way
`captureADCImmediate`/`captureADCAbsolute` already are today (a `switch`
on `m_IR` in `onClockHigh`/`onClockLow`) — this pass does not introduce a
generic dispatch table (see Non-goals).

### Page-crossing cycle model

When a capture step computes `m_pageCrossed = true`, the immediately
following cycle does no bus access at all (idle) instead of the real
hardware's wrong-address dummy read; the cycle after that performs the real
read at `m_effAddr` and completes the instruction. This keeps total cycle
counts identical to real hardware for every addressing mode while avoiding
any bus access that doesn't reflect the instruction's actual data
dependencies.

### Zero-page wraparound

Zero-page-relative addressing (`zero page,X`, and the pointer-table/pointer
bytes of both indirect modes) always wraps within page 0 via `& 0xFF` —
never crosses a page boundary, and never incurs the +1-cycle penalty. This
applies to: the indexed pointer address in `(indirect,X)`
(`(bb + X) & 0xFF`), and the pointer high-byte fetch in both indirect modes
(`(ptrAddr + 1) & 0xFF`).

## Opcode cycle tables

Capture runs on clock `High`, reading from the bus into latches. Commit
runs on clock `Low`, writing latches/`PC`/`CpuStep`. This mirrors the
existing `ADC #immediate`/`ADC absolute` tables.

**`ADC zero page` (`0x65`, 2 bytes, 3 cycles, fixed)**

| T | Capture | Commit |
|---|---|---|
| T1 | read `bb` at `PC` → `m_addrLatch` | `PC++`; `T2` |
| T2 | `loadAluInputs(bus.read(m_addrLatch))` | `commitAluResult()`; `T0` |

**`ADC zero page,X` (`0x75`, 2 bytes, 4 cycles, fixed)**

| T | Capture | Commit |
|---|---|---|
| T1 | read `bb` → `m_addrLatch` | `PC++`; `T2` |
| T2 | idle: `m_addrLatch = (m_addrLatch + X) & 0xFF` | `T3` |
| T3 | `loadAluInputs(bus.read(m_addrLatch))` | `commitAluResult()`; `T0` |

**`ADC absolute,X` (`0x7D`, 3 bytes, 4 cycles, +1 if page crossed)**
— `ADC absolute,Y` (`0x79`) is identical with `Y` substituted for `X`.

| T | Capture | Commit |
|---|---|---|
| T1 | read low byte → `m_addrLatch` | `PC++`; `T2` |
| T2 | read high byte, combine with `m_addrLatch` → `base`; `{m_effAddr, m_pageCrossed} = indexedAddress(base, X)` | `PC++`; `T3` |
| T3 | if `!m_pageCrossed`: `loadAluInputs(bus.read(m_effAddr))`; else idle | if `!m_pageCrossed`: `commitAluResult()`, `T0`; else `T4` |
| T4 *(crossed only)* | `loadAluInputs(bus.read(m_effAddr))` | `commitAluResult()`; `T0` |

**`ADC (indirect,X)` (`0x61`, 2 bytes, 6 cycles, fixed)**

| T | Capture | Commit |
|---|---|---|
| T1 | read `bb` → `m_addrLatch` | `PC++`; `T2` |
| T2 | idle: `m_addrLatch = (m_addrLatch + X) & 0xFF` | `T3` |
| T3 | `m_effAddr = bus.read(m_addrLatch)` (ptr low) | `T4` |
| T4 | `m_effAddr \|= bus.read((m_addrLatch + 1) & 0xFF) << 8` (ptr high, zero-page wrap) | `T5` |
| T5 | `loadAluInputs(bus.read(m_effAddr))` | `commitAluResult()`; `T0` |

**`ADC (indirect),Y` (`0x71`, 2 bytes, 5 cycles, +1 if page crossed)**

| T | Capture | Commit |
|---|---|---|
| T1 | read `bb` → `m_addrLatch` | `PC++`; `T2` |
| T2 | `m_effAddr = bus.read(m_addrLatch)` (ptr low) | `T3` |
| T3 | `hi = bus.read((m_addrLatch + 1) & 0xFF)` (zero-page wrap); `base = m_effAddr \| (hi << 8)`; `{m_effAddr, m_pageCrossed} = indexedAddress(base, Y)` | `T4` |
| T4 | if `!m_pageCrossed`: `loadAluInputs(bus.read(m_effAddr))`; else idle | if `!m_pageCrossed`: `commitAluResult()`, `T0`; else `T5` |
| T5 *(crossed only)* | `loadAluInputs(bus.read(m_effAddr))` | `commitAluResult()`; `T0` |

## `docs/6502-manual.md`

A new top-level (not `superpowers/`) reference doc, since it's a living
artifact maintained across every future opcode pass, not a point-in-time
design record. One table, one row per opcode variant:

| Opcode | Mnemonic | Addressing mode | Bytes | Cycles (as implemented) | Notes |
|---|---|---|---|---|---|

Backfilled with the two existing opcodes (`0x69`, `0x6D`) plus the six
added by this pass. The "Notes" column is where hardware-timing divergences
(the no-dummy-read decision above) get called out per affected row, so
anyone reading the manual — not just this spec — can see where the
emulator departs from real 6502 behavior.

## Compatibility

- Public API (`executeInstruction()`, register/flag getters/setters,
  `reset()`) is unchanged.
- Existing `ADC #immediate` / `ADC absolute` tests and behavior are
  unaffected; `CpuStep`'s widened range (`T4`, `T5`) is only ever reached by
  the new opcodes.
- The unimplemented-opcode fallback (1-cycle no-op) is unchanged and still
  covers every opcode not listed above.

## Testing plan

Extends `test/cpu/cpu6502_adc_test.cpp` (matching its existing style) with,
per new addressing mode:

- Basic-correctness test: operand fetched via that addressing mode is added
  to `A` correctly, `PC` advances by the opcode's byte length.
- Tick-level cycle-count test: the fixed-cycle modes (`zero page`,
  `zero page,X`, `(indirect,X)`) always complete in their documented cycle
  count; the variable-cycle modes (`absolute,X`, `absolute,Y`,
  `(indirect),Y`) complete in the base cycle count on a same-page access and
  exactly one more on a page-crossing access.
- Zero-page-wraparound test for both indirect modes: a pointer byte at
  `0xFF` wraps to `0x00` (not `0x100`) when read as `(ptrAddr + 1) & 0xFF`,
  and `(indirect,X)`'s indexed pointer address wraps the same way when
  `bb + X` exceeds `0xFF`.

`docs/6502-manual.md` is written/updated as part of this same pass, not as
a separate follow-up.

## Follow-on work (not this pass)

- Remaining ALU-shaped ops (SBC, AND, ORA, EOR, ASL, LSR, ROL, ROR, CMP,
  CPX, CPY, INC, DEC, INX, DEX, INY, DEY), each reusing `indexedAddress()`
  and the addressing-mode cycle tables established here.
- Decimal (BCD) mode for ADC/SBC.
- A general opcode dispatch table, once enough opcode/addressing-mode
  combinations exist to make the current per-opcode `switch` unwieldy.
