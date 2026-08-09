# Tape peripheral — design spec

## Context

The system monitor (`docs/superpowers/specs/2026-08-08-system-monitor-design.md`)
gave the emulated 6502 a way to peek/poke/list/run memory interactively, but
there is still no way to persist a program or data across sessions — every
`sys6-monitor` run starts from a blank `RAM`. Real machines of this era
solved that with a cassette interface: the Apple II toggled a single output
bit and sampled a single input bit, with all encoding done by cycle-counted
software delay loops; Commodore's Datasette exposed read/write/motor/sense
control lines with OS-framed, checksummed data blocks.

`TTY` (`src/peripherals/tty.h`/`tty.cpp`) already abstracts serial I/O to a
byte-level STATUS/DATA register pair rather than bit-banging a UART, so a
tape peripheral at the same fidelity — byte-level, Commodore-flavored
(block header + checksum) rather than Apple-II-flavored (analog phase
encoding) — is the consistent choice here. Rather than a second
`MemoryDevice` living in its own bus range, tape support is added directly
to `TTY`, which grows from a 2-byte device into a 256-byte one: a single
memory-mapped I/O page hosting two independent register groups. This
mirrors how real 6502-era systems worked — a 6522 VIA or 6821 PIA chip
exposed several unrelated register blocks (ports, timers, shift registers)
within one page of address space, because chip-select logic was cheap
per-page and expensive per-byte. The Apple II's `$C0xx` I/O page and the
C64's CIA chips are both this same pattern.

## Goals

- Extend `TTY` with a tape register group (STATUS/CONTROL/DATA) occupying
  offsets `0x02`–`0x04` of its now-256-byte address range, alongside the
  existing serial STATUS/DATA at `0x00`/`0x01`.
- Tape I/O is backed by a `std::iostream *` supplied at construction —
  `nullptr` models "no cassette inserted." Real persistence (a file on
  disk) is a host-level concern wired up in `monitor_main.cpp`/`System`,
  not something `TTY` itself knows about — mirrors how `TTY`'s existing
  serial half takes a plain `std::ostream &` rather than owning a file or
  a terminal.
