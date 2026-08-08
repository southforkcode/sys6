# System-level tty monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a human interact with the emulated 6502 through a real terminal, via a memory-mapped TTY peripheral and a hand-assembled monitor firmware (peek/poke/list/run) running on the CPU itself — never by modifying `CPU6502`.

**Architecture:** A `TTY` `MemoryDevice` (2-register STATUS/DATA) sits on the `Bus` alongside `RAM`/`ROM`. A `System` class owns all of it plus a `TerminalIO` seam that bridges real stdin/stdout in raw mode. The actual peek/poke/list/run logic is 6502 machine code (hand-assembled hex, `loadProgram()`-loaded into `ROM`) that polls the `TTY` registers — the same way real WozMon-era hardware worked.

**Tech Stack:** C++17, CMake, GoogleTest/CTest, POSIX `termios` (macOS/Darwin target).

## Global Constraints

- Never modify `CPU6502` (`src/cpu/cpu6502.h`/`.cpp`) — per `CLAUDE.md`, it is the frozen reference implementation. Everything here is built against its existing public interface (`reset()`, `executeInstruction()`, `PC()`, etc.).
- Memory map: `RAM` `0x0000`–`0x7FFF` (32 KiB), `TTY` `0x8000`–`0x8001`, `0x8002`–`0xBFFF` unmapped, `ROM` `0xC000`–`0xFFFF` (16 KiB, monitor firmware).
- `loadProgram(device, offset, hex)` (`src/utils/program_loader.h`) writes at **device-relative offsets**, not bus addresses — the `Bus` does the address→offset translation (`addr - mapping.start`), `loadProgram()` does not. Since `ROM` is mapped at bus `0xC000`, every hex blob loaded into it must be loaded at `busAddress - 0xC000`, never at the raw bus address. This is why `monitor::loadRoutine()` exists (Task 2) — it's the one place this subtraction happens, so it's never repeated (and never gotten wrong) at each call site.
- Hex-string programs follow the existing project convention (see `test/cpu/cpu6502_fibonacci_e2e_test.cpp`): one string literal per instruction, each with a `//` comment giving the mnemonic and, where non-obvious, what it does.
- New source files use the project's existing style: `#pragma once` headers, `explicit` single-argument constructors, member-init-list order matching declaration order.

---

## Firmware address map (reference for every task below)

Zero page:

| Address       | Name           | Purpose                                             |
|---------------|----------------|------------------------------------------------------|
| `$00`–`$3F`   | `LINEBUF`      | 64-byte line input buffer                             |
| `$40`         | `LINELEN`      | number of characters in the current line              |
| `$41`         | `LINEPOS`      | parse cursor into `LINEBUF`                           |
| `$F0`/`$F1`   | `ADDR` lo/hi   | primary address (peek/poke/run target, list start)    |
| `$F2`/`$F3`   | `ADDR2` lo/hi  | list end address                                       |
| `$F4`         | `BYTEVAL`      | last byte parsed by `PARSE_BYTE`                       |
| `$F5`/`$F6`   | scratch        | transient use within a single routine only             |
| `$F7`         | `ROWCOUNT`     | bytes printed in the current `LIST` row (0–15)         |
| `$F8`–`$FB`   | run trampoline | `JSR <addr>` + `RTS`, built fresh by `RUN`             |
| `$FC`/`$FD`   | `STRPTR` lo/hi | pointer used by `PRINT_STRING`                         |

ROM routines (bus addresses; each gets its own 256-byte page so tasks never have to renumber earlier routines — this is the hand-assembly equivalent of a linker, done by hand since there's no assembler):

| Address   | Routine          | Added in |
|-----------|------------------|----------|
| `$C000`   | `GETCHAR`        | Task 2   |
| `$C100`   | `PUTCHAR`        | Task 2   |
| `$C200`   | `PRINT_HEX_BYTE` (+ `PRINT_NIBBLE` at `$C220`) | Task 2 |
| `$C300`   | `PRINT_STRING`   | Task 2   |
| `$C400`   | `READ_LINE`      | Task 5   |
| `$C500`   | `PARSE_ADDR`     | Task 3   |
| `$C600`   | `PARSE_BYTE`     | Task 3   |
| `$C700`   | `HEXVAL`         | Task 2   |
| `$C800`   | `DISPATCH`       | Task 4   |
| `$C900`   | *(reserved for test driver snippets — never used by real firmware)* | — |
| `$CA00`   | `PEEK`           | Task 4   |
| `$CB00`   | `LIST`           | Task 4   |
| `$CC00`   | `POKE`           | Task 4   |
| `$CD00`   | `RUN`            | Task 4   |
| `$CE00`   | banner/prompt strings | Task 5 |
| `$CF00`/`$CF0E`/`$CF11` | `COLD_START`/`WARM_START`/`MAIN_LOOP` | Task 5 |
| `$FFFA`–`$FFFF` | NMI/RESET/BRK vectors | Task 5 |

RAM trampoline `$00F8`–`$00FB` is writable (unlike ROM, where `write()` is a documented no-op) — this is why `RUN` builds its `JSR`/`RTS` there instead of self-modifying ROM.

---

### Task 1: `TTY` peripheral

**Files:**
- Create: `src/peripherals/tty.h`
- Create: `src/peripherals/tty.cpp`
- Test: `test/peripherals/tty_test.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces: `class TTY : public MemoryDevice` — `explicit TTY(std::ostream &out)`, `size()`/`read()`/`write()` (the `MemoryDevice` overrides), plus `void receive(uint8_t byte)` and `bool rxReady() const` (host-facing, not part of `MemoryDevice`). Offset 0 = STATUS (bit0 RXRDY, bit1 TXRDY-always-1), offset 1 = DATA.

- [ ] **Step 1: Write the failing tests**

Create `test/peripherals/tty_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "peripherals/tty.h"

#include <sstream>

TEST(TTYTest, SizeIsTwoBytes) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.size(), 2u);
}

TEST(TTYTest, StatusRxReadyBitClearUntilAByteArrives) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.read(0) & 0x01, 0);
    tty.receive('A');
    EXPECT_EQ(tty.read(0) & 0x01, 1);
}

TEST(TTYTest, StatusTxReadyBitAlwaysSet) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.read(0) & 0x02, 0x02);
    tty.write(1, 'x');
    EXPECT_EQ(tty.read(0) & 0x02, 0x02);
}

TEST(TTYTest, ReadingDataReturnsReceivedByteAndClearsRxReady) {
    std::ostringstream out;
    TTY tty(out);
    tty.receive(0x42);
    EXPECT_EQ(tty.read(1), 0x42);
    EXPECT_EQ(tty.read(0) & 0x01, 0);
    EXPECT_FALSE(tty.rxReady());
}

TEST(TTYTest, WritingDataReachesOutputStream) {
    std::ostringstream out;
    TTY tty(out);
    tty.write(1, 'H');
    tty.write(1, 'i');
    EXPECT_EQ(out.str(), "Hi");
}

TEST(TTYTest, SecondReceiveBeforeReadIsDropped) {
    std::ostringstream out;
    TTY tty(out);
    tty.receive('A');
    tty.receive('B'); // single holding register -- no FIFO, this is dropped
    EXPECT_EQ(tty.read(1), 'A');
}
```

- [ ] **Step 2: Add the new files to the test build**

Edit `test/CMakeLists.txt`: add `peripherals/tty_test.cpp` to the test source list and `${CMAKE_SOURCE_DIR}/src/peripherals/tty.cpp` to the production source list (both lists already exist in the file — add one line to each, next to the other `.cpp` entries).

- [ ] **Step 3: Run tests to verify they fail**

Run: `cmake -S . -B build && cmake --build build --target sys6_tests 2>&1 | tail -30`
Expected: FAIL to compile (`tty.h` doesn't exist yet).

- [ ] **Step 4: Write `TTY`**

Create `src/peripherals/tty.h`:

```cpp
#pragma once

#include "memory/memory_device.h"

#include <cstdint>
#include <ostream>

class TTY : public MemoryDevice {
public:
    explicit TTY(std::ostream &out) : m_out(out) {}

    size_t size() const override { return 2; }
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

    // Host-facing API (not part of MemoryDevice) -- called by System, or
    // directly by tests in place of a real terminal.
    void receive(uint8_t byte);
    bool rxReady() const { return m_rxReady; }

private:
    std::ostream &m_out;
    mutable uint8_t m_rxByte = 0;
    mutable bool m_rxReady = false;
};
```

Create `src/peripherals/tty.cpp`:

```cpp
#include "tty.h"

uint8_t TTY::read(uint16_t offset) const {
    if (offset == 0) {
        uint8_t status = 0x02; // TXRDY: always ready, no output buffering to model
        if (m_rxReady) {
            status |= 0x01;
        }
        return status;
    }
    // offset == 1: DATA
    m_rxReady = false;
    return m_rxByte;
}

void TTY::write(uint16_t offset, uint8_t val) {
    if (offset == 1) {
        m_out.put(static_cast<char>(val));
        m_out.flush();
    }
}

