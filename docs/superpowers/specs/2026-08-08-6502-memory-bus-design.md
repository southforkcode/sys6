# 6502 Memory/Bus Subsystem Design

Date: 2026-08-08

## Purpose

The CPU emulator (`CPU6502`) currently has no way to read or write memory —
`executeInstruction()` is an empty stub. This design adds a memory
subsystem, configured separately from the CPU, that the CPU reads and
writes through. It lays the groundwork for memory-mapped I/O peripherals
later (e.g. a PIA/VIA-style chip) without the CPU needing to know the
difference between RAM, ROM, or a device register — it all looks like
"read a byte from an address" / "write a byte to an address."

## Scope

In scope:
- A `Bus` that maps 16-bit addresses to memory devices over configured
  ranges.
- A `MemoryDevice` interface implemented by `RAM` and `ROM`.
- Wiring `CPU6502` to read from the bus.
- `executeInstruction()` performs a **basic fetch**: read the opcode byte
  at `PC` via the bus and increment `PC`. This proves the CPU/bus wiring
  works end-to-end.

Out of scope (explicitly deferred, not designed here):
- Opcode decoding/dispatch and instruction execution semantics.
- Memory-mapped I/O peripherals (the design accounts for them being
  addable later as another `MemoryDevice` implementation, but none is
  built now).
- Address mirroring (e.g. repeating zero-page/stack images) — add if and
  when a concrete system config needs it.
- Loading ROM images from disk — `ROM` takes its initial contents as a
  constructor argument; where those bytes come from is a separate concern.

## Architecture

```
CPU6502 --(reads/writes via)--> Bus --(routes by address range)--> MemoryDevice
                                                                     ├── RAM
                                                                     └── ROM
                                                                     (later: MMIO peripherals)
```

- **`MemoryDevice`** is a dumb interface: it knows nothing about where it's
  mapped in the address space. It exposes `size()`, `read(offset)`, and
  `write(offset, val)` in terms of device-relative offsets. This keeps
  devices reusable and independently testable.
- **`Bus`** owns an ordered list of `(start, end, MemoryDevice&)` mappings,
  set up once before emulation begins via `attach()`. On `read`/`write`,
  the bus finds the mapping containing the given 16-bit address, translates
  the address to a device-relative offset (`addr - start`), and delegates.
- **`CPU6502`** takes a `Bus &` in its constructor — a CPU cannot exist
  without a bus, mirroring real hardware. The base `CPU` interface is
  unchanged; bus access is a `CPU6502` implementation detail, not part of
  the abstract CPU contract.

## Components

### `MemoryDevice` (`src/memory/memory_device.h`)

```cpp
class MemoryDevice {
public:
    virtual ~MemoryDevice() = default;
    virtual uint16_t size() const = 0;
    virtual uint8_t read(uint16_t offset) const = 0;
    virtual void write(uint16_t offset, uint8_t val) = 0;
};
```

### `RAM` (`src/memory/ram.h`, `src/memory/ram.cpp`)

- Constructed with a size; backed by a zero-initialized `std::vector<uint8_t>`.
- `read`/`write` operate directly on the backing vector. Writes always
  succeed.

### `ROM` (`src/memory/rom.h`, `src/memory/rom.cpp`)

- Constructed from a `std::vector<uint8_t>` of initial contents — the
  vector's size defines the device's `size()`. No separate load step.
- `read` returns the stored byte.
- `write` is a no-op that logs a warning (via an optional injected
  `Logger *`, matching the existing `setLogger` pattern on `CPU`) instead
  of mutating the backing store.

### `Bus` (`src/memory/bus.h`, `src/memory/bus.cpp`)

```cpp
class Bus {
public:
    void attach(uint16_t start, uint16_t end, MemoryDevice &device);
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t val);
    void setLogger(Logger *logger);

private:
    struct Mapping { uint16_t start; uint16_t end; MemoryDevice *device; };
    std::vector<Mapping> m_mappings;
    Logger *m_logger = nullptr;
};
```

