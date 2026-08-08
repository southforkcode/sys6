# System-level tty monitor — design spec

## Context

`CPU6502` (`src/cpu/cpu.h`/`cpu6502.h`) is frozen per `CLAUDE.md`: new
capability must not be added to the CPU class itself. Today the only
driver is `src/main.cpp`, which wires a `RAM`+`ROM`+`Bus` together and
executes a single instruction — there's no way for a human to interact
with a running emulated machine.

We want a way to interact with the emulated 6502 the way a real 6502
system of that era would have been interacted with: no directly attached
keyboard or screen, only a serial line to a terminal. That means the
"interact with the CPU" capability has to live outside the CPU, as:

1. an emulated serial peripheral, memory-mapped onto the `Bus` like `RAM`
   and `ROM` already are;
2. a hand-assembled 6502 monitor program (in the spirit of WozMon/the
   KIM-1 monitor) that runs *on* the emulated CPU and talks to that
   peripheral to implement peek/poke/list/run; and
3. a host-level harness that bridges the peripheral's registers to the
   real process's stdin/stdout.

## Goals

- A `TTY` peripheral: a `MemoryDevice` exposing a 2-register (STATUS/DATA)
  memory-mapped UART-like interface, host-agnostic and independently
  unit-testable.
- A `TerminalIO` abstraction plus a `PosixTerminalIO` implementation that
  puts the real stdin into raw mode and polls it non-blockingly.
- A `System` class that owns `RAM` + `TTY` + `ROM` + `Bus` + `CPU6502` +
  `TerminalIO&`, and runs an infinite loop bridging real tty bytes into
  the `TTY` peripheral while stepping the CPU.
- A hand-assembled 6502 monitor firmware (loaded into `ROM` via the
  existing `loadProgram()`) implementing:
  - **peek** — `AAAA` → prints `AAAA: BB`
  - **list** — `AAAA.BBBB` → hex-dumps that inclusive range, 16 bytes/row
  - **poke** — `AAAA: BB BB BB` → writes the given bytes starting at `AAAA`
  - **run** — `AAAA R` → `JSR AAAA`; returns to the prompt on `RTS` or `BRK`
- A new `sys6-monitor` executable target that runs the whole system
  against the real terminal.
- Automated tests for the `TTY` register semantics, the `System` loop
  (against a fake `TerminalIO`), and the monitor firmware end-to-end
  (scripted input → asserted output transcript), following the existing
  Fibonacci e2e test's pattern of hex-string programs with per-instruction
  mnemonic comments.

## Non-goals

- Modifying `CPU6502` in any way. All of this is built against its
  existing public interface (`reset()`, `executeInstruction()`, `PC()`).
- A multi-byte RX FIFO. The `TTY` has a single-byte RX holding register,
  same as a real bare UART — a second host byte arriving before the CPU
  services the first just waits in the OS's kernel input buffer.
- Interrupts/NMI-driven I/O. The monitor firmware polls STATUS; nothing
  here wires up IRQ.
- Saving/loading programs to/from disk, or any monitor command beyond
  peek/poke/list/run.
- Windows support for `TerminalIO` — `PosixTerminalIO` targets POSIX
  (`termios`), matching the project's macOS/Darwin development target.

## Architecture

### Memory map

| Range             | Device                          |
|--------------------|----------------------------------|
| `0x0000`–`0x7FFF`  | `RAM` (32 KiB)                   |
| `0x8000`–`0x8001`  | `TTY` (STATUS, DATA)             |
| `0x8002`–`0xBFFF`  | unmapped (reads `0xFF`, logged)  |
| `0xC000`–`0xFFFF`  | `ROM` (16 KiB, monitor firmware) |

Reset vector (`0xFFFC`/`0xFFFD`) points at the firmware's cold-start entry
(prints the banner, then falls into the prompt loop). BRK vector
(`0xFFFE`/`0xFFFF`) points at a warm-start entry — the same prompt loop
without the banner — so a stray `BRK` (including one falling out of a
user program launched with `R`) always lands back at a working prompt
instead of wedging the system.

### `TTY` peripheral (`src/peripherals/tty.h`/`tty.cpp`)

```cpp
class TTY : public MemoryDevice {
public:
    explicit TTY(std::ostream &out);

    size_t size() const override; // 2
    uint8_t read(uint16_t offset) const override;
    void write(uint16_t offset, uint8_t val) override;

    // Host-facing API (not part of MemoryDevice) -- called by System, or
    // directly by tests in place of a real terminal.
    void receive(uint8_t byte);
    bool rxReady() const;

private:
    std::ostream &m_out;
    mutable uint8_t m_rxByte = 0;
    mutable bool m_rxReady = false;
};
```

Register layout (offsets within the 2-byte device range, i.e. bus
addresses `0x8000`/`0x8001`):

