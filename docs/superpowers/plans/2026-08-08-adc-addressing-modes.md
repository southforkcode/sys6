# ADC Remaining Addressing Modes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the six remaining ADC addressing modes (zero page, zero page,X, absolute,X, absolute,Y, (indirect,X), (indirect),Y) with correct operand fetch, cycle-accurate timing including page-crossing penalties, and a reusable address-resolution helper, plus a living `docs/6502-manual.md` reference of every implemented opcode's cycle count.

**Architecture:** Each addressing mode gets a `captureADCXxx()`/`commitADCXxx()` pair dispatched from the existing `onClockHigh()`/`onClockLow()` switch on `m_IR`, following the capture-on-High/commit-on-Low pattern already used by `ADC #immediate` and `ADC absolute`. A new protected helper `indexedAddress(base, index)` centralizes indexed-address math and page-crossing detection so it's reusable by future ALU-shaped opcodes (SBC, AND, ...). `CpuStep` widens from `{T0..T3}` to `{T0..T5}` to fit `(indirect,X)`'s 6-cycle timing. Per the design spec, no opcode models the real 6502's "dummy read" quirk (the wrong-address read during page-crossing fixups, or the throwaway base-address read before indexing) — those cycles are spent idle instead, with identical total cycle counts.

**Tech Stack:** C++17, CMake + CTest, GoogleTest (already wired via `FetchContent`).

## Global Constraints

- C++17, matching `CMakeLists.txt` (`CMAKE_CXX_STANDARD 17`).
- Follow the existing explicit-source-list pattern in `test/CMakeLists.txt` — no globbing for build sources.
- Naming: PascalCase class/struct names, `m_`-prefixed member variables, lowerCamelCase methods — matches `CPU6502`/`ALU`/`captureADCAbsolute`/`m_addrLatch` already in the codebase.
- No opcode in this plan models a real 6502 "dummy read": where hardware would issue a throwaway bus read (page-crossing fixup, or the base-address read before zero-page indexing), this implementation does nothing on the bus that cycle. Total cycle counts still match real hardware.
- `indexedAddress()` is a pure computation (no bus/register access) so it can be unit-tested directly and reused by future opcodes.
- Every new test file/source file follows the existing `src/cpu/` + `test/cpu/` layout; new test files are added to `test/CMakeLists.txt`'s explicit source list.
- Reference design doc: `docs/superpowers/specs/2026-08-08-adc-addressing-modes-design.md`. Cycle tables and architecture in this plan implement that spec exactly; consult it if a step here seems ambiguous.

---

### Task 1: `indexedAddress()` helper and `CpuStep`/latch widening

**Files:**
- Create: `test/cpu/cpu6502_addressing_test.cpp`
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces: `struct EffectiveAddress { uint16_t address; bool pageCrossed; };` (file-scope in `cpu6502.h`, alongside `CpuStep`/`ClockPhase`).
- Produces: `EffectiveAddress CPU6502::indexedAddress(uint16_t base, uint8_t index) const` — protected method, reused by Tasks 4, 5, 7.
- Produces: `CpuStep` widened to `{ T0, T1, T2, T3, T4, T5 }` — consumed by every later task.
- Produces: protected members `uint16_t m_effAddr = 0;` and `bool m_pageCrossed = false;` — consumed by Tasks 4, 5, 6, 7.

- [ ] **Step 1: Write the failing test**

Create `test/cpu/cpu6502_addressing_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"

class CPU6502AddressingTestAccess : public CPU6502 {
public:
    using CPU6502::CPU6502;
    using CPU6502::indexedAddress;
};

class CPU6502AddressingTest : public ::testing::Test {
protected:
    RAM ram{0x10000};
    Bus bus;
    CPU6502AddressingTestAccess cpu{bus};

    CPU6502AddressingTest() { bus.attach(0x0000, 0xFFFF, ram); }
};

TEST_F(CPU6502AddressingTest, IndexedAddressAddsIndexToBaseWithinSamePage) {
    EffectiveAddress result = cpu.indexedAddress(0x0200, 0x05);

    EXPECT_EQ(result.address, 0x0205);
    EXPECT_FALSE(result.pageCrossed);
}

TEST_F(CPU6502AddressingTest, IndexedAddressDetectsPageCrossing) {
    EffectiveAddress result = cpu.indexedAddress(0x02FF, 0x01);

    EXPECT_EQ(result.address, 0x0300);
    EXPECT_TRUE(result.pageCrossed);
}

TEST_F(CPU6502AddressingTest, IndexedAddressWrapsAt16BitBoundary) {
    EffectiveAddress result = cpu.indexedAddress(0xFFFF, 0x01);

    EXPECT_EQ(result.address, 0x0000);
}

TEST_F(CPU6502AddressingTest, IndexedAddressWithZeroIndexNeverCrosses) {
    EffectiveAddress result = cpu.indexedAddress(0x0200, 0x00);

    EXPECT_EQ(result.address, 0x0200);
    EXPECT_FALSE(result.pageCrossed);
}
```