void TTY::receive(uint8_t byte) {
    if (m_rxReady) {
        return; // single holding register, no FIFO
    }
    m_rxByte = byte;
    m_rxReady = true;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R TTYTest`
Expected: all 6 `TTYTest` cases PASS.

- [ ] **Step 6: Commit**

```bash
git add src/peripherals/tty.h src/peripherals/tty.cpp test/peripherals/tty_test.cpp test/CMakeLists.txt
git commit -m "feat: add TTY memory-mapped peripheral"
```

---

### Task 2: Monitor firmware skeleton + I/O primitives

Establishes `monitor_firmware.h`/`.cpp` (the address-constant table + `loadRoutine()` helper every later task builds on) and the four foundational routines every other routine calls: `GETCHAR`, `PUTCHAR`, `PRINT_HEX_BYTE`/`PRINT_NIBBLE`, `PRINT_STRING`, `HEXVAL`.

Also establishes the routine-level test pattern used through Task 4: a shared `RoutineTestFixture` builds a `RAM`+`TTY`+`ROM`+`Bus`+`CPU6502`, loads just the routine(s) under test via `monitor::loadRoutine()`, loads a tiny hand-assembled "driver" snippet at the reserved `$C900` test page (never used by the real firmware) that sets up inputs, `JSR`s into the routine under test, and ends in `BRK`, points the reset vector at the driver, then runs via `cpu.run(N)`/`cpu.halted()` — the same pattern `cpu6502_fibonacci_e2e_test.cpp` already uses.

**Files:**
- Create: `src/system/monitor_firmware.h`
- Create: `src/system/monitor_firmware.cpp`
- Create: `test/system/routine_test_fixture.h`
- Create: `test/system/monitor_routines_test.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `MemoryDevice` (`src/memory/memory_device.h`), `loadProgram()` (`src/utils/program_loader.h`).
- Produces: `namespace monitor { constexpr uint16_t kRomBase, k*Addr (zero-page and routine constants), void loadRoutine(MemoryDevice&, uint16_t busAddr, const std::string &hex); }`. `RoutineTestFixture` (test-only): public members `ram`, `output` (`std::ostringstream`), `tty`, `rom`, `bus`, `cpu`; method `void loadDriver(const std::string &hex)`.

- [ ] **Step 1: Write `monitor_firmware.h`/`.cpp` with the address table and `loadRoutine()`**

Create `src/system/monitor_firmware.h`:

```cpp
#pragma once

#include "memory/memory_device.h"

#include <cstdint>
#include <string>

namespace monitor {

constexpr uint16_t kRomBase = 0xC000;
constexpr uint16_t kRomSize = 0x4000;

// Zero-page layout
constexpr uint16_t kLineBufAddr = 0x0000; // 64 bytes, $00-$3F
constexpr uint16_t kLineLenAddr = 0x0040;
constexpr uint16_t kLinePosAddr = 0x0041;
constexpr uint16_t kAddrLoAddr = 0x00F0;
constexpr uint16_t kAddrHiAddr = 0x00F1;
constexpr uint16_t kAddr2LoAddr = 0x00F2;
constexpr uint16_t kAddr2HiAddr = 0x00F3;
constexpr uint16_t kByteValAddr = 0x00F4;
constexpr uint16_t kScratchLoAddr = 0x00F5;
constexpr uint16_t kScratchHiAddr = 0x00F6;
constexpr uint16_t kRowCountAddr = 0x00F7;
constexpr uint16_t kRunTrampolineAddr = 0x00F8; // 4 bytes, $F8-$FB
constexpr uint16_t kStrPtrLoAddr = 0x00FC;
constexpr uint16_t kStrPtrHiAddr = 0x00FD;

// ROM routine addresses (bus addresses -- see docs/superpowers/plans for the
// full table and why each routine gets a fixed 256-byte page)
constexpr uint16_t kGetCharAddr = 0xC000;
constexpr uint16_t kPutCharAddr = 0xC100;
constexpr uint16_t kPrintHexByteAddr = 0xC200;
constexpr uint16_t kPrintNibbleAddr = 0xC220;
constexpr uint16_t kPrintStringAddr = 0xC300;
constexpr uint16_t kHexValAddr = 0xC700;
constexpr uint16_t kTestDriverAddr = 0xC900; // reserved for tests only

extern const std::string kGetCharHex;
extern const std::string kPutCharHex;
extern const std::string kPrintHexByteHex;
extern const std::string kPrintNibbleHex;
extern const std::string kPrintStringHex;
extern const std::string kHexValHex;

// Writes `hex` into `rom` at device-relative offset (busAddr - kRomBase).
// loadProgram() (src/utils/program_loader.h) writes at device-relative
// offsets, not bus addresses -- Bus does that translation for normal
// reads/writes, but loadProgram() talks to the MemoryDevice directly, so
// every routine load has to do the subtraction itself. This is the one
// place it happens.
void loadRoutine(MemoryDevice &rom, uint16_t busAddr, const std::string &hex);

} // namespace monitor
```

Create `src/system/monitor_firmware.cpp`:

```cpp
#include "monitor_firmware.h"

#include "utils/program_loader.h"

namespace monitor {

void loadRoutine(MemoryDevice &rom, uint16_t busAddr, const std::string &hex) {
    loadProgram(rom, static_cast<uint16_t>(busAddr - kRomBase), hex);
}

// GETCHAR ($C000): busy-waits for RXRDY, reads and returns DATA in A. Does
// not echo -- callers that want an echo call PUTCHAR themselves.
const std::string kGetCharHex =
    "AD 00 80" // GETCHAR: LDA $8000        (TTY STATUS)
    "29 01"    //   AND #$01                (RXRDY bit)
    "F0 F9"    //   BEQ GETCHAR             (loop while not ready)
    "AD 01 80" //   LDA $8001               (TTY DATA -- clears RXRDY)
    "60";      //   RTS

// PUTCHAR ($C100): writes A to TTY DATA. TXRDY is always 1 by design, so
// there's nothing to poll before writing.
const std::string kPutCharHex =
    "8D 01 80" // PUTCHAR: STA $8001
    "60";      //   RTS

// PRINT_HEX_BYTE ($C200): prints A as two hex ASCII chars, high nibble
// first. PRINT_NIBBLE ($C220) is its private helper (A in [0..15]); both
// nibble paths tail-call PUTCHAR via JMP so PUTCHAR's own RTS returns
// straight to PRINT_HEX_BYTE's caller.
const std::string kPrintHexByteHex =
    "48"       // PRINT_HEX_BYTE: PHA       (save original byte)
    "4A"       //   LSR A
    "4A"       //   LSR A
    "4A"       //   LSR A
    "4A"       //   LSR A                   (A = high nibble)
    "20 20 C2" //   JSR PRINT_NIBBLE
    "68"       //   PLA                     (restore original byte)
    "29 0F"    //   AND #$0F                (A = low nibble)
    "20 20 C2" //   JSR PRINT_NIBBLE
    "60";      //   RTS

const std::string kPrintNibbleHex =
    "C9 0A" // PRINT_NIBBLE: CMP #$0A
    "90 05" //   BCC DIGIT                 (A<10 -> carry clear -> digit path)
    "69 36" //   ADC #$36                  (carry=1 here: A+0x36+1='A'..'F')
    "4C 00 C1" //   JMP PUTCHAR             (tail call)
    "69 30" // DIGIT: ADC #$30             (carry=0 here: A+0x30='0'..'9')
    "4C 00 C1"; //   JMP PUTCHAR            (tail call)

// PRINT_STRING ($C300): prints the null-terminated string pointed to by
// STRPTR ($FC/$FD).
const std::string kPrintStringHex =
    "A0 00"    // PRINT_STRING: LDY #$00
    "B1 FC"    // LOOP: LDA ($FC),Y
    "F0 07"    //   BEQ DONE
    "20 00 C1" //   JSR PUTCHAR
    "C8"       //   INY
    "4C 02 C3" //   JMP LOOP
    "60";      // DONE: RTS

// HEXVAL ($C700): converts an ASCII hex char in A to its nibble value in A.
// Carry clear = valid, carry set = invalid (not 0-9/A-F/a-f).
const std::string kHexValHex =
    "C9 30" // HEXVAL: CMP #$30
    "90 23" //   BCC INVALID
    "C9 3A" //   CMP #$3A
    "B0 05" //   BCS CHECKALPHA
    "38"    //   SEC
    "E9 30" //   SBC #$30
    "18"    //   CLC
    "60"    //   RTS
    "C9 41" // CHECKALPHA: CMP #$41
    "90 16" //   BCC INVALID
    "C9 47" //   CMP #$47
    "B0 05" //   BCS CHECKLOWER
    "38"    //   SEC
    "E9 37" //   SBC #$37
    "18"    //   CLC
    "60"    //   RTS
    "C9 61" // CHECKLOWER: CMP #$61
    "90 09" //   BCC INVALID
    "C9 67" //   CMP #$67
    "B0 05" //   BCS INVALID
    "38"    //   SEC
    "E9 57" //   SBC #$57
    "18"    //   CLC
    "60"    //   RTS
    "38"    // INVALID: SEC
    "60";   //   RTS

} // namespace monitor
```

- [ ] **Step 2: Write the routine test fixture**

Create `test/system/routine_test_fixture.h`:

```cpp
#pragma once

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"
#include "peripherals/tty.h"
#include "system/monitor_firmware.h"

#include <sstream>
#include <string>
#include <vector>

// Builds a RAM+TTY+ROM+Bus+CPU6502 for testing individual monitor routines
// in isolation, the same way cpu6502_fibonacci_e2e_test.cpp tests a whole
// program: load a tiny hand-assembled "driver" snippet that sets up
// inputs, JSRs into the routine(s) under test, and ends in BRK; point the
// reset vector at it; run via cpu.run()/cpu.halted().
struct RoutineTestFixture {
    RAM ram{0x8000};
    std::ostringstream output;
    TTY tty{output};
    ROM rom{std::vector<uint8_t>(monitor::kRomSize)};
    Bus bus;
    CPU6502 cpu{bus};

    RoutineTestFixture() {
        bus.attach(0x0000, 0x7FFF, ram);
        bus.attach(0x8000, 0x8001, tty);
        bus.attach(0xC000, 0xFFFF, rom);
    }

    // Loads `hex` at the reserved test-driver page and points the reset
    // vector at it. Call cpu.reset() after this to boot into the driver.
    void loadDriver(const std::string &hex) {
        monitor::loadRoutine(rom, monitor::kTestDriverAddr, hex);
        monitor::loadRoutine(rom, 0xFFFC, "00 C9"); // reset vector -> $C900
    }
};
```

- [ ] **Step 3: Write the failing routine tests**

Create `test/system/monitor_routines_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "routine_test_fixture.h"

TEST(MonitorRoutines, PutCharWritesToOutput) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    fx.loadDriver("A9 41"    // LDA #$41           ('A')
                  "20 00 C1" // JSR PUTCHAR
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.output.str(), "A");
}

TEST(MonitorRoutines, GetCharReturnsAndConsumesReceivedByte) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kGetCharAddr, monitor::kGetCharHex);
    fx.loadDriver("20 00 C0" // JSR GETCHAR
                  "85 50"    // STA $50            (stash received char)
                  "00");     // BRK
    fx.tty.receive('X');
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.ram.read(0x50), 'X');
    EXPECT_FALSE(fx.tty.rxReady());
}

TEST(MonitorRoutines, PrintHexByteFormatsHighAndLowNibbles) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kPrintHexByteAddr, monitor::kPrintHexByteHex);
    monitor::loadRoutine(fx.rom, monitor::kPrintNibbleAddr, monitor::kPrintNibbleHex);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    fx.loadDriver("A9 3F"    // LDA #$3F
                  "20 00 C2" // JSR PRINT_HEX_BYTE
                  "A9 A0"    // LDA #$A0
                  "20 00 C2" // JSR PRINT_HEX_BYTE
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.output.str(), "3FA0");
}

TEST(MonitorRoutines, PrintStringPrintsUntilNull) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kPrintStringAddr, monitor::kPrintStringHex);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    fx.loadDriver("A9 0C"    // LDA #$0C           (string is 12 bytes past driver start)
                  "85 FC"    // STA $FC
                  "A9 C9"    // LDA #$C9
                  "85 FD"    // STA $FD
                  "20 00 C3" // JSR PRINT_STRING
                  "00"       // BRK
                  "48 69 00"); // "Hi\0"
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.output.str(), "Hi");
}

TEST(MonitorRoutines, HexValAcceptsDigitsUpperAndLowerLetters) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    fx.loadDriver("A9 39"    // LDA #$39           ('9')
                  "20 00 C7" // JSR HEXVAL
                  "85 50"    // STA $50            (expect 9)
                  "A9 42"    // LDA #$42           ('B')
                  "20 00 C7" // JSR HEXVAL
                  "85 51"    // STA $51            (expect 11)
                  "A9 66"    // LDA #$66           ('f')
                  "20 00 C7" // JSR HEXVAL
                  "85 52"    // STA $52            (expect 15)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.ram.read(0x50), 9);
    EXPECT_EQ(fx.ram.read(0x51), 11);
    EXPECT_EQ(fx.ram.read(0x52), 15);
}

TEST(MonitorRoutines, HexValRejectsNonHexAndSetsCarry) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    fx.loadDriver("A9 47"    // LDA #$47           ('G' -- not hex)
                  "20 00 C7" // JSR HEXVAL
                  "08"       // PHP
                  "68"       // PLA
                  "29 01"    // AND #$01           (isolate carry bit)
                  "85 50"    // STA $50            (expect 1: invalid)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(1000));
    EXPECT_EQ(fx.ram.read(0x50), 1);
}
```

- [ ] **Step 4: Add the new files to the test build**

Edit `test/CMakeLists.txt`: add `system/monitor_routines_test.cpp` to the test source list and `${CMAKE_SOURCE_DIR}/src/system/monitor_firmware.cpp` to the production source list.

- [ ] **Step 5: Run tests to verify they fail, then pass**

Run: `cmake --build build --target sys6_tests 2>&1 | tail -30`
Expected first: FAIL to compile (files referenced don't exist until Step 1/2 above are in place — if you're following this plan top-to-bottom the files already exist, so instead confirm the *tests* fail before Step 1's implementation and pass after; the two are combined in this task because the implementation was fully specified alongside the tests above).

Run: `ctest --test-dir build --output-on-failure -R MonitorRoutines`
Expected: all 6 `MonitorRoutines` cases PASS.

- [ ] **Step 6: Commit**

```bash
git add src/system/monitor_firmware.h src/system/monitor_firmware.cpp \
        test/system/routine_test_fixture.h test/system/monitor_routines_test.cpp \
        test/CMakeLists.txt
git commit -m "feat: add monitor firmware I/O primitives (GETCHAR/PUTCHAR/PRINT_HEX_BYTE/PRINT_STRING/HEXVAL)"
```

---

### Task 3: `PARSE_ADDR` and `PARSE_BYTE`

**Files:**
- Modify: `src/system/monitor_firmware.h`
- Modify: `src/system/monitor_firmware.cpp`
- Modify: `test/system/monitor_routines_test.cpp`

**Interfaces:**
- Consumes: `HEXVAL` (`$C700`, Task 2), `LINEBUF`/`LINELEN`/`LINEPOS` zero-page layout (Task 2 header).
- Produces: `kParseAddrAddr = 0xC500`, `kParseByteAddr = 0xC600`, `kParseAddrHex`, `kParseByteHex`. `PARSE_ADDR` reads 1–4 hex digits from `LINEBUF` starting at `LINEPOS` into `$F0`/`$F1` (zero-extended 16-bit), advancing `LINEPOS` past each consumed digit, stopping at the first non-hex character; carry set on return means zero digits were consumed (invalid). `PARSE_BYTE` does the same into `$F4`, capped at 2 digits.

- [ ] **Step 1: Add the new address constants and `extern` hex declarations**

Edit `src/system/monitor_firmware.h`, in the ROM routine addresses block, add after `kHexValAddr`:

```cpp
constexpr uint16_t kParseAddrAddr = 0xC500;
constexpr uint16_t kParseByteAddr = 0xC600;
```

And after `extern const std::string kHexValHex;`, add:

```cpp
extern const std::string kParseAddrHex;
extern const std::string kParseByteHex;
```

- [ ] **Step 2: Write the failing tests**

Append to `test/system/monitor_routines_test.cpp`:

```cpp
TEST(MonitorRoutines, ParseAddrParsesFourHexDigits) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 04"    // LDA #$04
                  "85 40"    // STA $40            (LINELEN = 4)
                  "A9 41"    // LDA #$41           ('A')
                  "85 00"    // STA $00
                  "A9 42"    // LDA #$42           ('B')
                  "85 01"    // STA $01
                  "A9 43"    // LDA #$43           ('C')
                  "85 02"    // STA $02
                  "A9 44"    // LDA #$44           ('D')
                  "85 03"    // STA $03
                  "20 00 C5" // JSR PARSE_ADDR
                  "08"       // PHP
                  "68"       // PLA
                  "29 01"    // AND #$01
                  "85 50"    // STA $50            (expect 0: valid)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kAddrLoAddr), 0xCD);
    EXPECT_EQ(fx.ram.read(monitor::kAddrHiAddr), 0xAB);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 4);
    EXPECT_EQ(fx.ram.read(0x50), 0);
}

TEST(MonitorRoutines, ParseAddrStopsAtNonHexAndZeroExtends) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 35"    // LDA #$35           ('5')
                  "85 00"    // STA $00
                  "A9 3A"    // LDA #$3A           (':')
                  "85 01"    // STA $01
                  "20 00 C5" // JSR PARSE_ADDR
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kAddrLoAddr), 0x05);
    EXPECT_EQ(fx.ram.read(monitor::kAddrHiAddr), 0x00);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 1); // stopped before ':'
}

TEST(MonitorRoutines, ParseAddrFailsOnZeroDigits) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 01"    // LDA #$01
                  "85 40"    // STA $40            (LINELEN = 1)
                  "A9 3A"    // LDA #$3A           (':')
                  "85 00"    // STA $00
                  "20 00 C5" // JSR PARSE_ADDR
                  "08"       // PHP
                  "68"       // PLA
                  "29 01"    // AND #$01
                  "85 50"    // STA $50            (expect 1: invalid)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(0x50), 1);
}

TEST(MonitorRoutines, ParseByteParsesTwoDigits) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseByteAddr, monitor::kParseByteHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 39"    // LDA #$39           ('9')
                  "85 00"    // STA $00
                  "A9 46"    // LDA #$46           ('F')
                  "85 01"    // STA $01
                  "20 00 C6" // JSR PARSE_BYTE
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kByteValAddr), 0x9F);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 2);
}

TEST(MonitorRoutines, ParseByteStopsAfterOneDigitOnNonHex) {
    RoutineTestFixture fx;
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseByteAddr, monitor::kParseByteHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 37"    // LDA #$37           ('7')
                  "85 00"    // STA $00
                  "A9 20"    // LDA #$20           (' ')
                  "85 01"    // STA $01
                  "20 00 C6" // JSR PARSE_BYTE
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(2000));
    EXPECT_EQ(fx.ram.read(monitor::kByteValAddr), 0x07);
    EXPECT_EQ(fx.ram.read(monitor::kLinePosAddr), 1); // stopped at the space
}
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests 2>&1 | tail -30`
Expected: FAIL to compile (`kParseAddrHex`/`kParseByteHex` undefined).

- [ ] **Step 4: Implement `PARSE_ADDR` and `PARSE_BYTE`**

Edit `src/system/monitor_firmware.cpp`, append after `kHexValHex`'s definition (still inside `namespace monitor { ... }`):

```cpp
// PARSE_ADDR ($C500): parses hex digits from LINEBUF starting at LINEPOS
// into ADDR ($F0/$F1, 16-bit, zero-extended), advancing LINEPOS past each
// consumed digit, stopping at the first non-hex character. Carry set on
// return = zero digits consumed (invalid).
const std::string kParseAddrHex =
    "A9 00"    // PARSE_ADDR: LDA #$00
    "85 F0"    //   STA $F0                 (ADDRLO = 0)
    "85 F1"    //   STA $F1                 (ADDRHI = 0)
    "85 F5"    //   STA $F5                 (digit count = 0)
    "A6 41"    // LOOP: LDX $41             (X = LINEPOS)
    "E4 40"    //   CPX $40                 (LINELEN)
    "B0 24"    //   BCS DONE
    "B5 00"    //   LDA $00,X               (A = LINEBUF[LINEPOS])
    "20 00 C7" //   JSR HEXVAL
    "B0 1D"    //   BCS DONE                (non-hex: stop, don't consume)
    "48"       //   PHA                     (save digit)
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1
    "06 F0"    //   ASL $F0
    "26 F1"    //   ROL $F1                 (ADDR <<= 4)
    "68"       //   PLA                     (restore digit)
    "05 F0"    //   ORA $F0
    "85 F0"    //   STA $F0                 (ADDRLO |= digit)
    "E6 41"    //   INC $41                 (LINEPOS++)
    "E6 F5"    //   INC $F5                 (digit count++)
    "4C 08 C5" //   JMP LOOP
    "A5 F5"    // DONE: LDA $F5
    "F0 02"    //   BEQ FAIL
    "18"       //   CLC
    "60"       //   RTS
    "38"       // FAIL: SEC
    "60";      //   RTS

// PARSE_BYTE ($C600): same shape as PARSE_ADDR but parses into BYTEVAL
// ($F4, 8-bit), capped at 2 digits.
const std::string kParseByteHex =
    "A9 00"    // PARSE_BYTE: LDA #$00
    "85 F4"    //   STA $F4                 (BYTEVAL = 0)
    "85 F5"    //   STA $F5                 (digit count = 0)
    "A5 F5"    // LOOP: LDA $F5
    "C9 02"    //   CMP #$02                (already 2 digits?)
    "B0 22"    //   BCS DONE
    "A6 41"    //   LDX $41                 (X = LINEPOS)
    "E4 40"    //   CPX $40
    "B0 1C"    //   BCS DONE
    "B5 00"    //   LDA $00,X
    "20 00 C7" //   JSR HEXVAL
    "B0 15"    //   BCS DONE                (non-hex: stop, don't consume)
    "48"       //   PHA
    "06 F4"    //   ASL $F4
    "06 F4"    //   ASL $F4
    "06 F4"    //   ASL $F4
    "06 F4"    //   ASL $F4                 (BYTEVAL <<= 4)
    "68"       //   PLA
    "05 F4"    //   ORA $F4
    "85 F4"    //   STA $F4                 (BYTEVAL |= digit)
    "E6 41"    //   INC $41                 (LINEPOS++)
    "E6 F5"    //   INC $F5                 (digit count++)
    "4C 06 C6" //   JMP LOOP
    "A5 F5"    // DONE: LDA $F5
    "F0 02"    //   BEQ FAIL
    "18"       //   CLC
    "60"       //   RTS
    "38"       // FAIL: SEC
    "60";      //   RTS
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines`
Expected: all `MonitorRoutines` cases (Task 2's + Task 3's 5 new ones) PASS.

- [ ] **Step 6: Commit**

```bash
git add src/system/monitor_firmware.h src/system/monitor_firmware.cpp test/system/monitor_routines_test.cpp
git commit -m "feat: add monitor firmware address parsing (PARSE_ADDR/PARSE_BYTE)"
```

---

### Task 4: `DISPATCH`, `PEEK`, `LIST`, `POKE`, `RUN`

**Files:**
- Modify: `src/system/monitor_firmware.h`
- Modify: `src/system/monitor_firmware.cpp`
- Modify: `test/system/monitor_routines_test.cpp`

**Interfaces:**
- Consumes: `PARSE_ADDR` (`$C500`), `PARSE_BYTE` (`$C600`), `PRINT_HEX_BYTE` (`$C200`), `PUTCHAR` (`$C100`), zero-page layout (Tasks 2–3).
- Produces: `kDispatchAddr = 0xC800`, `kPeekAddr = 0xCA00`, `kListAddr = 0xCB00`, `kPokeAddr = 0xCC00`, `kRunAddr = 0xCD00`, and their hex constants. `DISPATCH` is called with `LINEPOS` pointing just past a successfully-parsed address in `$F0`/`$F1`; it inspects `LINEBUF[LINEPOS]` and tail-jumps to `PEEK` (end of line), `LIST` (`.` + second address), `POKE` (`:` + byte list), `RUN` (` R`), or prints `?`+CRLF and returns.

- [ ] **Step 1: Add the new address constants and `extern` hex declarations**

Edit `src/system/monitor_firmware.h`, add after `kParseByteAddr`:

```cpp
constexpr uint16_t kDispatchAddr = 0xC800;
constexpr uint16_t kPeekAddr = 0xCA00;
constexpr uint16_t kListAddr = 0xCB00;
constexpr uint16_t kPokeAddr = 0xCC00;
constexpr uint16_t kRunAddr = 0xCD00;
```

And after `extern const std::string kParseByteHex;`, add:

```cpp
extern const std::string kDispatchHex;
extern const std::string kPeekHex;
extern const std::string kListHex;
extern const std::string kPokeHex;
extern const std::string kRunHex;
```

- [ ] **Step 2: Write the failing tests**

Append to `test/system/monitor_routines_test.cpp`. Each test loads `DISPATCH` plus everything it transitively needs (`PARSE_ADDR`, `PARSE_BYTE`, `HEXVAL`, `PRINT_HEX_BYTE`+`PRINT_NIBBLE`, `PUTCHAR`, plus whichever of `PEEK`/`LIST`/`POKE`/`RUN` it exercises), pre-loads `LINEBUF`/`LINELEN`/`LINEPOS`/`$F0`/`$F1` to simulate "an address was just parsed", and calls `DISPATCH` directly:

```cpp
namespace {
void loadDispatchDeps(RoutineTestFixture &fx) {
    monitor::loadRoutine(fx.rom, monitor::kHexValAddr, monitor::kHexValHex);
    monitor::loadRoutine(fx.rom, monitor::kParseAddrAddr, monitor::kParseAddrHex);
    monitor::loadRoutine(fx.rom, monitor::kParseByteAddr, monitor::kParseByteHex);
    monitor::loadRoutine(fx.rom, monitor::kPrintHexByteAddr, monitor::kPrintHexByteHex);
    monitor::loadRoutine(fx.rom, monitor::kPrintNibbleAddr, monitor::kPrintNibbleHex);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kDispatchAddr, monitor::kDispatchHex);
    monitor::loadRoutine(fx.rom, monitor::kPeekAddr, monitor::kPeekHex);
    monitor::loadRoutine(fx.rom, monitor::kListAddr, monitor::kListHex);
    monitor::loadRoutine(fx.rom, monitor::kPokeAddr, monitor::kPokeHex);
    monitor::loadRoutine(fx.rom, monitor::kRunAddr, monitor::kRunHex);
}
} // namespace

TEST(MonitorRoutines, DispatchPeeksWhenLineEndsAtAddress) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    fx.ram.write(0x0050, 0xAB);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 40"    // STA $40            (LINELEN = 0)
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 50"    // LDA #$50
                  "85 F0"    // STA $F0            (ADDR = $0050)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "0050: AB\r\n");
}

TEST(MonitorRoutines, DispatchListsRangeAcrossOneRow) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    fx.ram.write(0x0050, 0x11);
    fx.ram.write(0x0051, 0x22);
    fx.ram.write(0x0052, 0x33);
    // LINEBUF = ".0052", LINEPOS = 0, LINELEN = 5; $F0/$F1 already holds
    // the first address ($0050), as if MAIN_LOOP's PARSE_ADDR had already
    // run -- DISPATCH picks up from there.
    fx.loadDriver("A9 2E"    // LDA #$2E           ('.')
                  "85 00"    // STA $00
                  "A9 30"    // LDA #$30           ('0')
                  "85 01"    // STA $01
                  "A9 30"    // LDA #$30           ('0')
                  "85 02"    // STA $02
                  "A9 35"    // LDA #$35           ('5')
                  "85 03"    // STA $03
                  "A9 32"    // LDA #$32           ('2')
                  "85 04"    // STA $04
                  "A9 05"    // LDA #$05
                  "85 40"    // STA $40            (LINELEN = 5)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 50"    // LDA #$50
                  "85 F0"    // STA $F0            (ADDR = $0050)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.output.str(), "0050: 11 22 33 \r\n");
}

TEST(MonitorRoutines, DispatchPokesBytesStartingAtAddress) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // LINEBUF = ": AB CD", LINEPOS = 0, LINELEN = 7; $F0/$F1 = $0060.
    fx.loadDriver("A9 3A"    // LDA #$3A           (':')
                  "85 00"    // STA $00
                  "A9 20"    // LDA #$20           (' ')
                  "85 01"    // STA $01
                  "A9 41"    // LDA #$41           ('A')
                  "85 02"    // STA $02
                  "A9 42"    // LDA #$42           ('B')
                  "85 03"    // STA $03
                  "A9 20"    // LDA #$20           (' ')
                  "85 04"    // STA $04
                  "A9 43"    // LDA #$43           ('C')
                  "85 05"    // STA $05
                  "A9 44"    // LDA #$44           ('D')
                  "85 06"    // STA $06
                  "A9 07"    // LDA #$07
                  "85 40"    // STA $40            (LINELEN = 7)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 60"    // LDA #$60
                  "85 F0"    // STA $F0            (ADDR = $0060)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.ram.read(0x0060), 0xAB);
    EXPECT_EQ(fx.ram.read(0x0061), 0xCD);
}

TEST(MonitorRoutines, DispatchRunsProgramAtAddress) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // A tiny user program: INC $80 ; RTS
    fx.ram.write(0x0070, 0xE6);
    fx.ram.write(0x0071, 0x80);
    fx.ram.write(0x0072, 0x60);
    // LINEBUF = " R", LINEPOS = 0, LINELEN = 2; $F0/$F1 = $0070.
    fx.loadDriver("A9 20"    // LDA #$20           (' ')
                  "85 00"    // STA $00
                  "A9 52"    // LDA #$52           ('R')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40            (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "A9 70"    // LDA #$70
                  "85 F0"    // STA $F0            (ADDR = $0070)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.ram.read(0x0080), 1);
}

TEST(MonitorRoutines, DispatchPrintsQuestionMarkOnMalformedSuffix) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // LINEBUF = "#", LINEPOS = 0, LINELEN = 1 -- '#' is none of '.'/':'/' '.
    fx.loadDriver("A9 23"    // LDA #$23           ('#')
                  "85 00"    // STA $00
                  "A9 01"    // LDA #$01
                  "85 40"    // STA $40            (LINELEN = 1)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41            (LINEPOS = 0)
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests 2>&1 | tail -30`
Expected: FAIL to compile (`kDispatchHex` etc. undefined).

- [ ] **Step 4: Implement `DISPATCH`, `PEEK`, `LIST`, `POKE`, `RUN`**

Edit `src/system/monitor_firmware.cpp`, append after `kParseByteHex`'s definition:

```cpp
// DISPATCH ($C800): called with LINEPOS just past a successfully-parsed
// address in ADDR ($F0/$F1). Inspects LINEBUF[LINEPOS] and tail-jumps into
// PEEK/LIST/POKE/RUN (so their own RTS returns straight to DISPATCH's
// caller), or prints '?'+CRLF and returns itself.
const std::string kDispatchHex =
    "A5 41"    // DISPATCH: LDA $41         (LINEPOS)
    "C5 40"    //   CMP $40                 (LINELEN)
    "90 03"    //   BCC NOTEND
    "4C 00 CA" //   JMP PEEK                (end of line -> peek)
    "A6 41"    // NOTEND: LDX $41
    "B5 00"    //   LDA $00,X               (next char)
    "C9 2E"    //   CMP #$2E                ('.')
    "F0 0B"    //   BEQ DOLIST
    "C9 3A"    //   CMP #$3A                (':')
    "F0 31"    //   BEQ DOPOKE
    "C9 20"    //   CMP #$20                (' ')
    "F0 32"    //   BEQ MAYBERUN
    "4C 5B C8" //   JMP ERROR
    "E6 41"    // DOLIST: INC $41           (skip '.')
    "A5 F0"    //   LDA $F0
    "85 F2"    //   STA $F2                 (stash first-addr lo)
    "A5 F1"    //   LDA $F1
    "85 F3"    //   STA $F3                 (stash first-addr hi)
    "20 00 C5" //   JSR PARSE_ADDR          (second address -> $F0/$F1)
    "B0 30"    //   BCS ERROR               (invalid second address)
    "A5 F0"    //   LDA $F0
    "85 F5"    //   STA $F5                 (temp = second addr lo)
    "A5 F1"    //   LDA $F1
    "85 F6"    //   STA $F6                 (temp hi)
    "A5 F2"    //   LDA $F2
    "85 F0"    //   STA $F0                 ($F0/$F1 = first addr = START)
    "A5 F3"    //   LDA $F3
    "85 F1"    //   STA $F1
    "A5 F5"    //   LDA $F5
    "85 F2"    //   STA $F2                 ($F2/$F3 = second addr = END)
    "A5 F6"    //   LDA $F6
    "85 F3"    //   STA $F3
    "4C 00 CB" //   JMP LIST
    "E6 41"    // DOPOKE: INC $41           (skip ':')
    "4C 00 CC" //   JMP POKE
    "A6 41"    // MAYBERUN: LDX $41
    "E8"       //   INX                     (skip the space)
    "E4 40"    //   CPX $40
    "B0 09"    //   BCS ERROR               (nothing after space)
    "B5 00"    //   LDA $00,X
    "C9 52"    //   CMP #$52                ('R')
    "D0 03"    //   BNE ERROR
    "4C 00 CD" //   JMP RUN
    "A9 3F"    // ERROR: LDA #$3F           ('?')
    "20 00 C1" //   JSR PUTCHAR
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      //   RTS

// PEEK ($CA00): prints ADDR ($F0/$F1) as a 4-digit hex address, ": ", the
// byte at that address as 2 hex digits, then CRLF.
const std::string kPeekHex =
    "A5 F1"    // PEEK: LDA $F1             (addr hi)
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A5 F0"    //   LDA $F0                 (addr lo)
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 3A"    //   LDA #$3A                (':')
    "20 00 C1" //   JSR PUTCHAR
    "A9 20"    //   LDA #$20                (' ')
    "20 00 C1" //   JSR PUTCHAR
    "A0 00"    //   LDY #$00
    "B1 F0"    //   LDA ($F0),Y             (byte at ADDR)
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      //   RTS

// LIST ($CB00): prints START ($F0/$F1) through END ($F2/$F3) inclusive,
// 16 bytes per row, each row prefixed with its own start address. The
// newline for a full row is printed eagerly (before the next row's
// prefix, not after the 16th byte) so a range that stops exactly on a row
// boundary doesn't get a stray blank prefix or a doubled CRLF at STOP.
const std::string kListHex =
    "A9 00"    // LIST: LDA #$00
    "85 F7"    //   STA $F7                 (ROWCOUNT = 0)
    "A5 F1"    // LOOP: LDA $F1             (START hi)
    "C5 F3"    //   CMP $F3                 (END hi)
    "90 0A"    //   BCC CONTINUE
    "D0 4B"    //   BNE STOP
    "A5 F0"    //   LDA $F0                 (START lo)
    "C5 F2"    //   CMP $F2                 (END lo)
    "F0 02"    //   BEQ CONTINUE
    "B0 43"    //   BCS STOP
    "A5 F7"    // CONTINUE: LDA $F7
    "D0 14"    //   BNE SKIP_PREFIX
    "A5 F1"    //   LDA $F1
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A5 F0"    //   LDA $F0
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 3A"    //   LDA #$3A                (':')
    "20 00 C1" //   JSR PUTCHAR
    "A9 20"    //   LDA #$20                (' ')
    "20 00 C1" //   JSR PUTCHAR
    "A0 00"    // SKIP_PREFIX: LDY #$00
    "B1 F0"    //   LDA ($F0),Y
    "20 00 C2" //   JSR PRINT_HEX_BYTE
    "A9 20"    //   LDA #$20                (' ')
    "20 00 C1" //   JSR PUTCHAR
    "E6 F0"    //   INC $F0
    "D0 02"    //   BNE NOCARRY
    "E6 F1"    //   INC $F1
    "E6 F7"    // NOCARRY: INC $F7
    "A5 F7"    //   LDA $F7
    "C9 10"    //   CMP #$10
    "90 BE"    //   BCC LOOP
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "A9 00"    //   LDA #$00
    "85 F7"    //   STA $F7                 (ROWCOUNT = 0)
    "4C 04 CB" //   JMP LOOP
    "A5 F7"    // STOP: LDA $F7
    "F0 0A"    //   BEQ DONE_NOCRLF
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      // DONE_NOCRLF: RTS

// POKE ($CC00): entered with LINEPOS just past ':'. Skips spaces, parses
// one hex byte token at a time via PARSE_BYTE, writes each to consecutive
// addresses starting at ADDR ($F0/$F1), stops at end of line or the first
// invalid token.
const std::string kPokeHex =
    "A5 41"    // POKE/LOOP: LDA $41
    "C5 40"    //   CMP $40
    "B0 21"    //   BCS DONE
    "A6 41"    //   LDX $41
    "B5 00"    //   LDA $00,X
    "C9 20"    //   CMP #$20                (space?)
    "D0 05"    //   BNE PARSEBYTE
    "E6 41"    //   INC $41                 (skip space)
    "4C 00 CC" //   JMP LOOP
    "20 00 C6" // PARSEBYTE: JSR PARSE_BYTE
    "B0 0F"    //   BCS DONE                (invalid -> stop)
    "A0 00"    //   LDY #$00
    "A5 F4"    //   LDA $F4
    "91 F0"    //   STA ($F0),Y             (write byte at ADDR)
    "E6 F0"    //   INC $F0
    "D0 02"    //   BNE NOCARRY
    "E6 F1"    //   INC $F1
    "4C 00 CC" // NOCARRY: JMP LOOP
    "60";      // DONE: RTS

// RUN ($CD00): ROM::write() is a no-op, so the firmware can't
// self-modify a JSR operand in place. Instead it builds a 4-byte
// trampoline in RAM at $F8-$FB ("JSR <addr>" + "RTS") and JSRs to that.
// A user-program RTS returns into the trampoline's own RTS, which returns
// to DISPATCH's caller.
const std::string kRunHex =
    "A9 20"    // RUN: LDA #$20             (JSR opcode)
    "85 F8"    //   STA $F8
    "A5 F0"    //   LDA $F0
    "85 F9"    //   STA $F9
    "A5 F1"    //   LDA $F1
    "85 FA"    //   STA $FA
    "A9 60"    //   LDA #$60                (RTS opcode)
    "85 FB"    //   STA $FB
    "20 F8 00" //   JSR $00F8               (execute trampoline)
    "60";      //   RTS
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines`
Expected: all `MonitorRoutines` cases (16 total across Tasks 2–4) PASS.

- [ ] **Step 6: Commit**

```bash
git add src/system/monitor_firmware.h src/system/monitor_firmware.cpp test/system/monitor_routines_test.cpp
git commit -m "feat: add monitor firmware command dispatch (DISPATCH/PEEK/LIST/POKE/RUN)"
```

---

### Task 5: `READ_LINE`, cold/warm start, `install()`, and the full interactive e2e tests

This is where the firmware becomes a real, bootable system for the first time — `install()` wires every routine plus the reset/BRK vectors, so `cpu.reset()` now boots into an actual interactive prompt.

**Files:**
- Modify: `src/system/monitor_firmware.h`
- Modify: `src/system/monitor_firmware.cpp`
- Create: `test/system/monitor_test_helpers.h`
- Create: `test/system/monitor_firmware_test.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: every routine from Tasks 2–4.
- Produces: `kReadLineAddr = 0xC400`, `kBannerAddr = 0xCE00`, `kPromptAddr = 0xCE0F`, `kColdStartAddr = 0xCF00`, `kWarmStartAddr = 0xCF0E`, `kMainLoopAddr = 0xCF11`, `void monitor::install(MemoryDevice &rom)` — loads every routine at its fixed address plus the NMI/RESET/BRK vectors, so callers just do `monitor::install(rom); cpu.reset();` and get a running monitor. `typeChar()`/`typeLine()` test helpers (`test/system/monitor_test_helpers.h`) that later tasks (System tests) reuse.

- [ ] **Step 1: Add the remaining address constants and `extern` hex declarations, plus `install()`**

Edit `src/system/monitor_firmware.h`, add after `kRunAddr`:

```cpp
constexpr uint16_t kReadLineAddr = 0xC400;
constexpr uint16_t kBannerAddr = 0xCE00;
constexpr uint16_t kPromptAddr = 0xCE0F;
constexpr uint16_t kColdStartAddr = 0xCF00;
constexpr uint16_t kWarmStartAddr = 0xCF0E;
constexpr uint16_t kMainLoopAddr = 0xCF11;
```

And after `extern const std::string kRunHex;`, add:

```cpp
extern const std::string kReadLineHex;
extern const std::string kDataHex;
extern const std::string kMainHex;

// Loads every monitor routine plus the NMI/RESET/BRK vectors into `rom`.
// After this, cpu.reset() boots straight into the interactive prompt.
void install(MemoryDevice &rom);
```

- [ ] **Step 2: Write the shared `typeChar`/`typeLine` test helpers**

Create `test/system/monitor_test_helpers.h`:

```cpp
#pragma once

#include "cpu/cpu6502.h"
#include "peripherals/tty.h"

#include <cstdint>
#include <string>

// Delivers one byte through the TTY the way a real keystroke would: injects
// it, then pumps the CPU until GETCHAR has actually consumed it (rxReady()
// goes back to false), bounded so a firmware bug shows up as a test
// failure instead of a hang.
inline void typeChar(CPU6502 &cpu, TTY &tty, uint8_t byte) {
    tty.receive(byte);
    for (int i = 0; i < 2000 && tty.rxReady(); ++i) {
        cpu.executeInstruction();
    }
}

// Types every character of `line`, then a trailing CR, then pumps a
// generous fixed instruction budget so the firmware finishes dispatching
// the command and settles back at GETCHAR's wait loop before returning.
inline void typeLine(CPU6502 &cpu, TTY &tty, const std::string &line) {
    for (char c : line) {
        typeChar(cpu, tty, static_cast<uint8_t>(c));
    }
    typeChar(cpu, tty, 0x0D);
    for (int i = 0; i < 20000; ++i) {
        cpu.executeInstruction();
    }
}
```

- [ ] **Step 3: Write the failing end-to-end tests**

Create `test/system/monitor_firmware_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"
#include "monitor_test_helpers.h"
#include "peripherals/tty.h"
#include "system/monitor_firmware.h"

#include <sstream>
#include <vector>

namespace {
struct MonitorFixture {
    RAM ram{0x8000};
    std::ostringstream output;
    TTY tty{output};
    ROM rom{std::vector<uint8_t>(monitor::kRomSize)};
    Bus bus;
    CPU6502 cpu{bus};

    MonitorFixture() {
        bus.attach(0x0000, 0x7FFF, ram);
        bus.attach(0x8000, 0x8001, tty);
        bus.attach(0xC000, 0xFFFF, rom);
        monitor::install(rom);
    }
};

constexpr const char *kBannerAndPrompt = "sys6 monitor\r\n> ";
} // namespace

TEST(MonitorFirmwareE2E, ColdStartPrintsBannerThenPrompt) {
    MonitorFixture fx;
    fx.cpu.reset();
    for (int i = 0; i < 5000; ++i) {
        fx.cpu.executeInstruction();
    }
    EXPECT_EQ(fx.output.str(), kBannerAndPrompt);
}

TEST(MonitorFirmwareE2E, EchoAppearsBeforeCommandOutput) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050");
    std::string expected = kBannerAndPrompt;
    expected += "0050\r\n0050: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, PeekReturnsKnownByte) {
    MonitorFixture fx;
    fx.ram.write(0x0300, 0x7E);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0300");
    std::string expected = kBannerAndPrompt;
    expected += "0300\r\n0300: 7E\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, PokeThenPeekRoundTrips) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0400: 11 22");
    typeLine(fx.cpu, fx.tty, "0400");
    std::string expected = kBannerAndPrompt;
    expected += "0400: 11 22\r\n> 0400\r\n0400: 11\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, ListSpansMultipleRows) {
    MonitorFixture fx;
    for (int i = 0; i < 18; ++i) {
        fx.ram.write(static_cast<uint16_t>(0x0500 + i), static_cast<uint8_t>(i));
    }
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0500.0511");
    std::string expected = kBannerAndPrompt;
    expected += "0500.0511\r\n"
                 "0500: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F \r\n"
                 "0510: 10 11 \r\n"
                 "> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, RunViaRtsExecutesProgramAndReturnsToPrompt) {
    MonitorFixture fx;
    // INC $80 ; RTS
    fx.ram.write(0x0600, 0xE6);
    fx.ram.write(0x0601, 0x80);
    fx.ram.write(0x0602, 0x60);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0600 R");
    typeLine(fx.cpu, fx.tty, "0080");
    std::string expected = kBannerAndPrompt;
    expected += "0600 R\r\n> 0080\r\n0080: 01\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, RunViaBrkReachesWarmStartPromptNotBanner) {
    MonitorFixture fx;
    fx.ram.write(0x0700, 0x00); // BRK
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0700 R");
    typeLine(fx.cpu, fx.tty, "0000");
    std::string expected = kBannerAndPrompt;
    // No second banner after the BRK -- just the warm-start prompt -- and
    // the monitor is still usable afterward.
    expected += "0700 R\r\n> 0000\r\n0000: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, BackspaceCorrectsTypedAddress) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeChar(fx.cpu, fx.tty, '0');
    typeChar(fx.cpu, fx.tty, '0');
    typeChar(fx.cpu, fx.tty, '5');
    typeChar(fx.cpu, fx.tty, 0x7F); // backspace: erases the '5'
    typeChar(fx.cpu, fx.tty, '5');
    typeChar(fx.cpu, fx.tty, '0');
    typeChar(fx.cpu, fx.tty, 0x0D); // CR: dispatches "0050"
    for (int i = 0; i < 20000; ++i) {
        fx.cpu.executeInstruction();
    }
    std::string expected = kBannerAndPrompt;
    expected += "005";
    expected += "\x08 \x08"; // visual erase of the typo'd '5'
    expected += "50\r\n0050: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, MalformedLineThenValidCommandRecovers) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "ZZ");
    typeLine(fx.cpu, fx.tty, "0000");
    std::string expected = kBannerAndPrompt;
    expected += "ZZ\r\n?\r\n> 0000\r\n0000: 00\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, MultiCommandSessionChainsCleanly) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0200: AA BB CC");
    typeLine(fx.cpu, fx.tty, "0200");
    typeLine(fx.cpu, fx.tty, "0200.0202");
    // DEC $80 ; RTS
    fx.ram.write(0x0210, 0xC6);
    fx.ram.write(0x0211, 0x80);
    fx.ram.write(0x0212, 0x60);
    fx.ram.write(0x0080, 0x05);
    typeLine(fx.cpu, fx.tty, "0210 R");
    typeLine(fx.cpu, fx.tty, "0080");
    std::string expected = kBannerAndPrompt;
    expected += "0200: AA BB CC\r\n> "
                 "0200\r\n0200: AA\r\n> "
                 "0200.0202\r\n0200: AA BB CC \r\n> "
                 "0210 R\r\n> "
                 "0080\r\n0080: 04\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}

