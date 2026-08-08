# CPU identifier + frozen reference implementation — design spec

## Context

`CPU6502` is currently the only concrete implementation of the abstract
`CPU` interface (`src/cpu/cpu.h`). A future variant is planned — a
wider-bus (data/address width increased beyond 8/16-bit), CISC-like
successor sharing the same `CPU` interface. Before that work starts,
`CPU6502` needs to be locked in as the golden/reference model that the new
variant is verified against, and any `CPU`-holding code needs a way to
tell, at runtime, which concrete implementation it's driving.

## Goals

- Add a `CpuId` enum identifying concrete `CPU` implementations, with one
  enumerator today: `CpuId::Mos6502`.
- Add `virtual CpuId id() const = 0;` to the `CPU` interface, implemented
  by `CPU6502::id()` returning `CpuId::Mos6502`.
- Mark `CPU6502` as the frozen reference implementation via a class-level
  doc comment: it emulates the MOS 6502 ISA cycle-accurately and must not
  be modified to accommodate the future wide-bus/CISC-like variant — only
  for verified bug fixes in its existing 6502 behavior.
- Add `CLAUDE.md` at the repo root recording this rule as a durable
  project instruction, so it survives across sessions: new CPU variants
  get their own class/files implementing `CPU` (and their own `CpuId`
  enumerator) rather than edits to `CPU6502`.
- One test asserting `CPU6502{...}.id() == CpuId::Mos6502`.

## Non-goals

- Building the wide-bus/CISC-like variant itself. This spec only adds the
  identifier and the freeze convention that variant will rely on.
- Any comparison/diffing harness between implementations. Out of scope
  until a second `CPU` implementation actually exists.
- Relocating `CPU6502`'s files. The freeze is enforced by doc comment +
  `CLAUDE.md` instruction, not by directory structure.

## Architecture

### `CpuId` enum (src/cpu/cpu.h)

```cpp
enum class CpuId : uint8_t { Mos6502 };

class CPU {
public:
    virtual CpuId id() const = 0;
    virtual void reset() = 0;
    virtual void executeInstruction() = 0;
    ...
};
```

`id()` sits alongside the other pure-virtual interface methods so any code
holding a `CPU&`/`CPU*` — drivers, tests, a future comparison harness — can
identify the concrete implementation without downcasting.

### `CPU6502::id()` (src/cpu/cpu6502.h + cpu6502.cpp)

Declared in the getter block in the header (next to `A()`, `X()`, `PC()`,
etc.):

```cpp
CpuId id() const override;
```

Implemented in `cpu6502.cpp` next to the other simple getters (e.g. right
after `uint8_t CPU6502::A() const { return m_A; }`):

```cpp
CpuId CPU6502::id() const { return CpuId::Mos6502; }
```

### Freeze marker on `CPU6502`

A class-level doc comment directly above `class CPU6502 : public CPU {`
in `cpu6502.h`, stating:

- `CPU6502` is the frozen, cycle-accurate reference implementation of the
  MOS 6502 ISA.
- It is the golden model a future wider-bus/CISC-like `CPU` variant will
  be verified against.
- Changes to this class should be limited to verified bug fixes in its
  existing 6502 behavior — never modified to make room for a new variant.

### `CLAUDE.md`

New file at the repo root (none exists yet). Content: a short project
instruction recording the same rule from the doc comment — `CPU6502` is
the reference implementation and must not be modified to support new CPU
variants; new variants implement `CPU` in their own class/files with
their own `CpuId` enumerator.

## Testing

Add one assertion to `test/cpu/cpu6502_test.cpp` (a new
`CPU6502Smoke`-style case, alongside the existing
`CPU6502Smoke.GTestWiringWorks`):

- `IdReturnsMos6502` — constructs a `CPU6502` and asserts
  `cpu.id() == CpuId::Mos6502`.
