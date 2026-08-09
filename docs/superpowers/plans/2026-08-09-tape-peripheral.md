# Tape peripheral Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the monitor firmware save a range of memory to a tape image and load it back, via a memory-mapped tape register group folded into the existing `TTY` peripheral — no separate peripheral, one 256-byte I/O page.

**Architecture:** `TTY` grows from a 2-byte serial-only device into a 256-byte `MemoryDevice`: offsets `0x00`/`0x01` keep today's serial STATUS/DATA behavior unchanged, offsets `0x02`–`0x04` add a TAPE_STATUS/TAPE_CONTROL/TAPE_DATA register group backed by an optional `std::iostream *` (a real file at the host boundary, a `std::stringstream` in tests), and every other offset in the page reads `0x00`/ignores writes. Three new hand-assembled 6502 routines (`SAVE`/`LOAD`/`REWIND`) talk to those registers exactly the way `GETCHAR`/`PUTCHAR` already talk to the serial ones, and `DISPATCH` grows two new routing branches (`.`+`" S"`, and `" L"`/`" W"` alongside the existing `" R"`) to reach them.

**Tech Stack:** C++17, CMake, GoogleTest/CTest — same as the rest of the project. No new dependencies.

## Global Constraints

- Never modify `CPU6502` (`src/cpu/cpu6502.h`/`.cpp`) — per `CLAUDE.md`, it is the frozen reference implementation.
- `ROM::write()` is a no-op; firmware bytes are always loaded via `monitor::loadRoutine(rom, busAddr, hex)` (which uses `ROM::load()`), never `loadProgram()`.
- Hex-string routines follow the existing convention: one string literal per instruction, each with a `//` comment giving the mnemonic and, where non-obvious, what it does — see any existing routine in `src/system/monitor_firmware.cpp` for the exact style.
- Every routine gets its own fixed-address 256-byte ROM page, same as every existing routine — this is why branch math never has to account for a routine's neighbors shifting.
- New/changed C++ follows the existing style: `#pragma once` headers, `explicit` single-argument (or defaulted-second-argument) constructors, member-init-list order matching declaration order.
- `Bus::attach()` throws if `(end - start + 1) != device.size()` — every existing call site that attaches `TTY` (`src/system/system.cpp`, `test/system/monitor_firmware_test.cpp`, `test/system/routine_test_fixture.h`) currently attaches `0x8000`–`0x8001` and **must** change to `0x8000`–`0x80FF` in the same task that changes `TTY::size()`, or the entire test suite starts throwing at fixture construction.

---

## Firmware address map (reference for every task below)

Zero page (additions only — existing entries in `src/system/monitor_firmware.h` are unchanged):

| Address | Name | Purpose |
|---|---|---|
| `$42`/`$43` | `TAPE_LEN` lo/hi | 16-bit data-byte count, written/read as the tape block's length header |
| `$44` | `TAPE_CHECKSUM` | running XOR accumulator during `SAVE`/`LOAD` |

TTY tape registers (bus addresses, offsets within the `TTY` page):

| Address | Register | Access |
|---|---|---|
| `$8002` | `TAPE_STATUS` | RO — bit0 PRESENT, bit1 MOTOR, bit2 EOT, bit3 ERROR |
| `$8003` | `TAPE_CONTROL` | WO — bit0 MOTOR on/off, bit1 REWIND (strobe) |
| `$8004` | `TAPE_DATA` | RW — one byte, cursor-driven |

New ROM routines (each its own page):

| Address | Routine | Added in |
|---|---|---|
| `$D000` | `SAVE` | Task 4 |
| `$D100` | `LOAD` | Task 5 |
| `$D200` | `REWIND` | Task 3 |

`DISPATCH` (`$C800`) is rewritten in place in Task 6 — its address doesn't change.

On-tape block format: `[LEN_LO][LEN_HI][DATA...][CHECKSUM]`, `LEN` 16-bit little-endian count of data bytes, `CHECKSUM` = XOR of every data byte.

---

### Task 1: `TTY` grows into a 256-byte tape-capable I/O page

**Files:**
- Modify: `src/peripherals/tty.h`
- Modify: `src/peripherals/tty.cpp`
- Modify: `src/system/system.cpp:13` (bus.attach range)
- Modify: `test/system/monitor_firmware_test.cpp:24` (bus.attach range)
- Modify: `test/system/routine_test_fixture.h:29` (bus.attach range)
- Create: `test/peripherals/tty_tape_test.cpp`
- Modify: `test/CMakeLists.txt` (register the new test file)

**Interfaces:**
- Produces: `explicit TTY(std::ostream &out, std::iostream *tapeBacking = nullptr)`. `size()` now returns `256`. Existing `receive(uint8_t)`/`rxReady() const` are unchanged. No new public methods — tape state is only reachable through `read()`/`write()` at offsets `0x02`–`0x04`, exactly like the serial registers at `0x00`/`0x01`.

- [ ] **Step 1: Write the failing tests**

Create `test/peripherals/tty_tape_test.cpp`:

```cpp
#include <gtest/gtest.h>

#include "peripherals/tty.h"

#include <sstream>

namespace {
constexpr uint16_t kTapeStatus = 0x02;
constexpr uint16_t kTapeControl = 0x03;
constexpr uint16_t kTapeData = 0x04;
constexpr uint8_t kPresent = 0x01;
constexpr uint8_t kMotor = 0x02;
constexpr uint8_t kEot = 0x04;
constexpr uint8_t kError = 0x08;
} // namespace

TEST(TTYTapeTest, SizeIsA256Byteage) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.size(), 256u);
}

TEST(TTYTapeTest, ReservedOffsetsReadZeroAndIgnoreWrites) {
    std::ostringstream out;
    TTY tty(out);
    EXPECT_EQ(tty.read(0x05), 0);
    EXPECT_EQ(tty.read(0xFF), 0);
    tty.write(0x05, 0xAB);
    EXPECT_EQ(tty.read(0x05), 0);
}

TEST(TTYTapeTest, PresentBitReflectsWhetherABackingWasGiven) {
    std::ostringstream out;
    TTY noTape(out);
    EXPECT_EQ(noTape.read(kTapeStatus) & kPresent, 0);

    std::stringstream tape;
    TTY withTape(out, &tape);
    EXPECT_EQ(withTape.read(kTapeStatus) & kPresent, kPresent);
}

TEST(TTYTapeTest, ControlWriteSetsAndClearsMotorBitInStatus) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    EXPECT_EQ(tty.read(kTapeStatus) & kMotor, 0);
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeStatus) & kMotor, kMotor);
    tty.write(kTapeControl, 0x00);
    EXPECT_EQ(tty.read(kTapeStatus) & kMotor, 0);
}

TEST(TTYTapeTest, DataReadWriteRoundTripsThroughTheBackingWithMotorOn) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01); // motor on
    tty.write(kTapeData, 0xAA);
    tty.write(kTapeData, 0xBB);
    tty.write(kTapeControl, 0x02); // rewind (also turns motor off)
    tty.write(kTapeControl, 0x01); // motor back on
    EXPECT_EQ(tty.read(kTapeData), 0xAA);
    EXPECT_EQ(tty.read(kTapeData), 0xBB);
}

TEST(TTYTapeTest, ReadPastEndSetsEotAndErrorWithoutAdvancing) {
    std::ostringstream out;
    std::stringstream tape("A");
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeData), 'A');
    EXPECT_EQ(tty.read(kTapeData), 0x00);
    EXPECT_EQ(tty.read(kTapeStatus) & kEot, kEot);
    EXPECT_EQ(tty.read(kTapeStatus) & kError, kError);
}

TEST(TTYTapeTest, DataAccessWithMotorOffSetsErrorAndDoesNothing) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    tty.write(kTapeData, 0xAA); // motor never turned on
    EXPECT_EQ(tty.read(kTapeStatus) & kError, kError);
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeData), 0x00); // nothing was ever written
}

TEST(TTYTapeTest, DataAccessWithNoBackingSetsError) {
    std::ostringstream out;
    TTY tty(out); // no tape backing at all
    tty.write(kTapeControl, 0x01);
    EXPECT_EQ(tty.read(kTapeData), 0x00);
    EXPECT_EQ(tty.read(kTapeStatus) & kError, kError);
}

TEST(TTYTapeTest, WritePastCurrentEndExtendsTheBacking) {
    std::ostringstream out;
    std::stringstream tape;
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01);
    tty.write(kTapeData, 0x11);
    tty.write(kTapeData, 0x22);
    EXPECT_EQ(tape.str(), std::string("\x11\x22", 2));
}

TEST(TTYTapeTest, SuccessfulReadClearsAPreviousEot) {
    std::ostringstream out;
    std::stringstream tape("A");
    TTY tty(out, &tape);
    tty.write(kTapeControl, 0x01);
    tty.read(kTapeData);         // consume the only byte
    tty.read(kTapeData);         // EOT now set
    ASSERT_EQ(tty.read(kTapeStatus) & kEot, kEot);
    tty.write(kTapeControl, 0x02); // rewind clears EOT
    tty.write(kTapeControl, 0x01);
    tty.read(kTapeData);         // succeeds again
    EXPECT_EQ(tty.read(kTapeStatus) & kEot, 0);
}
```