TEST(MonitorFirmwareE2E, AddressEdgeCasesParseCorrectly) {
    MonitorFixture fx;
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "5"); // 1-digit, zero-extends to $0005
    typeLine(fx.cpu, fx.tty, "FFFF"); // top of the address space: BRK-vector hi byte
    std::string expected = kBannerAndPrompt;
    expected += "5\r\n0005: 00\r\n> FFFF\r\nFFFF: CF\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
}
```

- [ ] **Step 4: Add the new test file to the test build**

Edit `test/CMakeLists.txt`: add `system/monitor_firmware_test.cpp` to the test source list (`monitor_firmware.cpp` is already listed from Task 2).

- [ ] **Step 5: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests 2>&1 | tail -30`
Expected: FAIL to compile (`kReadLineHex`, `kDataHex`, `kMainHex`, `monitor::install` undefined).

- [ ] **Step 6: Implement `READ_LINE`, cold/warm start, data strings, and `install()`**

Edit `src/system/monitor_firmware.cpp`, append after `kRunHex`'s definition:

```cpp
// READ_LINE ($C400): reads characters via GETCHAR into LINEBUF (indexed by
// X), echoing each one via PUTCHAR as it's read (raw mode means the OS
// won't echo for us), until CR. Backspace (DEL $7F or BS $08) erases the
// last buffered character both in the buffer and visually ("\b \b"). A
// full buffer (64 chars) silently drops further characters rather than
// overflowing. Sets LINELEN to the final character count.
const std::string kReadLineHex =
    "A2 00"    // LDX #$00                 (X = 0, line length)
    "20 00 C0" // LOOP: JSR GETCHAR
    "C9 0D"    //   CMP #$0D                (CR?)
    "F0 2C"    //   BEQ ENDLINE
    "C9 7F"    //   CMP #$7F                (DEL?)
    "F0 11"    //   BEQ BACKSPACE
    "C9 08"    //   CMP #$08                (BS?)
    "F0 0D"    //   BEQ BACKSPACE
    "E0 40"    //   CPX #$40                (buffer full?)
    "B0 ED"    //   BCS LOOP                (drop char if full)
    "95 00"    //   STA $00,X               (LINEBUF[X] = A)
    "20 00 C1" //   JSR PUTCHAR             (echo)
    "E8"       //   INX
    "4C 02 C4" //   JMP LOOP
    "E0 00"    // BACKSPACE: CPX #$00
    "F0 E0"    //   BEQ LOOP                (nothing to erase)
    "CA"       //   DEX
    "A9 08"    //   LDA #$08
    "20 00 C1" //   JSR PUTCHAR             (backspace)
    "A9 20"    //   LDA #$20
    "20 00 C1" //   JSR PUTCHAR             (space)
    "A9 08"    //   LDA #$08
    "20 00 C1" //   JSR PUTCHAR             (backspace)
    "4C 02 C4" //   JMP LOOP
    "A9 0D"    // ENDLINE: LDA #$0D
    "20 00 C1" //   JSR PUTCHAR             (CR)
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR             (LF)
    "86 40"    //   STX $40                 (LINELEN = X)
    "60";      //   RTS

// Data at $CE00: BANNER ("sys6 monitor\r\n\0", 15 bytes) followed
// immediately by PROMPT ("> \0", 3 bytes) at $CE0F.
const std::string kDataHex =
    "73 79 73 36 20 6D 6F 6E 69 74 6F 72 0D 0A 00" // "sys6 monitor\r\n\0"
    "3E 20 00";                                     // "> \0"

// COLD_START ($CF00): prints the banner, falls into MAIN_LOOP.
// WARM_START ($CF0E, the BRK vector target): resets SP (so repeated
// accidental BRKs during a long session don't let the stack drift) then
// falls into MAIN_LOOP without the banner.
// MAIN_LOOP ($CF11): prints the prompt, reads a line, parses the leading
// address, dispatches it (or prints '?' on a parse failure), repeats
// forever.
const std::string kMainHex =
    "A9 00"    // COLD_START: LDA #$00
    "85 FC"    //   STA $FC                 (STRPTR lo = BANNER lo)
    "A9 CE"    //   LDA #$CE
    "85 FD"    //   STA $FD                 (STRPTR hi = BANNER hi)
    "20 00 C3" //   JSR PRINT_STRING        (print banner)
    "4C 11 CF" //   JMP MAIN_LOOP
    "A2 FF"    // WARM_START: LDX #$FF
    "9A"       //   TXS                     (reset stack pointer)
    "A9 0F"    // MAIN_LOOP: LDA #$0F
    "85 FC"    //   STA $FC                 (STRPTR lo = PROMPT lo)
    "A9 CE"    //   LDA #$CE
    "85 FD"    //   STA $FD                 (STRPTR hi = PROMPT hi)
    "20 00 C3" //   JSR PRINT_STRING        (print prompt)
    "20 00 C4" //   JSR READ_LINE
    "A9 00"    //   LDA #$00
    "85 41"    //   STA $41                 (LINEPOS = 0)
    "20 00 C5" //   JSR PARSE_ADDR
    "B0 06"    //   BCS BADLINE
    "20 00 C8" //   JSR DISPATCH
    "4C 11 CF" //   JMP MAIN_LOOP
    "A9 3F"    // BADLINE: LDA #$3F         ('?')
    "20 00 C1" //   JSR PUTCHAR
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "4C 11 CF"; //   JMP MAIN_LOOP

void install(MemoryDevice &rom) {
    loadRoutine(rom, kGetCharAddr, kGetCharHex);
    loadRoutine(rom, kPutCharAddr, kPutCharHex);
    loadRoutine(rom, kPrintHexByteAddr, kPrintHexByteHex);
    loadRoutine(rom, kPrintNibbleAddr, kPrintNibbleHex);
    loadRoutine(rom, kPrintStringAddr, kPrintStringHex);
    loadRoutine(rom, kReadLineAddr, kReadLineHex);
    loadRoutine(rom, kParseAddrAddr, kParseAddrHex);
    loadRoutine(rom, kParseByteAddr, kParseByteHex);
    loadRoutine(rom, kHexValAddr, kHexValHex);
    loadRoutine(rom, kDispatchAddr, kDispatchHex);
    loadRoutine(rom, kPeekAddr, kPeekHex);
    loadRoutine(rom, kListAddr, kListHex);
    loadRoutine(rom, kPokeAddr, kPokeHex);
    loadRoutine(rom, kRunAddr, kRunHex);
    loadRoutine(rom, kBannerAddr, kDataHex);
    loadRoutine(rom, kColdStartAddr, kMainHex);
    loadRoutine(rom, 0xFFFA, "0E CF"); // NMI vector -> WARM_START
    loadRoutine(rom, 0xFFFC, "00 CF"); // RESET vector -> COLD_START
    loadRoutine(rom, 0xFFFE, "0E CF"); // BRK vector -> WARM_START
}
```