- **`attach(start, end, device)`**: setup-time configuration, called before
  emulation runs. Validates:
  - `end - start + 1 == device.size()` — the range must exactly match the
    device's declared size.
  - The new range must not overlap any already-attached mapping.

  Both are programmer/configuration errors, not emulated-runtime
  conditions, so `attach()` throws `std::invalid_argument` on either
  violation — fail fast at wiring time rather than silently misbehaving
  during emulation.

- **`read(addr) const`**: linear-scans mappings for one containing `addr`.
  On a match, translates to `addr - start` and returns
  `device->read(offset)`. On no match, logs a warning (if a logger is set)
  and returns `0xFF` (floating-bus convention). A linear scan is
  appropriate here — a real system has a handful of devices, not
  thousands.

- **`write(addr, val)`**: same lookup. On a match, delegates to
  `device->write(offset, val)` (RAM mutates; ROM logs-and-ignores per its
  own behavior). On no match, logs a warning and does nothing.

### CPU wiring (`src/cpu/cpu6502.h`, `src/cpu/cpu6502.cpp`)

- Constructor becomes `CPU6502(Bus &bus)`, storing `Bus &m_bus`.
- `executeInstruction()`:

  ```cpp
  void CPU6502::executeInstruction() {
      uint8_t opcode = m_bus.read(m_PC);
      if (m_tracing) {
          // log fetched opcode + PC via existing Logger
      }
      m_PC++;
  }
  ```

  This is intentionally minimal: it proves fetch-via-bus works but does
  not decode or execute the opcode. Full instruction dispatch is a
  separate follow-up task.

### `main.cpp`

Assembles a concrete system: a `Bus`, a `RAM` (e.g. `0x0000`–`0x7FFF`), a
`ROM` (e.g. `0x8000`–`0xFFFF`, constructed with placeholder/zeroed data —
there is no ROM-image loading yet), attaches both to the bus, then
constructs `CPU6502 cpu(bus)`.

## File / build layout

New sources under `src/memory/`:
- `memory_device.h`
- `ram.h`, `ram.cpp`
- `rom.h`, `rom.cpp`
- `bus.h`, `bus.cpp`

New tests under `test/memory/`:
- `ram_test.cpp`
- `rom_test.cpp`
- `bus_test.cpp`

Both the root `CMakeLists.txt` (executable target) and `test/CMakeLists.txt`
(test target) get the new `.cpp` files added to their explicit source
lists, consistent with the existing pattern (no globbing for build
sources — globbing is only used for the `format`/`lint` targets).

## Testing strategy

- **RAM**: read/write round-trips, boundary offsets (first/last byte),
  zero-initialized on construction.
- **ROM**: reads return the constructor-supplied contents; writes are
  ignored (value unchanged after a write attempt); `size()` reflects the
  constructor data's length.
- **Bus**:
  - `attach()` accepts valid, non-overlapping ranges.
  - `attach()` throws on a size-mismatched range.
  - `attach()` throws on an overlapping range.
  - `read`/`write` route to the correct device with correctly-translated
    offsets, including with multiple devices attached.
  - Unmapped `read` returns `0xFF`.
  - Unmapped `write` is a no-op (doesn't throw, doesn't crash).
- **CPU6502**: extend `test/cpu/cpu6502_test.cpp` with a fixture wiring a
  small `Bus` + `RAM`, verifying `executeInstruction()` reads the byte at
  `PC` and increments `PC` by 1.

## Error handling summary

| Condition | Behavior |
|---|---|
| `attach()` size mismatch | throw `std::invalid_argument` |
| `attach()` overlapping range | throw `std::invalid_argument` |
| `read()` unmapped address | log warning, return `0xFF` |
| `write()` unmapped address | log warning, no-op |
| `write()` to ROM | log warning, no-op |
