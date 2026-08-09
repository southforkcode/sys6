# sys6

A 6502 emulator.

## Building

    cmake -S . -B build
    cmake --build build

## Testing

Unit tests are built with GoogleTest (fetched automatically via CMake's
`FetchContent`) and run through CTest:

    ctest --test-dir build --output-on-failure

Pass `-DBUILD_TESTING=OFF` to `cmake -S . -B build` to skip building tests.

## Running the monitor

`sys6-monitor` boots the emulated CPU straight into a hand-assembled 6502
monitor program (in the spirit of WozMon/the KIM-1 monitor), talking to
your real terminal through an emulated serial peripheral. Build and run
it with:

    cmake --build build --target sys6-monitor
    ./build/sys6-monitor

You'll see a `sys6 monitor` banner and a `>` prompt. Commands:

| Syntax            | Effect                                              |
|-------------------|-------------------------------------------------------|
| `AAAA`            | peek: prints the byte at address `AAAA`               |
| `AAAA.BBBB`       | list: hex-dumps that inclusive range, 16 bytes/row     |
| `AAAA: BB BB BB`  | poke: writes the given bytes starting at `AAAA`        |
| `AAAA R`          | run: jumps to `AAAA`; returns to the prompt on `RTS`   |
| `AAAA.BBBB S`     | save: writes that inclusive range to the tape as a block |
| `AAAA L`          | load: reads the next block from the tape into RAM starting at `AAAA` |
| `AAAA W`          | rewind: seeks the tape back to its start (`AAAA` is required by the syntax but ignored) |

Addresses and byte values are hex, 1–4 and 1–2 digits respectively
(shorter values are zero-extended). Backspace corrects a typo before you
press Enter. An unrecognized line prints `?` and returns to the prompt
without changing memory.

Note that the monitor's own line-input buffer lives at RAM `$0000`-`$003F`,
so peeking or poking an address in that range shows the monitor's live
working memory rather than untouched RAM.

### Tape

`sys6-monitor` optionally takes a path to a tape image file:

    ./build/sys6-monitor mytape.bin

If given, `S`/`L`/`W` read and write that file (creating it if it doesn't
exist yet). If omitted, every tape command prints `?` — there's no way to
attach a tape after the process has started. A tape is a flat sequence of
`[length][data][checksum]` blocks written back to back; `W` rewinds to
the first one, and repeated `L` commands read through them in the order
they were saved.

## Formatting and linting

    cmake --build build --target format   # auto-format via clang-format
    cmake --build build --target lint     # run clang-tidy (report-only)

Both targets require LLVM's `clang-format`/`clang-tidy` to be installed
(e.g. via Homebrew: `brew install llvm`). The `lint` target reports
findings to the console; it does not modify files or fail the build.