- [ ] **Step 7: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R "MonitorFirmwareE2E|MonitorRoutines"`
Expected: all `MonitorRoutines` (16) and `MonitorFirmwareE2E` (11) cases PASS. If any `MonitorFirmwareE2E` string comparison fails, `ctest --output-on-failure` prints both the actual and expected strings — diff them and fix whichever is wrong (a byte in `kMainHex`/a routine, or the test's expected string).

- [ ] **Step 8: Commit**

```bash
git add src/system/monitor_firmware.h src/system/monitor_firmware.cpp \
        test/system/monitor_test_helpers.h test/system/monitor_firmware_test.cpp \
        test/CMakeLists.txt
git commit -m "feat: complete monitor firmware (READ_LINE, cold/warm start, install()) with e2e tests"
```

---

### Task 6: `TerminalIO` interface, `FakeTerminalIO`, and `System`

**Files:**
- Create: `src/system/terminal_io.h`
- Create: `test/system/fake_terminal_io.h`
- Create: `src/system/system.h`
- Create: `src/system/system.cpp`
- Create: `test/system/system_test.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `monitor::install()` (Task 5), `TTY` (Task 1).
- Produces: `class TerminalIO { virtual std::optional<uint8_t> tryReadByte() = 0; }`. `class FakeTerminalIO : public TerminalIO` (test-only) with `void push(uint8_t)`. `class System { System(TerminalIO&, std::ostream&); void reset(); void run(); void step(); }`.