Add the new file to `test/CMakeLists.txt`'s `add_executable(sys6_tests ...)` list, right after `peripherals/tty_test.cpp`:

```cmake
    peripherals/tty_test.cpp
    peripherals/tty_tape_test.cpp
```

- [ ] **Step 2: Run to verify the new tests fail (and existing TTY tests still pass their old assertions once we build)**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R TTYTapeTest`
Expected: build fails or every `TTYTapeTest.*` case fails — `TTY` doesn't have a second constructor argument or tape registers yet.

- [ ] **Step 3: Implement the tape register group**

Replace `src/peripherals/tty.h` with:

```cpp
#pragma once

#include "memory/memory_device.h"

#include <cstdint>
#include <iostream>

class TTY : public MemoryDevice {
public:
    explicit TTY(std::ostream &out, std::iostream *tapeBacking = nullptr)
        : m_out(out), m_tapeBacking(tapeBacking) {}

    size_t size() const override { return 256; }
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

    // Host-facing API (not part of MemoryDevice) -- called by System, or
    // directly by tests in place of a real terminal.
    void receive(uint8_t byte);
    bool rxReady() const { return m_rxReady; }

private:
    uint8_t readSerial(uint16_t offset) const;
    void writeSerial(uint16_t offset, uint8_t val);
    uint8_t readTape(uint16_t offset) const;
    void writeTape(uint16_t offset, uint8_t val);

    // serial (offsets 0x00/0x01)
    std::ostream &m_out;
    mutable uint8_t m_rxByte = 0;
    mutable bool m_rxReady = false;

    // tape (offsets 0x02-0x04) -- a single m_tapePosition cursor drives
    // both reads and writes, modeling a real tape's one physical head
    // rather than independent read/write pointers that could diverge.
    std::iostream *m_tapeBacking;
    mutable size_t m_tapePosition = 0;
    bool m_tapeMotorOn = false;
    mutable bool m_tapeEot = false;
    mutable bool m_tapeError = false;
};
```

Replace `src/peripherals/tty.cpp` with:

```cpp
#include "tty.h"

uint8_t TTY::read(uint16_t offset) const {
    if (offset <= 1) {
        return readSerial(offset);
    }
    if (offset >= 2 && offset <= 4) {
        return readTape(offset);
    }
    return 0x00;
}

void TTY::write(uint16_t offset, uint8_t val) {
    if (offset <= 1) {
        writeSerial(offset, val);
        return;
    }
    if (offset >= 2 && offset <= 4) {
        writeTape(offset, val);
    }
    // every other offset in the page: reserved, writes ignored
}

