# CPU Identifier Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every concrete `CPU` implementation a runtime-queryable `CpuId`, and lock `CPU6502` in as the frozen reference implementation that a future wide-bus/CISC-like variant will be verified against.

**Architecture:** Add a `CpuId` enum and a pure-virtual `id()` accessor to the abstract `CPU` interface (`src/cpu/cpu.h`); implement it in `CPU6502` (`src/cpu/cpu6502.h`/`.cpp`) returning `CpuId::Mos6502`. Mark `CPU6502` frozen via a class-level doc comment and a new root-level `CLAUDE.md` project instruction.

**Tech Stack:** C++17, GoogleTest/CTest, CMake.

## Global Constraints

- `CpuId` enum lives in `src/cpu/cpu.h`, underlying type `uint8_t`, one enumerator today: `Mos6502`.
- `CPU::id()` is pure virtual (`virtual CpuId id() const = 0;`).
- `CPU6502::id()` returns `CpuId::Mos6502`, implemented in `cpu6502.cpp` (declaration-only in the header, matching the existing getter style — see `A()`/`X()`/`PC()`).
- `CPU6502` must not be modified going forward except for verified bug fixes in its existing 6502 behavior — this is recorded in both a class-level doc comment and `CLAUDE.md`, and this plan's own edits to `cpu6502.h`/`.cpp` are additive-only (new declarations/definitions), never a change to existing 6502 behavior.

---

### Task 1: `CpuId` enum and `id()` accessor

**Files:**
- Modify: `src/cpu/cpu.h`
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_test.cpp`

**Interfaces:**
- Produces: `enum class CpuId : uint8_t { Mos6502 };` (in `src/cpu/cpu.h`, at namespace scope, above `class CPU`).
- Produces: `virtual CpuId id() const = 0;` on `CPU`.
- Produces: `CpuId CPU6502::id() const` returning `CpuId::Mos6502`.

- [ ] **Step 1: Write the failing test**

Open `test/cpu/cpu6502_test.cpp`. Add this test right after the existing
`TEST(CPU6502Smoke, GTestWiringWorks)` line (which is right after the
`CPU6502Test` fixture class closes):

```cpp
TEST_F(CPU6502Test, IdReturnsMos6502) { EXPECT_EQ(cpu.id(), CpuId::Mos6502); }
```

- [ ] **Step 2: Run test to verify it fails to compile**

Run: `cmake --build build --target sys6_tests`
Expected: FAIL — `CpuId` and/or `id` not declared in this scope.

- [ ] **Step 3: Add `CpuId` and `CPU::id()` to the interface**

In `src/cpu/cpu.h`, add the enum above `class CPU` and the pure-virtual
accessor as the first member of the public section:

```cpp
#pragma once

#include "utils/log.h"

enum class CpuId : uint8_t { Mos6502 };

class CPU {
public:
    virtual CpuId id() const = 0;
    virtual void reset() = 0;
    virtual void executeInstruction() = 0;

    bool Debug() const { return m_debug; }
    void Debug(bool val) { m_debug = val; }
    bool Tracing() const { return m_tracing; }
    void Tracing(bool val) { m_tracing = val; }
    void setLogger(Logger *logger) { m_logger = logger; }

protected:
    bool m_debug;
    bool m_tracing;

    Logger *m_logger = nullptr;
};
```

- [ ] **Step 4: Declare `CPU6502::id()` in the header**

In `src/cpu/cpu6502.h`, add the override declaration directly above the
`//-------------------------------------- // CPU processor register
getters/setters` comment block (i.e. right after `void runToClockHigh();`
and before that comment):

```cpp
    void runToClockHigh();

    CpuId id() const override;

    //--------------------------------------
    // CPU processor register getters/setters

    uint8_t A() const;
```

- [ ] **Step 5: Implement `CPU6502::id()` in the source file**

In `src/cpu/cpu6502.cpp`, add the definition directly above
`uint8_t CPU6502::A() const { return m_A; }`:

```cpp
CPU6502::CPU6502(Bus &bus) : m_bus(bus) {}

CpuId CPU6502::id() const { return CpuId::Mos6502; }

uint8_t CPU6502::A() const { return m_A; }
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R CPU6502Test.IdReturnsMos6502`
Expected: PASS (1 test run, 0 failures).

- [ ] **Step 7: Run the full test suite to confirm no regressions**

Run: `ctest --test-dir build --output-on-failure`
Expected: All tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/cpu/cpu.h src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_test.cpp
git commit -m "feat: add CpuId enum and CPU::id() accessor"
```

---

### Task 2: Freeze `CPU6502` as the reference implementation

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Create: `CLAUDE.md`

**Interfaces:**
- Consumes: `CpuId::Mos6502` and `CPU6502::id()` from Task 1 (referenced in the doc comment/CLAUDE.md prose, not called).
- Produces: none (documentation only).

- [ ] **Step 1: Add the freeze doc comment to `CPU6502`**

In `src/cpu/cpu6502.h`, add this comment directly above
`class CPU6502 : public CPU {`:

```cpp
// CPU6502 is the frozen, cycle-accurate reference implementation of the
// MOS 6502 ISA (CpuId::Mos6502). It is the golden model a future
// wider-bus, CISC-like CPU variant will be verified against. Changes to
// this class should be limited to verified bug fixes in its existing
// 6502 behavior -- never modified to make room for a new variant. New
// variants implement the CPU interface in their own class/files with
// their own CpuId enumerator.
class CPU6502 : public CPU {
```

- [ ] **Step 2: Create `CLAUDE.md`**

Create `CLAUDE.md` at the repo root with this content:

```markdown
# Project instructions

## Reference CPU implementation

`src/cpu/cpu6502.h`/`cpu6502.cpp` (`CPU6502`, `CpuId::Mos6502`) is the
frozen, cycle-accurate reference implementation of the MOS 6502 ISA. A
future wider-bus, CISC-like CPU variant will be verified against it.

Do not modify `CPU6502` to accommodate a new CPU variant. Only change it
for verified bug fixes in its existing 6502 behavior. New variants
implement the `CPU` interface (`src/cpu/cpu.h`) in their own class/files,
with their own `CpuId` enumerator.
```

- [ ] **Step 3: Verify the build still succeeds**

Run: `cmake --build build`
Expected: builds cleanly (this task only added comments/a new doc file, no
behavioral change).

- [ ] **Step 4: Commit**

```bash
git add src/cpu/cpu6502.h CLAUDE.md
git commit -m "docs: freeze CPU6502 as the reference implementation"
```