- [ ] **Step 1: Write `TerminalIO` and `FakeTerminalIO`**

Create `src/system/terminal_io.h`:

```cpp
#pragma once

#include <cstdint>
#include <optional>

// Input-only seam between System and the real OS. Output doesn't need one
// -- TTY already takes a plain std::ostream&, so a test can capture output
// with an std::ostringstream independent of however input is faked here.
class TerminalIO {
public:
    virtual ~TerminalIO() = default;
    virtual std::optional<uint8_t> tryReadByte() = 0;
};
```

Create `test/system/fake_terminal_io.h`:

```cpp
#pragma once

#include "system/terminal_io.h"

#include <deque>

class FakeTerminalIO : public TerminalIO {
public:
    void push(uint8_t byte) { m_queue.push_back(byte); }

    std::optional<uint8_t> tryReadByte() override {
        if (m_queue.empty()) {
            return std::nullopt;
        }
        uint8_t b = m_queue.front();
        m_queue.pop_front();
        return b;
    }

private:
    std::deque<uint8_t> m_queue;
};
```

- [ ] **Step 2: Write the failing `System` tests**

Create `test/system/system_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "fake_terminal_io.h"
#include "system/system.h"

#include <sstream>

TEST(SystemTest, StepInjectsAvailableByteAndAdvancesTheFirmware) {
    FakeTerminalIO term;
    std::ostringstream out;
    System system(term, out);
    system.reset();
    for (char c : std::string("0000")) {
        term.push(static_cast<uint8_t>(c));
    }
    term.push(0x0D);
    for (int i = 0; i < 20000; ++i) {
        system.step();
    }
    EXPECT_EQ(out.str(), "sys6 monitor\r\n> 0000\r\n0000: 00\r\n> ");
}

TEST(SystemTest, StepDoesNotFabricateInputWhenNoneIsAvailable) {
    FakeTerminalIO term;
    std::ostringstream out;
    System system(term, out);
    system.reset();
    for (int i = 0; i < 2000; ++i) {
        system.step();
    }
    EXPECT_EQ(out.str(), "sys6 monitor\r\n> ");
}
```