Modify `test/CMakeLists.txt`: add `cpu/cpu6502_addressing_test.cpp` to the `add_executable(sys6_tests ...)` source list, alongside `cpu/cpu6502_adc_test.cpp`.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R CPU6502Addressing`
Expected: build FAILS — `indexedAddress` and `EffectiveAddress` are not declared.

- [ ] **Step 3: Implement the helper**

In `src/cpu/cpu6502.h`, add the struct before `class CPU6502`, next to the existing `CpuStep`/`ClockPhase` enums:

```cpp
struct EffectiveAddress {
    uint16_t address;
    bool pageCrossed;
};
```

Widen `CpuStep`:

```cpp
enum class CpuStep : uint8_t { T0, T1, T2, T3, T4, T5 };
```

Add two new protected members immediately after the existing `uint16_t m_addrLatch = 0;`:

```cpp
uint16_t m_effAddr = 0;
bool m_pageCrossed = false;
```

Add the protected method declaration immediately after the ALU wiring members (after `AluResult m_aluOutput{};`), before the `private:` section:

```cpp
EffectiveAddress indexedAddress(uint16_t base, uint8_t index) const;
```

In `src/cpu/cpu6502.cpp`, add the implementation (e.g. right before `CPU6502::captureOpcodeFetch`):

```cpp
EffectiveAddress CPU6502::indexedAddress(uint16_t base, uint8_t index) const {
    auto address = static_cast<uint16_t>(base + index);
    bool pageCrossed = (base & 0xFF00) != (address & 0xFF00);
    return EffectiveAddress{address, pageCrossed};
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build -R CPU6502Addressing`
Expected: PASS (all 4 tests).

- [ ] **Step 5: Run the full test suite to confirm no regressions**

Run: `ctest --test-dir build`
Expected: all existing tests still PASS (widening `CpuStep` and adding unused members must not change any existing opcode's behavior).

- [ ] **Step 6: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_addressing_test.cpp test/CMakeLists.txt
git commit -m "feat: add indexedAddress() helper and widen CpuStep for new addressing modes"
```

---

### Task 2: `ADC zero page` (`0x65`)

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Consumes: `loadAluInputs(uint8_t operand)`, `commitAluResult()` (existing, opcode-agnostic).
- Produces: `void captureADCZeroPage(); void commitADCZeroPage();` (private methods, dispatched by `m_IR == 0x65`).

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_adc_test.cpp`:

```cpp
TEST_F(CPU6502AdcTest, AdcZeroPageAddsOperandFromMemory) {
    ram.write(0x0000, 0x65);
    ram.write(0x0001, 0x10); // zero page address
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, TwelveTicksCompleteAdcZeroPageWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x65);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.CFlag(false);

    for (int i = 0; i < 8; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.PC(), 0x0002);
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R CPU6502AdcTest.AdcZeroPage`
Expected: `AdcZeroPageAddsOperandFromMemory` FAILS — opcode `0x65` falls to the unimplemented-opcode default case, so `PC` only advances by 1, not 2, and `A` stays `0x10`.

- [ ] **Step 3: Implement zero page addressing**

In `src/cpu/cpu6502.h`, add opcode-specific declarations after `commitADCAbsolute();`:

```cpp
void captureADCZeroPage();
void commitADCZeroPage();
```

In `src/cpu/cpu6502.cpp`, add the opcode constant next to `cOpADCAbsolute`:

```cpp
const uint8_t cOpADCZeroPage = 0x65;
```

Add `case cOpADCZeroPage: captureADCZeroPage(); break;` to the `switch (m_IR)` in `onClockHigh()`, and `case cOpADCZeroPage: commitADCZeroPage(); break;` to the `switch (m_IR)` in `onClockLow()`.

Add the method bodies after `commitADCAbsolute()`:

```cpp
void CPU6502::captureADCZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        loadAluInputs(m_bus.read(m_addrLatch));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}

