# 6502 manual (as implemented)

This is a living reference of every opcode this emulator implements: its
addressing mode, instruction length, and cycle count *as this codebase
implements it* — not always identical to real 6502 hardware. See
"Divergences from real hardware" below for where and why. Every future
opcode-implementation pass adds its rows here.

| Opcode | Mnemonic | Addressing mode | Bytes | Cycles (as implemented) | Notes |
|---|---|---|---|---|---|
| `0x69` | ADC | Immediate | 2 | 2 | |
| `0x65` | ADC | Zero Page | 2 | 3 | |
| `0x75` | ADC | Zero Page,X | 2 | 4 | No dummy read — see below |
| `0x6D` | ADC | Absolute | 3 | 4 | |
| `0x7D` | ADC | Absolute,X | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x79` | ADC | Absolute,Y | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x61` | ADC | (Indirect,X) | 2 | 6 | No dummy read — see below |
| `0x71` | ADC | (Indirect),Y | 2 | 5 (+1 if page crossed) | No dummy read — see below |

## Divergences from real hardware

**No dummy-read cycles.** Real 6502 hardware fills certain cycles with a
bus read whose result is discarded — a "dummy read" — purely for timing:
the wrong (uncorrected) address on a page-crossing fixup, and the
unindexed base address before zero-page/indirect indexing is applied. This
emulator does not reproduce those reads: the equivalent cycle is spent
idle, with no bus access at all. Total cycle counts still match real
hardware exactly; only the bus-access trace within a multi-cycle
instruction differs. This is a deliberate simplification, not a bug — see
`docs/superpowers/specs/2026-08-08-adc-addressing-modes-design.md` for the
full rationale. It matters only for hardware that reacts to reads as a
side effect (e.g. certain memory-mapped I/O registers); nothing in this
emulator currently does.