- [ ] **Step 3: Add the new files to the test build**

Edit `test/CMakeLists.txt`: add `system/system_test.cpp` to the test source list and `${CMAKE_SOURCE_DIR}/src/system/system.cpp` to the production source list.

- [ ] **Step 4: Run tests to verify they fail**

Run: `cmake --build build --target sys6_tests 2>&1 | tail -30`
Expected: FAIL to compile (`system/system.h` doesn't exist yet).

- [ ] **Step 5: Write `System`**

Create `src/system/system.h`:

```cpp
#pragma once

#include "cpu/cpu6502.h"
#include "memory/bus.h"
#include "memory/ram.h"
#include "memory/rom.h"
#include "peripherals/tty.h"
#include "system/terminal_io.h"

#include <ostream>

class System {
public:
    System(TerminalIO &term, std::ostream &out);

    void reset(); // resets the CPU -- exposed so tests can drive step()
                   // from a known state without going through run()'s
                   // infinite loop
    void run();    // reset() once, then loops step() forever
    void step();   // one read/inject/execute iteration

private:
    TerminalIO &m_term;
    RAM m_ram;
    TTY m_tty;
    ROM m_rom;
    Bus m_bus;
    CPU6502 m_cpu;
};
```

Create `src/system/system.cpp`:

```cpp
#include "system.h"

#include "system/monitor_firmware.h"

namespace {
constexpr uint16_t kRamSize = 0x8000;
} // namespace

System::System(TerminalIO &term, std::ostream &out)
    : m_term(term), m_ram(kRamSize), m_tty(out), m_rom(std::vector<uint8_t>(monitor::kRomSize)),
      m_cpu(m_bus) {
    m_bus.attach(0x0000, 0x7FFF, m_ram);
    m_bus.attach(0x8000, 0x8001, m_tty);
    m_bus.attach(0xC000, 0xFFFF, m_rom);
    monitor::install(m_rom);
}

void System::reset() { m_cpu.reset(); }

void System::step() {
    if (auto b = m_term.tryReadByte()) {
        m_tty.receive(*b);
    }
    m_cpu.executeInstruction();
}

void System::run() {
    reset();
    while (true) {
        step();
    }
}
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R SystemTest`
Expected: both `SystemTest` cases PASS.

- [ ] **Step 7: Commit**

```bash
git add src/system/terminal_io.h src/system/system.h src/system/system.cpp \
        test/system/fake_terminal_io.h test/system/system_test.cpp test/CMakeLists.txt
git commit -m "feat: add System class wiring TTY/ROM/RAM/CPU with a TerminalIO seam"
```

---

### Task 7: `PosixTerminalIO` and the `sys6-monitor` executable

No automated test here — a real raw-mode terminal isn't something CI can meaningfully fake. This task's verification is: it builds, and a documented manual smoke test works.

**Files:**
- Create: `src/system/posix_terminal_io.h`
- Create: `src/system/posix_terminal_io.cpp`
- Create: `src/system/monitor_main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `TerminalIO` (Task 6), `System` (Task 6).
- Produces: `class PosixTerminalIO : public TerminalIO`.

- [ ] **Step 1: Write `PosixTerminalIO`**

Create `src/system/posix_terminal_io.h`:

```cpp
#pragma once

#include "system/terminal_io.h"

#include <termios.h>

// Puts real stdin into raw mode (no OS echo, no line buffering, non-
// blocking reads) for the duration of its lifetime, and restores the
// original settings on destruction -- RAII, so terminal state is always
// cleaned up on the way out, including on exceptions.
class PosixTerminalIO : public TerminalIO {
public:
    PosixTerminalIO();
    ~PosixTerminalIO() override;

    PosixTerminalIO(const PosixTerminalIO &) = delete;
    PosixTerminalIO &operator=(const PosixTerminalIO &) = delete;

    std::optional<uint8_t> tryReadByte() override;

private:
    struct termios m_savedTermios {};
};
```

Create `src/system/posix_terminal_io.cpp`:

```cpp
#include "posix_terminal_io.h"

#include <unistd.h>

PosixTerminalIO::PosixTerminalIO() {
    tcgetattr(STDIN_FILENO, &m_savedTermios);
    struct termios raw = m_savedTermios;
    raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON | ECHO));
    raw.c_oflag &= ~(static_cast<tcflag_t>(OPOST));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