uint8_t TTY::readSerial(uint16_t offset) const {
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

void TTY::writeSerial(uint16_t offset, uint8_t val) {
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

uint8_t TTY::readTape(uint16_t offset) const {
    if (offset == 0x02) {
        uint8_t status = 0;
        if (m_tapeBacking != nullptr) {
            status |= 0x01; // PRESENT
        }
        if (m_tapeMotorOn) {
            status |= 0x02; // MOTOR
        }
        if (m_tapeEot) {
            status |= 0x04; // EOT
        }
        if (m_tapeError) {
            status |= 0x08; // ERROR
        }
        return status;
    }
    if (offset == 0x03) {
        return 0x00; // TAPE_CONTROL is write-only
    }
    // offset == 0x04: TAPE_DATA
    if (m_tapeBacking == nullptr || !m_tapeMotorOn) {
        m_tapeError = true;
        return 0x00;
    }
    m_tapeBacking->seekg(static_cast<std::streamoff>(m_tapePosition));
    int c = m_tapeBacking->get();
    if (c == std::char_traits<char>::eof()) {
        m_tapeEot = true;
        m_tapeError = true;
        m_tapeBacking->clear();
        return 0x00;
    }
    m_tapeEot = false;
    m_tapeError = false;
    ++m_tapePosition;
    return static_cast<uint8_t>(c);
}

void TTY::writeTape(uint16_t offset, uint8_t val) {
    if (offset == 0x02) {
        return; // TAPE_STATUS is read-only
    }
    if (offset == 0x03) {
        m_tapeMotorOn = (val & 0x01) != 0;
        if ((val & 0x02) != 0) {
            m_tapePosition = 0;
            m_tapeEot = false;
            m_tapeError = false;
        }
        return;
    }
    // offset == 0x04: TAPE_DATA
    if (m_tapeBacking == nullptr || !m_tapeMotorOn) {
        m_tapeError = true;
        return;
    }
    m_tapeBacking->seekp(static_cast<std::streamoff>(m_tapePosition));
    m_tapeBacking->put(static_cast<char>(val));
    m_tapeBacking->flush();
    ++m_tapePosition;
    m_tapeEot = false;
    m_tapeError = false;
}
```

`tty.h`'s `#include <iostream>` brings in everything `readTape()` needs (`std::char_traits<char>::eof()` included) — `tty.cpp` doesn't need any include beyond `#include "tty.h"`.

Fix the three now-mismatched `Bus::attach` call sites:

`src/system/system.cpp:13`:
```cpp
    m_bus.attach(0x8000, 0x80FF, m_tty);
```

`test/system/monitor_firmware_test.cpp:24`:
```cpp
        bus.attach(0x8000, 0x80FF, tty);
```

`test/system/routine_test_fixture.h:29`:
```cpp
        bus.attach(0x8000, 0x80FF, tty);
```

- [ ] **Step 4: Run to verify everything passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure`
Expected: PASS, including every pre-existing test (`TTYTest.*`, `MonitorRoutines.*`, `MonitorFirmwareE2E.*`, `SystemTest.*`) — this is the regression check that the page resize didn't break anything.

- [ ] **Step 5: Commit**

```bash
git add src/peripherals/tty.h src/peripherals/tty.cpp src/system/system.cpp \
        test/system/monitor_firmware_test.cpp test/system/routine_test_fixture.h \
        test/peripherals/tty_tape_test.cpp test/CMakeLists.txt
git commit -m "feat: grow TTY into a 256-byte page with tape STATUS/CONTROL/DATA registers"
```

---

### Task 2: Host wiring — `System` and `sys6-monitor` take an optional tape image

**Files:**
- Modify: `src/system/system.h`
- Modify: `src/system/system.cpp`
- Modify: `src/system/monitor_main.cpp`
- Modify: `test/system/system_test.cpp`

**Interfaces:**
- Consumes: `TTY(std::ostream &, std::iostream * = nullptr)` from Task 1.
- Produces: `System(TerminalIO &term, std::ostream &out, std::iostream *tapeBacking = nullptr)`.

- [ ] **Step 1: Write the failing test**

Add to `test/system/system_test.cpp`:

```cpp
#include <sstream>

TEST(SystemTest, TapeBackingReachesTheTtyPeripheral) {
    FakeTerminalIO term;
    std::ostringstream out;
    std::stringstream tape;
    System system(term, out, &tape);
    system.reset();
    // Peek TAPE_STATUS ($8002) -- PRESENT (bit0) should read back set,
    // proving the pointer passed to System actually reached TTY.
    for (char c : std::string("8002")) {
        term.push(static_cast<uint8_t>(c));
    }
    term.push(0x0D);
    for (int i = 0; i < 20000; ++i) {
        system.step();
    }
    EXPECT_EQ(out.str(), "sys6 monitor\r\n> 8002\r\n8002: 01\r\n> ");
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R TapeBackingReachesTheTtyPeripheral`
Expected: FAIL — `System`'s constructor doesn't take a third argument yet.

- [ ] **Step 3: Implement**

`src/system/system.h`:

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
    System(TerminalIO &term, std::ostream &out, std::iostream *tapeBacking = nullptr);

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

`src/system/system.cpp`'s constructor:

```cpp
System::System(TerminalIO &term, std::ostream &out, std::iostream *tapeBacking)
    : m_term(term), m_ram(kRamSize), m_tty(out, tapeBacking), m_rom(std::vector<uint8_t>(monitor::kRomSize)),
      m_cpu(m_bus) {
    m_bus.attach(0x0000, 0x7FFF, m_ram);
    m_bus.attach(0x8000, 0x80FF, m_tty);
    m_bus.attach(0xC000, 0xFFFF, m_rom);
    monitor::install(m_rom);
}
```

(The rest of `system.cpp` — `reset()`/`step()`/`run()` — is unchanged.)

`src/system/monitor_main.cpp`:

```cpp
#include "system/posix_terminal_io.h"
#include "system/system.h"

#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
    PosixTerminalIO term;
    std::fstream tapeFile;
    std::iostream *tapeBacking = nullptr;
    if (argc > 1) {
        tapeFile.open(argv[1], std::ios::in | std::ios::out | std::ios::binary);
        if (!tapeFile.is_open()) {
            std::ofstream(argv[1], std::ios::binary).close(); // create it, then reopen for read+write
            tapeFile.open(argv[1], std::ios::in | std::ios::out | std::ios::binary);
        }
        tapeBacking = &tapeFile;
    }
    System system(term, std::cout, tapeBacking);
    system.run();
    return 0;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure`
Expected: PASS, including the whole suite (this constructor signature change touches every `System` call site, all of which use the new default argument).

Also build the real binary to catch anything the test suite wouldn't:

Run: `cmake --build build --target sys6-monitor`
Expected: builds cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/system/system.h src/system/system.cpp src/system/monitor_main.cpp test/system/system_test.cpp
git commit -m "feat: wire an optional tape image path through System and sys6-monitor"
```

---

### Task 3: `REWIND` routine

**Files:**
- Modify: `src/system/monitor_firmware.h`
- Modify: `src/system/monitor_firmware.cpp`
- Modify: `test/system/routine_test_fixture.h`
- Modify: `test/system/monitor_routines_test.cpp`

**Interfaces:**
- Consumes: `TAPE_STATUS`/`TAPE_CONTROL` at `$8002`/`$8003` from Task 1.
- Produces: `monitor::kRewindAddr = 0xD200`, `extern const std::string monitor::kRewindHex`. Entry: address in `$F0`/`$F1` (ignored). Exit: RTS, tape motor off, position at 0 if a tape was present, or `?`+CRLF printed if not.

- [ ] **Step 1: Extend `RoutineTestFixture` to optionally take a tape backing, and write the failing tests**

`test/system/routine_test_fixture.h` — replace the struct body:

```cpp
struct RoutineTestFixture {
    RAM ram{0x8000};
    std::ostringstream output;
    TTY tty;
    ROM rom{std::vector<uint8_t>(monitor::kRomSize)};
    Bus bus;
    CPU6502 cpu{bus};

    explicit RoutineTestFixture(std::iostream *tapeBacking = nullptr) : tty(output, tapeBacking) {
        bus.attach(0x0000, 0x7FFF, ram);
        bus.attach(0x8000, 0x80FF, tty);
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

(`#include <sstream>` is already present via the existing includes; `RoutineTestFixture fx;` at every existing call site keeps compiling unchanged thanks to the default argument.)

Add to `test/system/monitor_routines_test.cpp`:

```cpp
TEST(MonitorRoutines, RewindResetsPositionAndClearsEot) {
    std::stringstream tape("A");
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kRewindAddr, monitor::kRewindHex);
    fx.loadDriver("A9 01"    // LDA #$01
                  "8D 03 80" // STA $8003          (motor on)
                  "AD 04 80" // LDA $8004          (consume the only byte)
                  "AD 04 80" // LDA $8004          (hits EOF -> EOT/ERROR set)
                  "20 00 D2" // JSR $D200          (REWIND)
                  "AD 02 80" // LDA $8002          (STATUS after rewind)
                  "85 50"    // STA $50
                  "A9 01"    // LDA #$01
                  "8D 03 80" // STA $8003          (motor on again)
                  "AD 04 80" // LDA $8004          (re-read -- should be 'A' again)
                  "85 51"    // STA $51
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.ram.read(0x50), 0x01); // PRESENT only -- MOTOR/EOT/ERROR all clear
    EXPECT_EQ(fx.ram.read(0x51), 'A');  // position truly reset to 0
}

TEST(MonitorRoutines, RewindPrintsQuestionMarkWithNoTapePresent) {
    RoutineTestFixture fx; // no tape backing
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kRewindAddr, monitor::kRewindHex);
    fx.loadDriver("20 00 D2" // JSR $D200 (REWIND)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}
```

- [ ] **Step 2: Run to verify the new tests fail**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines.Rewind`
Expected: FAIL (or build failure) — `kRewindAddr`/`kRewindHex` don't exist yet.

- [ ] **Step 3: Implement `REWIND`**

Add to `src/system/monitor_firmware.h`, alongside the other `constexpr uint16_t` ROM addresses:

```cpp
constexpr uint16_t kRewindAddr = 0xD200;
```

and alongside the other `extern const std::string` declarations:

```cpp
extern const std::string kRewindHex;
```

Add to `src/system/monitor_firmware.cpp`, near the other routine definitions:

```cpp
// REWIND ($D200): if a tape is present, seeks it to position 0 and clears
// EOT/ERROR by writing TAPE_CONTROL's rewind-strobe bit (also turning the
// motor off, since a plain register write sets the whole byte). Otherwise
// prints '?'. The caller's address in $F0/$F1 is required by DISPATCH's
// grammar but unused here.
const std::string kRewindHex =
    "AD 02 80" // REWIND: LDA $8002    (TAPE_STATUS)
    "29 01"    //   AND #$01           (PRESENT bit)
    "D0 03"    //   BNE REWIND_OK
    "4C 10 D2" //   JMP REWIND_ERROR
    "A9 02"    // REWIND_OK: LDA #$02  (REWIND bit, motor bit clear)
    "8D 03 80" //   STA $8003          (TAPE_CONTROL)
    "60"       //   RTS
    "A9 3F"    // REWIND_ERROR: LDA #$3F  ('?')
    "20 00 C1" //   JSR PUTCHAR
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      //   RTS
```

Add to `install()`:

```cpp
    loadRoutine(rom, kRewindAddr, kRewindHex);
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines`
Expected: PASS, all `MonitorRoutines.*` including the two new ones and every pre-existing one.

- [ ] **Step 5: Commit**

```bash
git add src/system/monitor_firmware.h src/system/monitor_firmware.cpp \
        test/system/routine_test_fixture.h test/system/monitor_routines_test.cpp
git commit -m "feat: add REWIND monitor routine"
```

---

### Task 4: `SAVE` routine

**Files:**
- Modify: `src/system/monitor_firmware.h`
- Modify: `src/system/monitor_firmware.cpp`
- Modify: `test/system/monitor_routines_test.cpp`

**Interfaces:**
- Consumes: `TAPE_STATUS`/`TAPE_CONTROL`/`TAPE_DATA` at `$8002`–`$8004` from Task 1; `$42`/`$43`/`$44` (`TAPE_LEN`/`TAPE_CHECKSUM`) as new private zero-page scratch.
- Produces: `monitor::kSaveAddr = 0xD000`, `extern const std::string monitor::kSaveHex`. Entry: `$F0`/`$F1` = inclusive range start, `$F2`/`$F3` = inclusive range end (same convention `DISPATCH`'s existing `.`-handling already sets up before jumping to `LIST`). Exit: RTS, tape motor off. Writes `?`+CRLF instead of touching the tape if no tape is present or if the end address is before the start address.

- [ ] **Step 1: Write the failing tests**

Add to `test/system/monitor_routines_test.cpp`:

```cpp
TEST(MonitorRoutines, SaveWritesLengthDataAndChecksumToTape) {
    std::stringstream tape;
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    fx.ram.write(0x0052, 0xCC);
    fx.loadDriver("A9 50"    // LDA #$50
                  "85 F0"    // STA $F0   (START lo = $50)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1   (START hi)
                  "A9 52"    // LDA #$52
                  "85 F2"    // STA $F2   (END lo = $52)
                  "A9 00"    // LDA #$00
                  "85 F3"    // STA $F3   (END hi)
                  "20 00 D0" // JSR $D000 (SAVE)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(tape.str(), std::string("\x03\x00\xAA\xBB\xCC\xDD", 6));
    EXPECT_EQ(fx.tty.read(0x02) & 0x02, 0); // motor off after SAVE completes
}

TEST(MonitorRoutines, SavePrintsQuestionMarkWithNoTapePresent) {
    RoutineTestFixture fx; // no tape backing
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    fx.loadDriver("A9 00"    // LDA #$00
                  "85 F0"    // STA $F0
                  "85 F1"    // STA $F1
                  "85 F2"    // STA $F2
                  "85 F3"    // STA $F3
                  "20 00 D0" // JSR $D000 (SAVE)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}

TEST(MonitorRoutines, SavePrintsQuestionMarkWhenEndIsBeforeStart) {
    std::stringstream tape;
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    fx.loadDriver("A9 52"    // LDA #$52
                  "85 F0"    // STA $F0   (START = $0052)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "A9 50"    // LDA #$50
                  "85 F2"    // STA $F2   (END = $0050, before START)
                  "A9 00"    // LDA #$00
                  "85 F3"    // STA $F3
                  "20 00 D0" // JSR $D000 (SAVE)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
    EXPECT_EQ(tape.str(), "");
}
```

- [ ] **Step 2: Run to verify the new tests fail**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines.Save`
Expected: FAIL (or build failure) — `kSaveAddr`/`kSaveHex` don't exist yet.

- [ ] **Step 3: Implement `SAVE`**

Add to `src/system/monitor_firmware.h`:

```cpp
constexpr uint16_t kSaveAddr = 0xD000;
constexpr uint16_t kTapeLenLoAddr = 0x0042;
constexpr uint16_t kTapeLenHiAddr = 0x0043;
constexpr uint16_t kTapeChecksumAddr = 0x0044;
```

and:

```cpp
extern const std::string kSaveHex;
```

Add to `src/system/monitor_firmware.cpp`:

```cpp
// SAVE ($D000): entered with $F0/$F1 = inclusive range START, $F2/$F3 =
// inclusive range END (the same setup DISPATCH's '.' handling already
// builds before jumping to LIST -- SAVE reuses it identically). Prints
// '?' without touching the tape if no tape is present, or if END is
// before START. Otherwise: turns the tape motor on, computes LEN =
// END-START+1 into $42/$43, writes LEN_LO/LEN_HI, walks START..END
// (mirrors LIST's own loop-and-compare structure) writing each byte and
// XORing it into a running checksum in $44, writes the checksum, turns
// the motor back off.
const std::string kSaveHex =
    "AD 02 80" // SAVE: LDA $8002      (TAPE_STATUS)
    "29 01"    //   AND #$01           (PRESENT bit)
    "D0 03"    //   BNE PRESENT_OK
    "4C 70 D0" //   JMP SAVE_ERROR
    "A5 F3"    // PRESENT_OK: LDA $F3  (END hi)
    "C5 F1"    //   CMP $F1            (START hi)
    "90 08"    //   BCC R1             (END hi < START hi -> bad range)
    "D0 09"    //   BNE RANGE_OK       (END hi > START hi -> ok)
    "A5 F2"    //   LDA $F2            (END lo)
    "C5 F0"    //   CMP $F0            (START lo)
    "B0 03"    //   BCS RANGE_OK       (END lo >= START lo -> ok)
    "4C 70 D0" // R1: JMP SAVE_ERROR
    "A9 01"    // RANGE_OK: LDA #$01
    "8D 03 80" //   STA $8003          (motor on)
    "38"       //   SEC
    "A5 F2"    //   LDA $F2            (END lo)
    "E5 F0"    //   SBC $F0            (- START lo)
    "85 42"    //   STA $42            (LEN lo, pre-increment)
    "A5 F3"    //   LDA $F3            (END hi)
    "E5 F1"    //   SBC $F1            (- START hi)
    "85 43"    //   STA $43            (LEN hi)
    "E6 42"    //   INC $42            (LEN += 1, 16-bit)
    "D0 02"    //   BNE LENOK
    "E6 43"    //   INC $43
    "A9 00"    // LENOK: LDA #$00
    "85 44"    //   STA $44            (checksum = 0)
    "A5 42"    //   LDA $42
    "8D 04 80" //   STA $8004          (write LEN lo)
    "A5 43"    //   LDA $43
    "8D 04 80" //   STA $8004          (write LEN hi)
    "A5 F1"    // SAVE_LOOP: LDA $F1   (cursor hi)
    "C5 F3"    //   CMP $F3            (END hi)
    "90 0A"    //   BCC SAVE_CONT
    "D0 1C"    //   BNE SAVE_DONE
    "A5 F0"    //   LDA $F0            (cursor lo)
    "C5 F2"    //   CMP $F2            (END lo)
    "F0 02"    //   BEQ SAVE_CONT
    "B0 14"    //   BCS SAVE_DONE
    "A0 00"    // SAVE_CONT: LDY #$00
    "B1 F0"    //   LDA ($F0),Y        (byte at cursor)
    "8D 04 80" //   STA $8004          (write it to tape)
    "45 44"    //   EOR $44            (fold into checksum)
    "85 44"    //   STA $44
    "E6 F0"    //   INC $F0
    "D0 02"    //   BNE SAVE_NOCARRY
    "E6 F1"    //   INC $F1
    "4C 41 D0" // SAVE_NOCARRY: JMP SAVE_LOOP
    "A5 44"    // SAVE_DONE: LDA $44
    "8D 04 80" //   STA $8004          (write checksum)
    "A9 00"    //   LDA #$00
    "8D 03 80" //   STA $8003          (motor off)
    "60"       //   RTS
    "A9 3F"    // SAVE_ERROR: LDA #$3F  ('?')
    "20 00 C1" //   JSR PUTCHAR
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      //   RTS
```

Add to `install()`:

```cpp
    loadRoutine(rom, kSaveAddr, kSaveHex);
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines`
Expected: PASS, all `MonitorRoutines.*`.

If `SaveWritesLengthDataAndChecksumToTape` fails on the exact byte content, recompute the checksum by hand (XOR of `0xAA`, `0xBB`, `0xCC` is `0xDD`) and re-check the branch math against the offsets in the comment above each instruction — every branch operand is `target_offset - (this_instruction's_offset + this_instruction's_length)`, computed from a linear byte count starting at `SAVE`'s own `$D000`.

- [ ] **Step 5: Commit**

```bash
git add src/system/monitor_firmware.h src/system/monitor_firmware.cpp test/system/monitor_routines_test.cpp
git commit -m "feat: add SAVE monitor routine"
```

---

### Task 5: `LOAD` routine

**Files:**
- Modify: `src/system/monitor_firmware.h`
- Modify: `src/system/monitor_firmware.cpp`
- Modify: `test/system/monitor_routines_test.cpp`

**Interfaces:**
- Consumes: `TAPE_STATUS`/`TAPE_CONTROL`/`TAPE_DATA` at `$8002`–`$8004`, `$42`/`$43`/`$44` from Task 4.
- Produces: `monitor::kLoadAddr = 0xD100`, `extern const std::string monitor::kLoadHex`. Entry: `$F0`/`$F1` = target RAM address. Exit: RTS, tape motor off. Prints `?` (no rollback of bytes already written) if no tape is present, the tape runs out mid-block, or the trailing checksum doesn't match.

- [ ] **Step 1: Write the failing tests**

Add to `test/system/monitor_routines_test.cpp`:

```cpp
TEST(MonitorRoutines, LoadReadsLengthPrefixedBlockIntoRam) {
    std::stringstream tape(std::string("\x03\x00\xAA\xBB\xCC\xDD", 6));
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"    // LDA #$70
                  "85 F0"    // STA $F0   (target lo = $70)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1   (target hi)
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
    EXPECT_EQ(fx.ram.read(0x0072), 0xCC);
    EXPECT_EQ(fx.tty.read(0x02) & 0x02, 0); // motor off after LOAD completes
}

TEST(MonitorRoutines, LoadPrintsQuestionMarkAndKeepsPartialDataOnChecksumMismatch) {
    std::stringstream tape(std::string("\x03\x00\xAA\xBB\xCC\x00", 6)); // wrong checksum
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"
                  "85 F0"
                  "A9 00"
                  "85 F1"
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA); // already-read bytes are NOT rolled back
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
    EXPECT_EQ(fx.ram.read(0x0072), 0xCC);
}

TEST(MonitorRoutines, LoadPrintsQuestionMarkWhenTapeRunsOutMidBlock) {
    std::stringstream tape(std::string("\x05\x00\xAA", 3)); // claims 5 bytes, gives 1
    RoutineTestFixture fx(&tape);
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"
                  "85 F0"
                  "A9 00"
                  "85 F1"
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA); // the one byte that was available landed
}

TEST(MonitorRoutines, LoadPrintsQuestionMarkWithNoTapePresent) {
    RoutineTestFixture fx; // no tape backing
    monitor::loadRoutine(fx.rom, monitor::kPutCharAddr, monitor::kPutCharHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    fx.loadDriver("A9 70"
                  "85 F0"
                  "A9 00"
                  "85 F1"
                  "20 00 D1" // JSR $D100 (LOAD)
                  "00");
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}
```

- [ ] **Step 2: Run to verify the new tests fail**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines.Load`
Expected: FAIL (or build failure) — `kLoadAddr`/`kLoadHex` don't exist yet.

- [ ] **Step 3: Implement `LOAD`**

Add to `src/system/monitor_firmware.h`:

```cpp
constexpr uint16_t kLoadAddr = 0xD100;
```

and:

```cpp
extern const std::string kLoadHex;
```

Add to `src/system/monitor_firmware.cpp`:

```cpp
// LOAD ($D100): entered with $F0/$F1 = target RAM address. Prints '?'
// without writing anything if no tape is present. Otherwise turns the
// motor on, reads a 2-byte little-endian LEN into $42/$43 via the
// READ_TAPE_BYTE helper (same carry-flag success/fail convention as
// HEXVAL/PARSE_ADDR/PARSE_BYTE elsewhere in this file), then reads that
// many data bytes, writing each to RAM at the target cursor and folding
// it into a running checksum in $44 as it goes, then reads and verifies
// a trailing checksum byte. Any tape failure (end-of-tape, or a checksum
// mismatch after a full read) prints '?' -- bytes already written by
// that point are NOT rolled back. Always turns the motor back off before
// returning, on every path.
const std::string kLoadHex =
    "AD 02 80" // LOAD: LDA $8002          (TAPE_STATUS)
    "29 01"    //   AND #$01               (PRESENT bit)
    "D0 03"    //   BNE LOAD_PRESENT_OK
    "4C 5E D1" //   JMP LOAD_ERROR
    "A9 01"    // LOAD_PRESENT_OK: LDA #$01
    "8D 03 80" //   STA $8003              (motor on)
    "20 6E D1" //   JSR READ_TAPE_BYTE     (LEN lo)
    "B0 3C"    //   BCS LOAD_ERROR_MOTOROFF
    "85 42"    //   STA $42
    "20 6E D1" //   JSR READ_TAPE_BYTE     (LEN hi)
    "B0 35"    //   BCS LOAD_ERROR_MOTOROFF
    "85 43"    //   STA $43
    "A9 00"    //   LDA #$00
    "85 44"    //   STA $44                (checksum = 0)
    "A5 42"    // LOAD_LOOP: LDA $42
    "05 43"    //   ORA $43                (LEN == 0?)
    "F0 20"    //   BEQ LOAD_VERIFY
    "20 6E D1" //   JSR READ_TAPE_BYTE     (next data byte)
    "B0 24"    //   BCS LOAD_ERROR_MOTOROFF
    "48"       //   PHA                    (save byte)
    "45 44"    //   EOR $44
    "85 44"    //   STA $44                (checksum updated)
    "68"       //   PLA                    (restore original byte)
    "A0 00"    //   LDY #$00
    "91 F0"    //   STA ($F0),Y            (write to RAM at cursor)
    "E6 F0"    //   INC $F0
    "D0 02"    //   BNE LOAD_NOCARRY
    "E6 F1"    //   INC $F1
    "A5 42"    // LOAD_NOCARRY: LDA $42
    "D0 02"    //   BNE LOAD_DECLO
    "C6 43"    //   DEC $43
    "C6 42"    // LOAD_DECLO: DEC $42
    "4C 21 D1" //   JMP LOAD_LOOP
    "20 6E D1" // LOAD_VERIFY: JSR READ_TAPE_BYTE  (checksum byte)
    "B0 04"    //   BCS LOAD_ERROR_MOTOROFF
    "C5 44"    //   CMP $44
    "F0 08"    //   BEQ LOAD_OK
    "A9 00"    // LOAD_ERROR_MOTOROFF: LDA #$00
    "8D 03 80" //   STA $8003              (motor off)
    "4C 5E D1" //   JMP LOAD_ERROR
    "A9 00"    // LOAD_OK: LDA #$00
    "8D 03 80" //   STA $8003              (motor off)
    "60"       //   RTS
    "A9 3F"    // LOAD_ERROR: LDA #$3F     ('?')
    "20 00 C1" //   JSR PUTCHAR
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60"       //   RTS
    "AD 04 80" // READ_TAPE_BYTE: LDA $8004   (TAPE_DATA)
    "48"       //   PHA                       (save byte)
    "AD 02 80" //   LDA $8002                 (TAPE_STATUS)
    "29 08"    //   AND #$08                  (ERROR bit)
    "F0 03"    //   BEQ RTB_OK
    "68"       //   PLA                       (discard, balance stack)
    "38"       //   SEC                       (signal failure)
    "60"       //   RTS
    "68"       // RTB_OK: PLA                 (restore byte)
    "18"       //   CLC                       (signal success)
    "60";      //   RTS
```

Add to `install()`:

```cpp
    loadRoutine(rom, kLoadAddr, kLoadHex);
```

- [ ] **Step 4: Run to verify it passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines`
Expected: PASS, all `MonitorRoutines.*`.

If a test fails on exact bytes, the same debugging approach as Task 4 applies: recompute the failing branch's operand as `target_offset - (branch_instruction_offset + branch_instruction_length)`, counting bytes linearly from `LOAD`'s own `$D100`.

- [ ] **Step 5: Commit**

```bash
git add src/system/monitor_firmware.h src/system/monitor_firmware.cpp test/system/monitor_routines_test.cpp
git commit -m "feat: add LOAD monitor routine"
```

---

### Task 6: `DISPATCH` routes to `SAVE`/`LOAD`/`REWIND`

**Files:**
- Modify: `src/system/monitor_firmware.cpp`
- Modify: `test/system/monitor_routines_test.cpp`

**Interfaces:**
- Consumes: `kSaveAddr`/`kLoadAddr`/`kRewindAddr` from Tasks 3–5.
- Produces: `kDispatchHex` (address `$C800`, unchanged) now recognizes `.`+`" S"` (save) alongside the existing `.`-alone (list), and `" L"`/`" W"` alongside the existing `" R"` (run).

- [ ] **Step 1: Extend `loadDispatchDeps` and write the failing tests**

In `test/system/monitor_routines_test.cpp`, extend the existing `loadDispatchDeps` helper:

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
    monitor::loadRoutine(fx.rom, monitor::kSaveAddr, monitor::kSaveHex);
    monitor::loadRoutine(fx.rom, monitor::kLoadAddr, monitor::kLoadHex);
    monitor::loadRoutine(fx.rom, monitor::kRewindAddr, monitor::kRewindHex);
}
} // namespace
```

Add new tests, and keep every pre-existing `DispatchXxx` test in the file untouched (they're the regression check that rewriting `kDispatchHex` didn't change existing behavior):

```cpp
TEST(MonitorRoutines, DispatchSavesRangeAsBlock) {
    std::stringstream tape;
    RoutineTestFixture fx(&tape);
    loadDispatchDeps(fx);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    // LINEBUF = ".0051 S", LINEPOS = 0, LINELEN = 7; $F0/$F1 already
    // holds the first address ($0050), as if MAIN_LOOP's PARSE_ADDR had
    // already run.
    fx.loadDriver("A9 2E"    // LDA #$2E  ('.')
                  "85 00"    // STA $00
                  "A9 30"    // LDA #$30  ('0')
                  "85 01"    // STA $01
                  "A9 30"    // LDA #$30  ('0')
                  "85 02"    // STA $02
                  "A9 35"    // LDA #$35  ('5')
                  "85 03"    // STA $03
                  "A9 31"    // LDA #$31  ('1')
                  "85 04"    // STA $04
                  "A9 20"    // LDA #$20  (' ')
                  "85 05"    // STA $05
                  "A9 53"    // LDA #$53  ('S')
                  "85 06"    // STA $06
                  "A9 07"    // LDA #$07
                  "85 40"    // STA $40   (LINELEN = 7)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "A9 50"    // LDA #$50
                  "85 F0"    // STA $F0   (ADDR = $0050)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(tape.str(), std::string("\x02\x00\xAA\xBB\x11", 5)); // LEN=2, data, checksum=0xAA^0xBB=0x11
}

TEST(MonitorRoutines, DispatchLoadsBlockIntoRam) {
    std::stringstream tape(std::string("\x02\x00\xAA\xBB\x11", 5));
    RoutineTestFixture fx(&tape);
    loadDispatchDeps(fx);
    // LINEBUF = " L", LINEPOS = 0, LINELEN = 2; $F0/$F1 = $0070.
    fx.loadDriver("A9 20"    // LDA #$20  (' ')
                  "85 00"    // STA $00
                  "A9 4C"    // LDA #$4C  ('L')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40   (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "A9 70"    // LDA #$70
                  "85 F0"    // STA $F0   (ADDR = $0070)
                  "A9 00"    // LDA #$00
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
}

TEST(MonitorRoutines, DispatchRewindsTape) {
    std::stringstream tape("A");
    RoutineTestFixture fx(&tape);
    loadDispatchDeps(fx);
    // First, directly exhaust the tape so EOT/ERROR are set (bypassing
    // DISPATCH -- this is just test setup).
    fx.tty.write(0x03, 0x01); // motor on
    fx.tty.read(0x04);        // consume 'A'
    fx.tty.read(0x04);        // hits EOF
    fx.tty.write(0x03, 0x00); // motor off
    // LINEBUF = " W", LINEPOS = 0, LINELEN = 2; $F0/$F1 value is
    // irrelevant to REWIND but required by the grammar.
    fx.loadDriver("A9 20"    // LDA #$20  (' ')
                  "85 00"    // STA $00
                  "A9 57"    // LDA #$57  ('W')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40   (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "85 F0"    // STA $F0
                  "85 F1"    // STA $F1
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(10000));
    EXPECT_EQ(fx.tty.read(0x02), 0x01); // PRESENT only -- EOT/ERROR cleared, motor off
}

TEST(MonitorRoutines, DispatchStillPrintsQuestionMarkOnUnrecognizedTrailingLetter) {
    RoutineTestFixture fx;
    loadDispatchDeps(fx);
    // LINEBUF = " X", LINEPOS = 0, LINELEN = 2 -- 'X' is none of R/L/W.
    fx.loadDriver("A9 20"    // LDA #$20  (' ')
                  "85 00"    // STA $00
                  "A9 58"    // LDA #$58  ('X')
                  "85 01"    // STA $01
                  "A9 02"    // LDA #$02
                  "85 40"    // STA $40   (LINELEN = 2)
                  "A9 00"    // LDA #$00
                  "85 41"    // STA $41   (LINEPOS = 0)
                  "20 00 C8" // JSR DISPATCH
                  "00");     // BRK
    fx.cpu.reset();
    ASSERT_TRUE(fx.cpu.run(5000));
    EXPECT_EQ(fx.output.str(), "?\r\n");
}
```

- [ ] **Step 2: Run to verify the new tests fail, and confirm the pre-existing ones still pass unmodified**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorRoutines`
Expected: the four new `Dispatch*` tests FAIL (old `kDispatchHex` doesn't route `S`/`L`/`W` anywhere); every pre-existing `MonitorRoutines.Dispatch*` test still PASSES (nothing about them changed).

- [ ] **Step 3: Replace `kDispatchHex`**

Replace the entire `kDispatchHex` definition in `src/system/monitor_firmware.cpp` (keep the comment above it, just update its last sentence to mention the new branches):

```cpp
// DISPATCH ($C800): called with LINEPOS just past a successfully-parsed
// address in ADDR ($F0/$F1). Inspects LINEBUF[LINEPOS] and tail-jumps
// into PEEK/LIST/POKE/RUN/SAVE/LOAD/REWIND (so their own RTS returns
// straight to DISPATCH's caller), or prints '?'+CRLF and returns itself.
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
    "F0 4D"    //   BEQ DOPOKE
    "C9 20"    //   CMP #$20                (' ')
    "F0 4E"    //   BEQ MAYBERUN
    "4C 88 C8" //   JMP ERROR
    "E6 41"    // DOLIST: INC $41           (skip '.')
    "A5 F0"    //   LDA $F0
    "85 F2"    //   STA $F2                 (stash first-addr lo)
    "A5 F1"    //   LDA $F1
    "85 F3"    //   STA $F3                 (stash first-addr hi)
    "20 00 C5" //   JSR PARSE_ADDR          (second address -> $F0/$F1)
    "B0 5D"    //   BCS ERROR               (invalid second address)
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
    "A5 41"    //   LDA $41                 (LINEPOS)
    "C5 40"    //   CMP $40                 (LINELEN)
    "90 03"    //   BCC DOLIST_CHECKSAVE    (not end of line -- maybe " S")
    "4C 00 CB" //   JMP LIST                (end of line -> list, unchanged)
    "A6 41"    // DOLIST_CHECKSAVE: LDX $41
    "B5 00"    //   LDA $00,X               (char right after the range)
    "C9 20"    //   CMP #$20                (' ')
    "D0 34"    //   BNE ERROR
    "E8"       //   INX                     (skip the space)
    "E4 40"    //   CPX $40
    "B0 2F"    //   BCS ERROR               (nothing after the space)
    "B5 00"    //   LDA $00,X
    "C9 53"    //   CMP #$53                ('S')
    "D0 29"    //   BNE ERROR
    "4C 00 D0" //   JMP SAVE
    "E6 41"    // DOPOKE: INC $41           (skip ':')
    "4C 00 CC" //   JMP POKE
    "A6 41"    // MAYBERUN: LDX $41
    "E8"       //   INX                     (skip the space)
    "E4 40"    //   CPX $40
    "B0 1A"    //   BCS ERROR               (nothing after the space)
    "B5 00"    //   LDA $00,X
    "C9 52"    //   CMP #$52                ('R')
    "F0 0B"    //   BEQ DORUN
    "C9 4C"    //   CMP #$4C                ('L')
    "F0 0A"    //   BEQ DOLOAD
    "C9 57"    //   CMP #$57                ('W')
    "F0 09"    //   BEQ DOREWIND
    "4C 88 C8" //   JMP ERROR
    "4C 00 CD" // DORUN: JMP RUN
    "4C 00 D1" // DOLOAD: JMP LOAD
    "4C 00 D2" // DOREWIND: JMP REWIND
    "A9 3F"    // ERROR: LDA #$3F           ('?')
    "20 00 C1" //   JSR PUTCHAR
    "A9 0D"    //   LDA #$0D
    "20 00 C1" //   JSR PUTCHAR
    "A9 0A"    //   LDA #$0A
    "20 00 C1" //   JSR PUTCHAR
    "60";      //   RTS
```

No change to `install()` — it already has `loadRoutine(rom, kDispatchAddr, kDispatchHex)`.

- [ ] **Step 4: Run to verify everything passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure`
Expected: PASS — every `MonitorRoutines.*` test (old and new), and the entire rest of the suite (this is the point where a wrong branch offset anywhere in the rewritten `DISPATCH` would most likely surface, either as a new test failing or, more alarmingly, as one of the five pre-existing `Dispatch*` tests failing — treat that as a sign to recheck the operand math for the branch nearest the described symptom, not to touch `SAVE`/`LOAD`/`REWIND`, which Tasks 4–5 already verified standalone).

- [ ] **Step 5: Commit**

```bash
git add src/system/monitor_firmware.cpp test/system/monitor_routines_test.cpp
git commit -m "feat: route DISPATCH to SAVE/LOAD/REWIND"
```

---

### Task 7: End-to-end monitor session tests

**Files:**
- Modify: `test/system/monitor_firmware_test.cpp`

**Interfaces:**
- Consumes: everything above, exercised only through typed command lines via the existing `typeLine()`/`typeChar()` helpers (`test/system/monitor_test_helpers.h`) — no direct register pokes.

- [ ] **Step 1: Give `MonitorFixture` an optional tape backing, and write the new tests**

In `test/system/monitor_firmware_test.cpp`, change the anonymous-namespace fixture:

```cpp
namespace {
struct MonitorFixture {
    RAM ram{0x8000};
    std::ostringstream output;
    TTY tty;
    ROM rom{std::vector<uint8_t>(monitor::kRomSize)};
    Bus bus;
    CPU6502 cpu{bus};

    explicit MonitorFixture(std::iostream *tapeBacking = nullptr) : tty(output, tapeBacking) {
        bus.attach(0x0000, 0x7FFF, ram);
        bus.attach(0x8000, 0x80FF, tty);
        bus.attach(0xC000, 0xFFFF, rom);
        monitor::install(rom);
    }
};

constexpr const char *kBannerAndPrompt = "sys6 monitor\r\n> ";
} // namespace
```

(`MonitorFixture fx;` at every pre-existing call site keeps compiling unchanged thanks to the default argument.)

`#include <sstream>` is already present in this file's includes. Add the new tests:

```cpp
TEST(MonitorFirmwareE2E, SaveRewindLoadRoundTrip) {
    std::stringstream tape;
    MonitorFixture fx(&tape);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    fx.ram.write(0x0052, 0xCC);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050.0052 S");
    typeLine(fx.cpu, fx.tty, "0 W");
    typeLine(fx.cpu, fx.tty, "0070 L");
    typeLine(fx.cpu, fx.tty, "0070");
    std::string expected = kBannerAndPrompt;
    expected += "0050.0052 S\r\n> ";
    expected += "0 W\r\n> ";
    expected += "0070 L\r\n> ";
    expected += "0070\r\n0070: AA\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
    EXPECT_EQ(fx.ram.read(0x0072), 0xCC);
}

TEST(MonitorFirmwareE2E, LoadWithCorruptedChecksumStillLandsPartialDataAndReportsError) {
    std::stringstream tape;
    MonitorFixture fx(&tape);
    fx.ram.write(0x0050, 0xAA);
    fx.ram.write(0x0051, 0xBB);
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050.0051 S");
    std::string saved = tape.str();
    ASSERT_EQ(saved.size(), 4u); // LEN(2) + 2 data bytes + checksum
    saved.back() = static_cast<char>(saved.back() ^ 0xFF); // corrupt the checksum
    tape.str(saved);
    typeLine(fx.cpu, fx.tty, "0 W");
    typeLine(fx.cpu, fx.tty, "0070 L");
    std::string expected = kBannerAndPrompt;
    expected += "0050.0051 S\r\n> ";
    expected += "0 W\r\n> ";
    expected += "0070 L\r\n?\r\n> ";
    EXPECT_EQ(fx.output.str(), expected);
    EXPECT_EQ(fx.ram.read(0x0070), 0xAA);
    EXPECT_EQ(fx.ram.read(0x0071), 0xBB);
}

TEST(MonitorFirmwareE2E, TapeCommandsFailCleanlyWithNoTapeAttachedAndPromptStaysUsable) {
    MonitorFixture fx; // no tape backing
    fx.cpu.reset();
    typeLine(fx.cpu, fx.tty, "0050.0051 S");
    typeLine(fx.cpu, fx.tty, "0 W");
    typeLine(fx.cpu, fx.tty, "0070 L");
    typeLine(fx.cpu, fx.tty, "0300");
    std::string expected = kBannerAndPrompt;
    expected += "0050.0051 S\r\n?\r\n> ";
    expected += "0 W\r\n?\r\n> ";
    expected += "0070 L\r\n?\r\n> ";
    expected += "0300\r\n0300: 00\r\n> "; // prompt is still fully usable afterward
    EXPECT_EQ(fx.output.str(), expected);
}
```

- [ ] **Step 2: Run to verify the new tests fail**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure -R MonitorFirmwareE2E`
Expected: the three new tests FAIL or don't compile yet if any earlier task's byte-level detail was off; if Tasks 1–6 are all green already, these should mostly just confirm the exact echo/prompt shape — treat any mismatch here as a spec in the string literal to fix (e.g. exact spacing), not a firmware bug, since Tasks 3–6 already proved the underlying routines correct in isolation.

- [ ] **Step 3: Fix up expected strings if needed, then confirm the full suite is green**

No production code changes are expected in this task — only test code. If a string comparison fails, read the actual `fx.output.str()` from the test failure output and reconcile it against the echo/prompt model established by the existing `EchoAppearsBeforeCommandOutput` test in this same file (every typed character is echoed as it's read, `READ_LINE`'s own CRLF follows the Enter keystroke, then the command's own output if any, then the next `"> "` prompt).

- [ ] **Step 4: Run to verify everything passes**

Run: `cmake --build build --target sys6_tests && ctest --test-dir build --output-on-failure`
Expected: PASS, the entire suite.

- [ ] **Step 5: Commit**

```bash
git add test/system/monitor_firmware_test.cpp
git commit -m "test: add end-to-end save/rewind/load monitor session coverage"
```

---

### Task 8: Document the new commands and the tape CLI argument

**Files:**
- Modify: `README.md`

**Interfaces:** none — documentation only, written last so it reflects verified behavior.

- [ ] **Step 1: Update the command table and add tape usage**

In `README.md`, extend the existing command table (in the "Running the monitor" section) with three new rows, right after the `R` row:

```markdown
| `AAAA.BBBB S`     | save: writes that inclusive range to the tape as a block |
| `AAAA L`          | load: reads the next block from the tape into RAM starting at `AAAA` |
| `AAAA W`          | rewind: seeks the tape back to its start (`AAAA` is required by the syntax but ignored) |
```

Immediately after the existing table and its addressing-format paragraph, add:

```markdown
### Tape

`sys6-monitor` optionally takes a path to a tape image file:

    ./build/sys6-monitor mytape.bin

If given, `S`/`L`/`W` read and write that file (creating it if it doesn't
exist yet). If omitted, every tape command prints `?` — there's no way to
attach a tape after the process has started. A tape is a flat sequence of
`[length][data][checksum]` blocks written back to back; `W` rewinds to
the first one, and repeated `L` commands read through them in the order
they were saved.
```

- [ ] **Step 2: Manually verify against the real binary**

Run: `cmake --build build --target sys6-monitor && ./build/sys6-monitor /tmp/sys6-tape-test.bin`

At the `>` prompt, type (pressing Enter after each line):

```
0050: AA BB CC
0050.0052 S
0 W
0070 L
0070
```

Expected: the last `0070` peek prints `0070: AA`. Exit with Ctrl-C, then run `xxd /tmp/sys6-tape-test.bin` (or `hexdump -C`) and confirm it shows `03 00 aa bb cc dd`.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: document tape save/load/rewind commands and the tape image argument"
```
