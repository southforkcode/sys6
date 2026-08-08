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
| `0xE9` | SBC | Immediate | 2 | 2 | |
| `0xE5` | SBC | Zero Page | 2 | 3 | |
| `0xF5` | SBC | Zero Page,X | 2 | 4 | No dummy read — see below |
| `0xED` | SBC | Absolute | 3 | 4 | |
| `0xFD` | SBC | Absolute,X | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0xF9` | SBC | Absolute,Y | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0xE1` | SBC | (Indirect,X) | 2 | 6 | No dummy read — see below |
| `0xF1` | SBC | (Indirect),Y | 2 | 5 (+1 if page crossed) | No dummy read — see below |
| `0x29` | AND | Immediate | 2 | 2 | |
| `0x25` | AND | Zero Page | 2 | 3 | |
| `0x35` | AND | Zero Page,X | 2 | 4 | No dummy read — see below |
| `0x2D` | AND | Absolute | 3 | 4 | |
| `0x3D` | AND | Absolute,X | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x39` | AND | Absolute,Y | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x21` | AND | (Indirect,X) | 2 | 6 | No dummy read — see below |
| `0x31` | AND | (Indirect),Y | 2 | 5 (+1 if page crossed) | No dummy read — see below |
| `0x09` | ORA | Immediate | 2 | 2 | |
| `0x05` | ORA | Zero Page | 2 | 3 | |
| `0x15` | ORA | Zero Page,X | 2 | 4 | No dummy read — see below |
| `0x0D` | ORA | Absolute | 3 | 4 | |
| `0x1D` | ORA | Absolute,X | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x19` | ORA | Absolute,Y | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x01` | ORA | (Indirect,X) | 2 | 6 | No dummy read — see below |
| `0x11` | ORA | (Indirect),Y | 2 | 5 (+1 if page crossed) | No dummy read — see below |
| `0x49` | EOR | Immediate | 2 | 2 | |
| `0x45` | EOR | Zero Page | 2 | 3 | |
| `0x55` | EOR | Zero Page,X | 2 | 4 | No dummy read — see below |
| `0x4D` | EOR | Absolute | 3 | 4 | |
| `0x5D` | EOR | Absolute,X | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x59` | EOR | Absolute,Y | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0x41` | EOR | (Indirect,X) | 2 | 6 | No dummy read — see below |
| `0x51` | EOR | (Indirect),Y | 2 | 5 (+1 if page crossed) | No dummy read — see below |
| `0xC9` | CMP | Immediate | 2 | 2 | |
| `0xC5` | CMP | Zero Page | 2 | 3 | |
| `0xD5` | CMP | Zero Page,X | 2 | 4 | No dummy read — see below |
| `0xCD` | CMP | Absolute | 3 | 4 | |
| `0xDD` | CMP | Absolute,X | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0xD9` | CMP | Absolute,Y | 3 | 4 (+1 if page crossed) | No dummy read — see below |
| `0xC1` | CMP | (Indirect,X) | 2 | 6 | No dummy read — see below |
| `0xD1` | CMP | (Indirect),Y | 2 | 5 (+1 if page crossed) | No dummy read — see below |
| `0xE0` | CPX | Immediate | 2 | 2 | |
| `0xE4` | CPX | Zero Page | 2 | 3 | |
| `0xEC` | CPX | Absolute | 3 | 4 | |
| `0xC0` | CPY | Immediate | 2 | 2 | |
| `0xC4` | CPY | Zero Page | 2 | 3 | |
| `0xCC` | CPY | Absolute | 3 | 4 | |
| `0x0A` | ASL | Accumulator | 1 | 2 | |
| `0x06` | ASL | Zero Page | 2 | 5 | No dummy write — see below |
| `0x16` | ASL | Zero Page,X | 2 | 6 | No dummy read/write — see below |
| `0x0E` | ASL | Absolute | 3 | 6 | No dummy write — see below |
| `0x1E` | ASL | Absolute,X | 3 | 7 (fixed) | No dummy reads/write; not page-cross-conditional — see below |
| `0x4A` | LSR | Accumulator | 1 | 2 | |
| `0x46` | LSR | Zero Page | 2 | 5 | No dummy write — see below |
| `0x56` | LSR | Zero Page,X | 2 | 6 | No dummy read/write — see below |
| `0x4E` | LSR | Absolute | 3 | 6 | No dummy write — see below |
| `0x5E` | LSR | Absolute,X | 3 | 7 (fixed) | No dummy reads/write; not page-cross-conditional — see below |
| `0x2A` | ROL | Accumulator | 1 | 2 | |
| `0x26` | ROL | Zero Page | 2 | 5 | No dummy write — see below |
| `0x36` | ROL | Zero Page,X | 2 | 6 | No dummy read/write — see below |
| `0x2E` | ROL | Absolute | 3 | 6 | No dummy write — see below |
| `0x3E` | ROL | Absolute,X | 3 | 7 (fixed) | No dummy reads/write; not page-cross-conditional — see below |
| `0x6A` | ROR | Accumulator | 1 | 2 | |
| `0x66` | ROR | Zero Page | 2 | 5 | No dummy write — see below |
| `0x76` | ROR | Zero Page,X | 2 | 6 | No dummy read/write — see below |
| `0x6E` | ROR | Absolute | 3 | 6 | No dummy write — see below |
| `0x7E` | ROR | Absolute,X | 3 | 7 (fixed) | No dummy reads/write; not page-cross-conditional — see below |
| `0xE6` | INC | Zero Page | 2 | 5 | No dummy write — see below |
| `0xF6` | INC | Zero Page,X | 2 | 6 | No dummy read/write — see below |
| `0xEE` | INC | Absolute | 3 | 6 | No dummy write — see below |
| `0xFE` | INC | Absolute,X | 3 | 7 (fixed) | No dummy reads/write; not page-cross-conditional — see below |
| `0xC6` | DEC | Zero Page | 2 | 5 | No dummy write — see below |
| `0xD6` | DEC | Zero Page,X | 2 | 6 | No dummy read/write — see below |
| `0xCE` | DEC | Absolute | 3 | 6 | No dummy write — see below |
| `0xDE` | DEC | Absolute,X | 3 | 7 (fixed) | No dummy reads/write; not page-cross-conditional — see below |
| `0xE8` | INX | Implied | 1 | 2 | |
| `0xCA` | DEX | Implied | 1 | 2 | |
| `0xC8` | INY | Implied | 1 | 2 | |
| `0x88` | DEY | Implied | 1 | 2 | |

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

**No dummy-write cycles (read-modify-write instructions).** Real 6502
read-modify-write instructions (`ASL`/`LSR`/`ROL`/`ROR`/`INC`/`DEC` in every
non-`Accumulator` mode) write the *unmodified* value back to memory as a
"dummy write" before writing the real, modified value on the following
cycle — a genuine, observable bus write, not just a read. This emulator
does not reproduce it: that cycle is spent idle, and only the final,
modified value is ever written. Total cycle counts still match real
hardware exactly (5/6/6/7 for zero page / zero page,X / absolute /
absolute,X). Same rationale and same divergence policy as the no-dummy-read
decision above — see
`docs/superpowers/specs/2026-08-08-remaining-alu-opcodes-design.md`.

**Read-modify-write `Absolute,X` timing is fixed, not page-cross-conditional.**
Unlike the read-only ALU family (`ADC`, `SBC`, `AND`, `ORA`, `EOR`, `CMP`),
where `Absolute,X`/`Absolute,Y`/`(Indirect),Y` cost one extra cycle only
when indexing crosses a page boundary, real 6502 read-modify-write
instructions always spend that extra cycle on `Absolute,X` regardless of
whether the index actually crosses a page. This emulator matches that
real-hardware behavior exactly (it is not a divergence) — `Absolute,X` for
`ASL`/`LSR`/`ROL`/`ROR`/`INC`/`DEC` is always 7 cycles.