PosixTerminalIO::~PosixTerminalIO() { tcsetattr(STDIN_FILENO, TCSANOW, &m_savedTermios); }

std::optional<uint8_t> PosixTerminalIO::tryReadByte() {
    unsigned char byte = 0;
    ssize_t n = read(STDIN_FILENO, &byte, 1);
    if (n == 1) {
        return byte;
    }
    return std::nullopt;
}
```

- [ ] **Step 2: Write `monitor_main.cpp`**

Create `src/system/monitor_main.cpp`:

```cpp
#include "system/posix_terminal_io.h"
#include "system/system.h"

#include <iostream>

int main() {
    PosixTerminalIO term;
    System system(term, std::cout);
    system.run();
    return 0;
}
```

- [ ] **Step 3: Add the `sys6-monitor` target**

Edit `CMakeLists.txt`, add immediately after the existing `target_include_directories(sys6 PRIVATE src)` line:

```cmake
add_executable(sys6-monitor
    src/system/monitor_main.cpp
    src/system/system.cpp
    src/system/posix_terminal_io.cpp
    src/system/monitor_firmware.cpp
    src/peripherals/tty.cpp
    src/cpu/alu.cpp
    src/cpu/cpu6502.cpp
    src/memory/ram.cpp
    src/memory/rom.cpp
    src/memory/bus.cpp
    src/utils/log.cpp
    src/utils/program_loader.cpp
)