void CPU6502::commitADCZeroPage() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1 or T2.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build -R CPU6502AdcTest`
Expected: PASS (all ADC tests, including the two new ones).

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: wire ADC zero page through the ALU unit"
```

---

### Task 3: `ADC zero page,X` (`0x75`)

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Consumes: `loadAluInputs`, `commitAluResult` (existing).
- Produces: `void captureADCZeroPageX(); void commitADCZeroPageX();`.

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_adc_test.cpp`:

```cpp
TEST_F(CPU6502AdcTest, AdcZeroPageXAddsOperandUsingIndexedAddress) {
    ram.write(0x0000, 0x75);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x05); // effective address 0x10 + X(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcZeroPageXWrapsWithinZeroPage) {
    ram.write(0x0000, 0x75);
    ram.write(0x0001, 0xFF);
    ram.write(0x0004, 0x07); // effective address wraps: (0xFF + 0x05) & 0xFF = 0x04
    cpu.reset();
    cpu.A(0x01);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x08);
}

TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcZeroPageXWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x75);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    for (int i = 0; i < 12; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R CPU6502AdcTest.AdcZeroPageX`
Expected: FAIL — opcode `0x75` is unimplemented.

- [ ] **Step 3: Implement zero page,X addressing**

In `src/cpu/cpu6502.h`, add after `commitADCZeroPage();`:

```cpp
void captureADCZeroPageX();
void commitADCZeroPageX();
```

In `src/cpu/cpu6502.cpp`, add the opcode constant next to `cOpADCZeroPage`:

```cpp
const uint8_t cOpADCZeroPageX = 0x75;
```

Add `case cOpADCZeroPageX: captureADCZeroPageX(); break;` / `case cOpADCZeroPageX: commitADCZeroPageX(); break;` to the two `switch (m_IR)` blocks.

Add the method bodies after `commitADCZeroPage()`:

```cpp
void CPU6502::captureADCZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch = (m_addrLatch + m_X) & 0xFF;
        break;
    case CpuStep::T3:
        loadAluInputs(m_bus.read(m_addrLatch));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
    }
}

void CPU6502::commitADCZeroPageX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1, T2, or T3.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `ctest --test-dir build -R CPU6502AdcTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: wire ADC zero page,X through the ALU unit"
```

---