- A simple on-tape block format: 16-bit little-endian length, that many
  data bytes, then one XOR checksum byte. No embedded start address (the
  tape doesn't dictate where data is loaded to) but length *is* embedded
  (the user doesn't have to know or restate how many bytes a saved block
  contains).
- Three new monitor commands, extending the existing grammar:
  - **`AAAA.BBBB S`** — save the inclusive range as a block
  - **`AAAA L`** — load the next block into RAM starting at `AAAA`
  - **`AAAA W`** — rewind the tape to position 0
- A `sys6-monitor` CLI argument for an optional tape image file path.
- Automated tests: tape register semantics against a `std::stringstream`
  backing (mirrors `tty_test.cpp`'s use of `std::ostringstream`), and
  end-to-end monitor firmware tests for save/rewind/load round-trips and
  failure modes.

## Non-goals

- No cycle-accurate analog encoding (zero-crossing phase timing, the
  Apple-II approach). This is explicitly the byte-level path instead.
- No tape-length limit. A real cassette is physically finite; this
  backing grows without bound as more is written to it.
- No runtime tape insert/eject. The backing is fixed for the process's
  lifetime, same as `TerminalIO`.
- No rollback on a failed `LOAD`. If a load fails partway (end-of-tape
  reached mid-block, or a checksum mismatch discovered after the fact),
  whatever bytes were already read remain in RAM. This is documented
  behavior, not a bug to fix later.
- No change to `DISPATCH`/`MAIN_LOOP`'s existing requirement that every
  command line begin with a parseable address (see the `W` grammar note
  below) — extending that shape is a separate, larger change to already-
  working firmware flow and isn't needed for this feature.
- Renaming `TTY`. It now does more than serial I/O, but the class and
  file names stay as-is per explicit direction; a rename is a cheap,
  separate follow-up if it starts to read wrong once it's real code.

## Architecture

### Memory map

| Range | Device |
|---|---|
| `0x0000`–`0x7FFF` | `RAM` (32 KiB) |
| `0x8000`–`0x80FF` | `TTY` (256-byte I/O page: serial + tape registers) |
| `0x8100`–`0xBFFF` | unmapped (reads `0xFF`, logged) |
| `0xC000`–`0xFFFF` | `ROM` (16 KiB, monitor firmware) |

### Register layout within the page

| Offset | Register | Access | Purpose |
|---|---|---|---|
| `0x00` | `SERIAL_STATUS` | RO | bit0 RXRDY, bit1 TXRDY (unchanged) |
| `0x01` | `SERIAL_DATA` | RW | unchanged |
| `0x02` | `TAPE_STATUS` | RO | bit0 PRESENT, bit1 MOTOR, bit2 EOT, bit3 ERROR |
| `0x03` | `TAPE_CONTROL` | WO | bit0 MOTOR on/off, bit1 REWIND (strobe) |
| `0x04` | `TAPE_DATA` | RW | byte transfer |
| `0x05`–`0xFF` | reserved | — | reads return `0x00`, writes are ignored |

### `TTY` (`src/peripherals/tty.h`/`tty.cpp`) — extended

```cpp
class TTY : public MemoryDevice {
public:
    explicit TTY(std::ostream &out, std::iostream *tapeBacking = nullptr);

    size_t size() const override { return 256; }
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

    // Host-facing API -- serial (unchanged)
    void receive(uint8_t byte);
    bool rxReady() const;

private:
    uint8_t readSerial(uint16_t offset) const;
    void writeSerial(uint16_t offset, uint8_t val);
    uint8_t readTape(uint16_t offset) const;
    void writeTape(uint16_t offset, uint8_t val);

    // serial
    std::ostream &m_out;
    mutable uint8_t m_rxByte = 0;
    mutable bool m_rxReady = false;

    // tape
    std::iostream *m_tapeBacking;
    mutable size_t m_tapePosition = 0;
    bool m_tapeMotorOn = false;
    mutable bool m_tapeEot = false;
    mutable bool m_tapeError = false;
};
```

`read()`/`write()` become small offset dispatchers: `0`/`1` go to the
existing serial handling (`readSerial`/`writeSerial`, behavior unchanged
from today's `TTY`), `2`–`4` go to `readTape`/`writeTape`, anything else
returns `0x00` (read) or is a no-op (write). Splitting into private
helpers keeps the two concerns readable in the source despite sharing one
class and one page — `TTY` is one `MemoryDevice`/one bus attachment, but
not one undifferentiated blob of logic.

`TAPE_STATUS`/`TAPE_CONTROL`/`TAPE_DATA` semantics:

- **`TAPE_STATUS`** (offset `0x02`, read-only): bit0 `PRESENT` is
  `m_tapeBacking != nullptr`. bit1 `MOTOR` echoes the last value written
  to `TAPE_CONTROL`'s bit0. bit2 `EOT` is set when a read has hit the end
  of the backing's current content. bit3 `ERROR` is set whenever the most
  recent tape operation failed (no backing, motor off, or EOT).
- **`TAPE_CONTROL`** (offset `0x03`, write-only): bit0 sets `m_tapeMotorOn`
  directly from the written value (not set/cleared — the whole byte is
  taken each write, same as `SERIAL_DATA` today). bit1 is a *strobe*: if
  set, `m_tapePosition` resets to 0 and `m_tapeEot`/`m_tapeError` clear,
  but the bit itself is never stored as state — a later `TAPE_STATUS` read
  never reflects "rewind was requested," only the motor bit is persistent.
  "Write-only" describes intended firmware usage, not enforcement:
  `readTape()` still handles offset `0x03` like any reserved offset and
  returns `0x00` rather than, say, echoing the last-written byte.
- **`TAPE_DATA`** (offset `0x04`): a single `m_tapePosition` cursor drives
  both reads and writes — `seekg`/`seekp` to that position before each
  operation — modeling a real tape's one physical head rather than
  independent read/write pointers that could silently diverge. Reading or
  writing while the motor is off or `m_tapeBacking` is `nullptr` sets
  `m_tapeError` and is otherwise a no-op (read returns `0x00`, write
  discards the byte, cursor doesn't move). A successful read past the end
  of the backing's content sets `m_tapeEot` and `m_tapeError`, returns
  `0x00`, and does not advance the cursor. A successful read or write
  clears `m_tapeEot`/`m_tapeError` and advances the cursor by one — this
  means a `TAPE_DATA` write past the previous end of content is how the
  backing grows (matches Non-goals: no length limit).

`TTY`'s constructor takes `std::iostream *` (nullable) rather than a
reference, unlike `m_out`'s `std::ostream &` — "no cassette inserted" is a
real, expected state to model, whereas `TTY` never operates without some
output sink.

### On-tape block format

```
[LEN_LO][LEN_HI][DATA...][CHECKSUM]
```

`LEN` is 16-bit little-endian, the count of data bytes that follow.
`CHECKSUM` is the XOR of every data byte (chosen over an additive
checksum because it's a single `EOR` per byte in 6502 — no carry to
propagate). If `LEN` is 0, `CHECKSUM` is `0x00`.

### Monitor commands

| Syntax | Effect |
|---|---|
| `AAAA.BBBB S` | **Save**: writes the inclusive range `[AAAA, BBBB]` as a block |
| `AAAA L` | **Load**: reads the next block from the tape's current position into RAM starting at `AAAA` |
| `AAAA W` | **Rewind**: seeks the tape to position 0 (`AAAA` is required by the grammar but ignored — see note below) |

These extend `DISPATCH` (`src/system/monitor_firmware.cpp`) along its
existing branches rather than adding new ones:

- The `.` branch (today: parse a second address, unconditionally jump to
  `LIST`) gains a check after the second address parses: end-of-line still
  means `LIST` (unchanged), but a trailing `" S"` means `SAVE` instead.
- The space branch (today: only recognizes a trailing `R` for `RUN`) gains
  `L` → `LOAD` and `W` → `REWIND`, checked the same way `R` already is.

`SAVE`, `LOAD`, and `REWIND` firmware turns `TAPE_CONTROL`'s motor bit on
at the start of the routine and off at the end internally — there is no
separate user-facing "motor on" command to remember.

Errors — no tape present, end-of-tape reached mid-`LOAD`, or a checksum
mismatch after a `LOAD` completes — all print `?`, matching the existing
`DISPATCH` error convention (see Non-goals for what happens to
already-read bytes when a `LOAD` fails partway).

**Grammar note on `W`:** `MAIN_LOOP` always calls `PARSE_ADDR` before
`DISPATCH` and treats a parse failure (zero hex digits) as a bad line —
so a command that logically takes no argument still needs *some*
parseable address in front of it under the current structure. `AAAA W`
accepts and discards `AAAA`; by convention this doc's examples write it
as `0 W`. Restructuring `MAIN_LOOP` to allow zero-argument commands is
explicitly out of scope (see Non-goals).

### Host wiring

`System`'s constructor (`src/system/system.h`/`system.cpp`) gains an
optional parameter:

```cpp
System(TerminalIO &term, std::ostream &out, std::iostream *tapeBacking = nullptr);
```

passed straight through to `m_tty`'s constructor. Existing two-argument
call sites (including every current test) are unaffected — `tapeBacking`
defaults to `nullptr`, i.e. no tape present. The `Bus` wiring becomes a
single attach call for the full page:

```cpp
m_bus.attach(0x8000, 0x80FF, m_tty);
```

`monitor_main.cpp` takes an optional CLI argument: a tape image file
path. If given, it's opened `std::fstream(path, ios::in | ios::out |
ios::binary)`, creating the file first if it doesn't already exist, and a
pointer to it is passed through to `System`. If omitted, `System` gets
`nullptr` and every tape command prints `?` for the life of the process.

### Build

No new build target — `TTY`'s existing `.cpp` grows in place, and
`monitor_main.cpp`'s new argument handling is additive. `CMakeLists.txt`
is unchanged.

## Testing

- `test/peripherals/tty_test.cpp`: unchanged — still exercises only the
  serial STATUS/DATA behavior, unaffected by the page growing or the new
  private helpers.
- `test/peripherals/tty_tape_test.cpp` (new): tape register semantics
  against a `TTY` constructed with a `std::stringstream` backing (mirrors
  the existing file's use of `std::ostringstream` for serial output).
  Covers: `PRESENT`/`MOTOR`/`EOT`/`ERROR` bit behavior individually and in
  combination; `TAPE_DATA` read/write round-trip; a write past the
  current end of the backing extends it; `TAPE_DATA` access with the
  motor off or `tapeBacking == nullptr` sets `ERROR` and doesn't move the
  cursor; `TAPE_CONTROL`'s rewind strobe resets position and clears
  `EOT`/`ERROR` without itself being readable back from `TAPE_STATUS`;
  reserved offsets (`0x05`–`0xFF`) read `0x00` and ignore writes.
- End-to-end monitor firmware tests (extending
  `test/system/monitor_firmware_test.cpp` or a new sibling file, same
  `MonitorFixture`-style setup but with a `std::stringstream` tape
  backing added): `SAVE` a range, `REWIND`, `LOAD` it into a different
  RAM address, and confirm the bytes match — a true round trip. A
  separate test asserts the *exact* bytes written to the backing
  stream's `.str()` for one small, known range, to pin the on-tape
  format itself (length + data + checksum) rather than only proving
  encode/decode are symmetric, which would miss a bug present in both.
  Failure-mode tests: `LOAD` against a backing truncated mid-block prints
  `?` and leaves the successfully-read prefix in RAM; `LOAD` against a
  backing with a deliberately corrupted checksum byte prints `?`; any of
  `S`/`L`/`W` against a `TTY` constructed with no tape backing
  (`nullptr`) prints `?` immediately.