target_include_directories(sys6-monitor PRIVATE src)
```

- [ ] **Step 4: Verify it builds**

Run: `cmake -S . -B build && cmake --build build --target sys6-monitor 2>&1 | tail -30`
Expected: builds successfully, produces `build/bin/sys6-monitor` (or wherever `CMAKE_RUNTIME_OUTPUT_DIRECTORY` points — check with `find build -name sys6-monitor -type f`).

- [ ] **Step 5: Manual smoke test**

Run the binary directly in an interactive terminal (not through a pipe, or `tryReadByte()` will just see EOF): `./build/bin/sys6-monitor` (adjust path per Step 4's `find` result). Expect to see `sys6 monitor` followed by a `> ` prompt. Type `0000` and press Enter — expect `0000: 00` echoed back followed by a fresh prompt. Press Ctrl-C to exit (there's no `QUIT` command in this "very basic" monitor; exiting is a Non-goal-adjacent detail intentionally left to the terminal's own signal handling).

- [ ] **Step 6: Commit**

```bash
git add src/system/posix_terminal_io.h src/system/posix_terminal_io.cpp \
        src/system/monitor_main.cpp CMakeLists.txt
git commit -m "feat: add sys6-monitor executable (PosixTerminalIO + System wired to real stdin/stdout)"
```

---

### Task 8: `README.md` — "Running the monitor"

**Files:**
- Modify: `README.md`

**Interfaces:** None (documentation only).

- [ ] **Step 1: Add the section**

Edit `README.md`, insert a new section after the existing "## Testing" section (before "## Formatting and linting"):

```markdown
## Running the monitor

`sys6-monitor` boots the emulated CPU straight into a hand-assembled 6502
monitor program (in the spirit of WozMon/the KIM-1 monitor), talking to
your real terminal through an emulated serial peripheral. Build and run
it with:

    cmake --build build --target sys6-monitor
    ./build/bin/sys6-monitor

You'll see a `sys6 monitor` banner and a `>` prompt. Commands:

| Syntax              | Effect                                                |
|---------------------|--------------------------------------------------------|
| `AAAA`              | peek: prints the byte at address `AAAA`                 |
| `AAAA.BBBB`          | list: hex-dumps that inclusive range, 16 bytes/row       |
| `AAAA: BB BB BB`     | poke: writes the given bytes starting at `AAAA`          |
| `AAAA R`             | run: jumps to `AAAA`; returns to the prompt on `RTS`      |

Addresses and byte values are hex, 1–4 and 1–2 digits respectively
(shorter values are zero-extended). Backspace corrects a typo before you
press Enter. An unrecognized line prints `?` and returns to the prompt
without changing memory.
```

- [ ] **Step 2: Verify the build commands in the new section actually work**

Run: `cmake --build build --target sys6-monitor && find build -name sys6-monitor -type f`
Expected: matches the path written in the README (adjust the README's `./build/bin/sys6-monitor` line if the actual output path differs).

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: document sys6-monitor build/run instructions and command syntax"
```