### Task 4: `ADC absolute,X` (`0x7D`), including page-crossing

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Consumes: `indexedAddress(uint16_t, uint8_t)` from Task 1, `loadAluInputs`, `commitAluResult`.
- Produces: `void captureADCAbsoluteX(); void commitADCAbsoluteX();`.

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_adc_test.cpp`:

```cpp
TEST_F(CPU6502AdcTest, AdcAbsoluteXAddsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + X(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteXAddsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02); // base address 0x02FF
    ram.write(0x0304, 0x05); // effective address 0x02FF + X(0x05) crosses into page 0x03
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcAbsoluteXWithoutPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    for (int i = 0; i < 12; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T3 completes the instruction: no page crossing

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyTicksCompleteAdcAbsoluteXWithPageCrossing) {
    ram.write(0x0000, 0x7D);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }
    // T0-T3 complete: T3 was the idle page-crossing fixup cycle, A unchanged
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick(); // T4 completes the instruction

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `ctest --test-dir build -R CPU6502AdcTest.AdcAbsoluteX`
Expected: FAIL — opcode `0x7D` is unimplemented.

- [ ] **Step 3: Implement absolute,X addressing**

In `src/cpu/cpu6502.h`, add after `commitADCZeroPageX();`:

```cpp
void captureADCAbsoluteX();
void commitADCAbsoluteX();
```

In `src/cpu/cpu6502.cpp`, add the opcode constant next to `cOpADCZeroPageX`:

```cpp
const uint8_t cOpADCAbsoluteX = 0x7D;
```

Add `case cOpADCAbsoluteX: captureADCAbsoluteX(); break;` / `case cOpADCAbsoluteX: commitADCAbsoluteX(); break;` to the two `switch (m_IR)` blocks.

Add the method bodies after `commitADCZeroPageX()`:

```cpp
void CPU6502::captureADCAbsoluteX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base = static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        EffectiveAddress resolved = indexedAddress(base, m_X);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        if (!m_pageCrossed) {
            loadAluInputs(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T4:
        loadAluInputs(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::commitADCAbsoluteX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_PC++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T4;
        } else {
            commitAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T4:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `ctest --test-dir build -R CPU6502AdcTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: wire ADC absolute,X through the ALU unit with page-crossing timing"
```

---

### Task 5: `ADC absolute,Y` (`0x79`), including page-crossing

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Consumes: `indexedAddress`, `loadAluInputs`, `commitAluResult`.
- Produces: `void captureADCAbsoluteY(); void commitADCAbsoluteY();`.

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_adc_test.cpp`:

```cpp
TEST_F(CPU6502AdcTest, AdcAbsoluteYAddsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02); // base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + Y(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0003);
}

TEST_F(CPU6502AdcTest, AdcAbsoluteYAddsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02); // base address 0x02FF
    ram.write(0x0304, 0x05); // effective address 0x02FF + Y(0x05) crosses into page 0x03
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, SixteenTicksCompleteAdcAbsoluteYWithoutPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0x00);
    ram.write(0x0002, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 12; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyTicksCompleteAdcAbsoluteYWithPageCrossing) {
    ram.write(0x0000, 0x79);
    ram.write(0x0001, 0xFF);
    ram.write(0x0002, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `ctest --test-dir build -R CPU6502AdcTest.AdcAbsoluteY`
Expected: FAIL — opcode `0x79` is unimplemented.

- [ ] **Step 3: Implement absolute,Y addressing**

In `src/cpu/cpu6502.h`, add after `commitADCAbsoluteX();`:

```cpp
void captureADCAbsoluteY();
void commitADCAbsoluteY();
```

In `src/cpu/cpu6502.cpp`, add the opcode constant next to `cOpADCAbsoluteX`:

```cpp
const uint8_t cOpADCAbsoluteY = 0x79;
```

Add `case cOpADCAbsoluteY: captureADCAbsoluteY(); break;` / `case cOpADCAbsoluteY: commitADCAbsoluteY(); break;` to the two `switch (m_IR)` blocks.

Add the method bodies after `commitADCAbsoluteX()` — identical to `captureADCAbsoluteX`/`commitADCAbsoluteX` with `m_Y` substituted for `m_X`:

```cpp
void CPU6502::captureADCAbsoluteY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2: {
        auto base = static_cast<uint16_t>(m_addrLatch | (static_cast<uint16_t>(m_bus.read(m_PC)) << 8));
        EffectiveAddress resolved = indexedAddress(base, m_Y);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T3:
        if (!m_pageCrossed) {
            loadAluInputs(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T4:
        loadAluInputs(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}

void CPU6502::commitADCAbsoluteY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_PC++;
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T4;
        } else {
            commitAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T4:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T4.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `ctest --test-dir build -R CPU6502AdcTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: wire ADC absolute,Y through the ALU unit with page-crossing timing"
```

---

### Task 6: `ADC (indirect,X)` (`0x61`)

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Consumes: `loadAluInputs`, `commitAluResult`.
- Produces: `void captureADCIndirectX(); void commitADCIndirectX();`.

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_adc_test.cpp`:

```cpp
TEST_F(CPU6502AdcTest, AdcIndirectXAddsOperandThroughPointerTable) {
    ram.write(0x0000, 0x61);
    ram.write(0x0001, 0x10); // bb
    ram.write(0x0015, 0x00); // (bb + X) = 0x15 -> pointer low byte
    ram.write(0x0016, 0x02); // pointer high byte -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcIndirectXWrapsPointerTableAddressWithinZeroPage) {
    ram.write(0x0000, 0x61);
    ram.write(0x0001, 0xFF); // bb
    ram.write(0x0004, 0x00); // (0xFF + 0x05) & 0xFF = 0x04 -> pointer low byte
    ram.write(0x0005, 0x02); // pointer high byte -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcIndirectXWrapsPointerHighByteWithinZeroPage) {
    ram.write(0x0300, 0x61);
    ram.write(0x0301, 0xFD); // bb
    ram.write(0x00FF, 0x00); // (bb + X) & 0xFF = 0xFF -> pointer low byte
    ram.write(0x0000, 0x02); // pointer high byte wraps to zero page address 0x00 -> effective address 0x0200
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.PC(0x0300);
    cpu.A(0x10);
    cpu.X(0x02);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyFourTicksCompleteAdcIndirectXWithAUnchangedUntilFinalCycle) {
    ram.write(0x0000, 0x61);
    ram.write(0x0001, 0x10);
    ram.write(0x0015, 0x00);
    ram.write(0x0016, 0x02);
    ram.write(0x0200, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.X(0x05);

    for (int i = 0; i < 20; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `ctest --test-dir build -R CPU6502AdcTest.AdcIndirectX`
Expected: FAIL — opcode `0x61` is unimplemented.

- [ ] **Step 3: Implement (indirect,X) addressing**

In `src/cpu/cpu6502.h`, add after `commitADCAbsoluteY();`:

```cpp
void captureADCIndirectX();
void commitADCIndirectX();
```

In `src/cpu/cpu6502.cpp`, add the opcode constant next to `cOpADCAbsoluteY`:

```cpp
const uint8_t cOpADCIndirectX = 0x61;
```

Add `case cOpADCIndirectX: captureADCIndirectX(); break;` / `case cOpADCIndirectX: commitADCIndirectX(); break;` to the two `switch (m_IR)` blocks.

Add the method bodies after `commitADCAbsoluteY()`:

```cpp
void CPU6502::captureADCIndirectX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_addrLatch = (m_addrLatch + m_X) & 0xFF;
        break;
    case CpuStep::T3:
        m_effAddr = m_bus.read(m_addrLatch);
        break;
    case CpuStep::T4:
        m_effAddr |= static_cast<uint16_t>(m_bus.read((m_addrLatch + 1) & 0xFF)) << 8;
        break;
    case CpuStep::T5:
        loadAluInputs(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitADCIndirectX() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        m_cpuStep = CpuStep::T5;
        break;
    case CpuStep::T5:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `ctest --test-dir build -R CPU6502AdcTest`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: wire ADC (indirect,X) through the ALU unit"
```

---

### Task 7: `ADC (indirect),Y` (`0x71`), including page-crossing

**Files:**
- Modify: `src/cpu/cpu6502.h`
- Modify: `src/cpu/cpu6502.cpp`
- Test: `test/cpu/cpu6502_adc_test.cpp`

**Interfaces:**
- Consumes: `indexedAddress`, `loadAluInputs`, `commitAluResult`.
- Produces: `void captureADCIndirectY(); void commitADCIndirectY();`.

- [ ] **Step 1: Write the failing tests**

Append to `test/cpu/cpu6502_adc_test.cpp`:

```cpp
TEST_F(CPU6502AdcTest, AdcIndirectYAddsOperandWithoutPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10); // bb
    ram.write(0x0010, 0x00); // pointer low byte
    ram.write(0x0011, 0x02); // pointer high byte -> base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + Y(0x05)
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
    EXPECT_EQ(cpu.PC(), 0x0002);
}

TEST_F(CPU6502AdcTest, AdcIndirectYAddsOperandAcrossPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF); // pointer low byte
    ram.write(0x0011, 0x02); // pointer high byte -> base address 0x02FF
    ram.write(0x0304, 0x05); // effective address 0x02FF + Y(0x05) crosses into page 0x03
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);
    cpu.CFlag(false);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, AdcIndirectYWrapsPointerHighByteWithinZeroPage) {
    ram.write(0x0300, 0x71);
    ram.write(0x0301, 0xFF); // bb
    ram.write(0x00FF, 0x00); // pointer low byte
    ram.write(0x0000, 0x02); // pointer high byte wraps to zero page address 0x00 -> base address 0x0200
    ram.write(0x0205, 0x05); // effective address 0x0200 + Y(0x05)
    cpu.reset();
    cpu.PC(0x0300);
    cpu.A(0x10);
    cpu.Y(0x05);

    cpu.executeInstruction();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyTicksCompleteAdcIndirectYWithoutPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0x00);
    ram.write(0x0011, 0x02);
    ram.write(0x0205, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 16; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}

TEST_F(CPU6502AdcTest, TwentyFourTicksCompleteAdcIndirectYWithPageCrossing) {
    ram.write(0x0000, 0x71);
    ram.write(0x0001, 0x10);
    ram.write(0x0010, 0xFF);
    ram.write(0x0011, 0x02);
    ram.write(0x0304, 0x05);
    cpu.reset();
    cpu.A(0x10);
    cpu.Y(0x05);

    for (int i = 0; i < 20; ++i) {
        cpu.tick();
    }
    EXPECT_EQ(cpu.A(), 0x10);

    cpu.tick();
    cpu.tick();
    cpu.tick();
    cpu.tick();

    EXPECT_EQ(cpu.A(), 0x15);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `ctest --test-dir build -R CPU6502AdcTest.AdcIndirectY`
Expected: FAIL — opcode `0x71` is unimplemented.

- [ ] **Step 3: Implement (indirect),Y addressing**

In `src/cpu/cpu6502.h`, add after `commitADCIndirectX();`:

```cpp
void captureADCIndirectY();
void commitADCIndirectY();
```

In `src/cpu/cpu6502.cpp`, add the opcode constant next to `cOpADCIndirectX`:

```cpp
const uint8_t cOpADCIndirectY = 0x71;
```

Add `case cOpADCIndirectY: captureADCIndirectY(); break;` / `case cOpADCIndirectY: commitADCIndirectY(); break;` to the two `switch (m_IR)` blocks.

Add the method bodies after `commitADCIndirectX()`:

```cpp
void CPU6502::captureADCIndirectY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_addrLatch = m_bus.read(m_PC);
        break;
    case CpuStep::T2:
        m_effAddr = m_bus.read(m_addrLatch);
        break;
    case CpuStep::T3: {
        auto hi = static_cast<uint16_t>(m_bus.read((m_addrLatch + 1) & 0xFF));
        auto base = static_cast<uint16_t>(m_effAddr | (hi << 8));
        EffectiveAddress resolved = indexedAddress(base, m_Y);
        m_effAddr = resolved.address;
        m_pageCrossed = resolved.pageCrossed;
        break;
    }
    case CpuStep::T4:
        if (!m_pageCrossed) {
            loadAluInputs(m_bus.read(m_effAddr));
        }
        break;
    case CpuStep::T5:
        loadAluInputs(m_bus.read(m_effAddr));
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}

void CPU6502::commitADCIndirectY() {
    switch (m_cpuStep) {
    case CpuStep::T1:
        m_PC++;
        m_cpuStep = CpuStep::T2;
        break;
    case CpuStep::T2:
        m_cpuStep = CpuStep::T3;
        break;
    case CpuStep::T3:
        m_cpuStep = CpuStep::T4;
        break;
    case CpuStep::T4:
        if (m_pageCrossed) {
            m_cpuStep = CpuStep::T5;
        } else {
            commitAluResult();
            m_cpuStep = CpuStep::T0;
        }
        break;
    case CpuStep::T5:
        commitAluResult();
        m_cpuStep = CpuStep::T0;
        break;
    default:
        break; // Unreachable: this handler is only invoked while m_cpuStep is T1-T5.
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `ctest --test-dir build -R CPU6502AdcTest`
Expected: PASS.

- [ ] **Step 5: Run the full test suite**

Run: `ctest --test-dir build`
Expected: all tests PASS — this is the last opcode task, confirming no regressions across the whole ADC addressing-mode matrix.

- [ ] **Step 6: Commit**

```bash
git add src/cpu/cpu6502.h src/cpu/cpu6502.cpp test/cpu/cpu6502_adc_test.cpp
git commit -m "feat: wire ADC (indirect),Y through the ALU unit with page-crossing timing"
```

---

### Task 8: `docs/6502-manual.md` — living opcode/cycle reference

**Files:**
- Create: `docs/6502-manual.md`

**Interfaces:**
- Consumes: none (documentation only).
- Produces: `docs/6502-manual.md`, a reference future opcode-implementation passes append rows to.

- [ ] **Step 1: Write the manual**

Create `docs/6502-manual.md`:

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add docs/6502-manual.md
git commit -m "docs: add 6502 manual with cycle counts as implemented"
```

---

## Final verification

- [ ] Run the full suite once more: `ctest --test-dir build --output-on-failure`. Expected: all tests pass, including every test added across Tasks 1-7.
- [ ] Run `cmake --build build --target lint` (if `clang-tidy` is available) and address any new findings introduced by this plan's code, matching the codebase's existing zero-warnings bar (see commit `ea7acb2`).