- `STATUS` (offset 0, read-only): bit 0 = RXRDY (an unread byte is
  latched), bit 1 = TXRDY (always 1 — writes are accepted immediately,
  there's no output buffering to model).
- `DATA` (offset 1): reading returns the latched RX byte and clears
  RXRDY; writing sends a byte out — `m_out.put(val)` and flush,
  synchronously, inside `write()`.

`read()`/`write()` mutate `m_rxReady`/`m_rxByte` despite `read()` being
`const` on `MemoryDevice` (a real UART's data register has read-clears-flag
side effects) — mirrors why `mutable` exists; both fields are marked
`mutable`.

`receive(uint8_t)` is how the outside world (real `System`, or a test)
delivers a byte: it sets `m_rxByte`/`m_rxReady = true`. If a byte is
already pending and unread, a new `receive()` call is dropped (single
holding register, no FIFO) — matches the Non-goals above.

### `TerminalIO` (`src/system/terminal_io.h` + `posix_terminal_io.h`/`.cpp`)

```cpp
class TerminalIO {
public:
    virtual ~TerminalIO() = default;
    virtual std::optional<uint8_t> tryReadByte() = 0;
};
```

`TerminalIO` only covers input. Output doesn't need an OS-specific seam
the way raw-mode/non-blocking input does — `TTY` already takes a plain
`std::ostream&` (see below), and `System` just points that at
`std::cout`, the same way `Logger` already takes an `ostream&` elsewhere
in this codebase. Keeping output on `ostream&` rather than folding it
into `TerminalIO` means a test can capture `TTY` output with a plain
`std::ostringstream` independent of however input is being faked.
`PosixTerminalIO`:

- Constructor: `tcgetattr` to save current stdin settings, then
  `cfmakeraw`-style raw mode (`ICANON`/`ECHO` off, `VMIN=0`/`VTIME=0` for
  non-blocking reads) via `tcsetattr`.
- Destructor: restores the saved settings (RAII — terminal state is
  always cleaned up on the way out, including on exceptions).
- `tryReadByte()`: non-blocking `read()` of `STDIN_FILENO`; returns
  `std::nullopt` if nothing is available.

A `FakeTerminalIO` (test-only, in `test/system/`) implements the same
interface over an in-memory byte queue, so `System`'s loop can be driven
and asserted on without a real tty.

### `System` (`src/system/system.h`/`system.cpp`)

```cpp
class System {
public:
    System(TerminalIO &term, std::ostream &out);
    void run(); // loops step() forever; never returns except via process exit
    void step(); // one read/inject/execute iteration -- the seam tests drive

private:
    TerminalIO &m_term;
    RAM m_ram{0x8000};
    TTY m_tty; // constructed with `out`
    ROM m_rom;
    Bus m_bus;
    CPU6502 m_cpu{m_bus};
};
```

`System`'s constructor attaches `m_ram`/`m_tty`/`m_rom` to `m_bus` per the
memory map above, and loads the monitor firmware hex string into `m_rom`
via the existing `loadProgram()`. `monitor_main.cpp` passes `std::cout`
as `out`; a test passes an `std::ostringstream` it can inspect.

`step()`:

```cpp
if (auto b = m_term.tryReadByte()) {
    m_tty.receive(*b);
}
m_cpu.executeInstruction();
```

`run()` calls `m_cpu.reset()` once, then loops `step()` forever.

No use of `CPU6502::run()`/`halted()` — those model "execute until BRK",
which is the wrong shape for a monitor that's meant to run forever. `BRK`
is instead handled entirely by where the BRK vector points (see Memory
map above).

### Monitor firmware

Hand-assembled 6502 machine code, written the same way
`cpu6502_fibonacci_e2e_test.cpp` writes its program: a hex string built
from concatenated string literals, one per instruction, each with a `//`
comment giving the mnemonic and what it does. Loaded into `ROM` at
construction via `loadProgram(rom, 0xC000, monitorHex)`.

Behavior:

- **Cold start**: print a short banner, fall into the prompt loop.
- **Warm start** (BRK vector target): jump straight into the prompt loop.
- **Prompt loop**: print `"> "`, read a line into a fixed-size zero-page
  buffer, echoing each received character back through `TTY.DATA` as it's
  read (raw mode means the OS does no echoing), until CR (`0x0D`).
  Backspace (`0x7F` or `0x08`) deletes the last buffered character and
  erases it visually (`\b \b`). On CR, parse and dispatch the line; on
  any parse failure, print `?` and re-prompt without altering memory.
- **Parsing**: an address is 1–4 hex digits (zero-extended to 16 bits).
  After the address, the next non-hex character selects the command:
  end-of-line → peek; `.` followed by a second address → list; `:`
  followed by space-separated 1–2-digit hex byte tokens → poke; a
  trailing space + `R` → run.
- **Peek**: read the byte at the address, print `AAAA: BB\r\n`.
- **List**: read the inclusive range, print 16 bytes per row, each row
  prefixed with its own start address, space-separated hex byte pairs.
- **Poke**: write each parsed byte to consecutive addresses starting at
  the given one.
- **Run**: the firmware lives in `ROM`, and `ROM::write()` is a no-op
  (confirmed in `rom.cpp`), so it cannot self-modify a `JSR` operand
  in place. Instead it reserves a 4-byte RAM trampoline (e.g.
  `$00F8`–`$00FB`) and, on `R`, writes `20 <lo> <hi> 60` into it — `JSR
  <addr>` followed by `RTS` — then executes `JSR $00F8`. The user
  program's own `RTS` returns into the trampoline's trailing `RTS`,
  which returns to the prompt loop; a `BRK` instead reaches the prompt
  loop via the BRK-vector warm start.

### Build target

`add_executable(sys6-monitor src/system/monitor_main.cpp src/system/system.cpp src/system/posix_terminal_io.cpp src/peripherals/tty.cpp ...)`
in `CMakeLists.txt`, alongside the existing `sys6` target — `main.cpp` is
untouched. `monitor_main.cpp` constructs a `PosixTerminalIO`, then a
`System`, and calls `run()`.

## Testing

- `test/peripherals/tty_test.cpp`: STATUS/DATA register semantics —
  RXRDY set by `receive()`/cleared by reading DATA, TXRDY always 1,
  writing DATA reaches the output `ostream`, a second `receive()` before
  the first is read is dropped. Mirrors `rom_test.cpp`/`ram_test.cpp`.
- `test/system/system_test.cpp`: calls `System::step()` a bounded number
  of times against a `FakeTerminalIO` to verify the read/inject/execute
  wiring, without ever calling the infinite `run()`.
### End-to-end monitor firmware tests

`test/system/monitor_firmware_test.cpp`, same shape as
`cpu6502_fibonacci_e2e_test.cpp`: construct `RAM`+`TTY`+`ROM`+`Bus`+
`CPU6502` directly (no `System`/`TerminalIO` needed — these tests never
touch the real terminal), script a command string through repeated
`tty.receive()` calls interleaved with `cpu.executeInstruction()`, and
assert on the captured output stream. A shared test helper
(`typeLine(cpu, tty, "A000 R")`-style) that feeds one byte at a time and
pumps the CPU between each keeps the individual test bodies readable —
this is what makes these genuinely end-to-end rather than a single
scripted blob: each byte crosses the same peripheral boundary a real
keystroke would.

This is the part of the system a human actually talks to, so it gets the
most test weight. Each of these is its own test case, not a shared
mega-scenario, so a failure points at exactly what broke:

- **Cold start**: booting the firmware (via `cpu.reset()`) prints the
  banner followed by the first `"> "` prompt, with nothing typed yet.
- **Echo**: every typed character of a command line appears in the
  output before that command's own output does — raw mode means the OS
  never does this, so it's entirely on the firmware, and easy to
  silently drop.
- **Peek**: preload a known byte via `loadProgram()`, type its address,
  assert the `AAAA: BB` line.
- **Poke then peek**: type a poke command, then a peek at the same
  address in the same session, asserting the poked value round-trips —
  proves the write actually landed in `RAM` (not just consumed off the
  input) rather than trusting the poke's own (nonexistent) echo of
  success.
- **List spanning multiple rows**: a range wider than 16 bytes, so the
  row-wrapping and per-row address prefix are both exercised, not just
  the single-row case.
- **Run via RTS**: poke a tiny program (e.g. increments a known RAM
  byte, then `RTS`) via the poke command, `R` it, then peek the RAM
  byte in the same session to confirm it executed, and confirm the
  prompt is usable again afterward.
- **Run via BRK (warm-start safety net)**: poke a tiny program that
  executes `BRK` instead of `RTS`, `R` it, and assert the transcript
  shows the *warm*-start prompt (no banner) rather than the system
  wedging — this is the one test that actually exercises the BRK-vector
  design decision end-to-end, not just "BRK doesn't crash."
- **Backspace**: type a wrong hex digit, backspace it, type the correct
  one, and assert both the visual erase sequence (`\b \b`) and the
  command's final effect reflect the *corrected* input, not the typo.
- **Malformed input recovery**: a garbage line gets a `?` and a fresh
  prompt, and — critically — a valid command typed right after still
  works, proving the bad line didn't leave stale state in the firmware's
  line buffer or parser.
- **Multi-command session**: one continuous transcript chaining several
  different commands back-to-back (poke, peek, list, run, another peek)
  in a single test, closer to how a person would actually drive it —
  catches state leaking between commands that isolated single-command
  tests each starting from a fresh `cpu.reset()` would miss.
- **Address edge cases**: a short address (1–2 hex digits, zero-extended
  to 16 bits) and an address at the top of the space (`$FFFF`) both
  parse and dispatch correctly.
