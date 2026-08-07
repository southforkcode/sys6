# Unit Testing + Lint/Format Infrastructure

**Date:** 2026-08-07
**Status:** Approved

## Purpose

`sys6` (a 6502 emulator) currently has no automated tests and no enforced
code style. Before more CPU behavior is implemented, we want:

1. A unit testing framework in place, with initial tests covering the
   currently-implemented, testable surface (register/flag API, `reset()`).
2. Formatting and linting available as part of the build, so style and
   common bug patterns can be checked on demand (and later wired into CI).

## Decisions

- **Test framework:** GoogleTest, pulled via CMake `FetchContent` (pinned to
  a release tag). No system package dependency, no submodule.
- **Formatter:** `clang-format`.
- **Linter:** `clang-tidy`.
- **Enforcement level:** convenience targets only. `cmake --build .` (the
  default target) is unaffected. `format` and `lint` are separate,
  opt-in CMake targets. Nothing blocks the inner-loop build today.
- **Initial test scope:** `CPU6502` register/flag API and `reset()` only.
  `executeInstruction()` is still a stub and is out of scope until it has
  real behavior to test. `Logger` is out of scope for this pass.

## Design

### 1. Test framework wiring

- Add `option(BUILD_TESTING "Build unit tests" ON)` to the top-level
  `CMakeLists.txt`.
- When `BUILD_TESTING` is on:
  - `enable_testing()`.
  - `FetchContent_Declare(googletest ...)` pinned to `v1.15.2`, then
    `FetchContent_MakeAvailable(googletest)`.
  - `add_subdirectory(test)`.
- `test/CMakeLists.txt` defines a `sys6_tests` executable that:
  - Compiles the project's own sources it needs directly (e.g.
    `src/cpu/cpu6502.cpp`, `src/utils/log.cpp`) — not `src/main.cpp`, since
    a `main()` can't coexist with GoogleTest's own `main` from
    `GTest::gtest_main`.
  - Links `GTest::gtest_main`.
  - Includes `src/` the same way the `sys6` target does.
  - Registers tests via `gtest_discover_test()` (from
    `GoogleTest.cmake`, included via `include(GoogleTest)`), so `ctest`
    lists each `TEST()` individually rather than one lump binary.

### 2. Test layout

Tests mirror `src/`'s structure:

```
test/
  cpu/
    cpu6502_test.cpp
```

Future subsystems (e.g. a bus/memory model) get their own `test/<subsystem>/`
directory following the same convention.

### 3. Initial test cases — `test/cpu/cpu6502_test.cpp`

- `reset()` post-conditions: `A()==0`, `X()==0`, `Y()==0`, `PC()==0`,
  `SP()==0xFF`, `IFlag()==true`, `DFlag()==false`, `BFlag()==true`.
- Register getter/setter round-trips for `A`, `X`, `Y`, `PC`, `SP`,
  including boundary values (`0x00`/`0xFF` for 8-bit registers,
  `0x0000`/`0xFFFF` for `PC`).
- Each flag (`C`, `Z`, `I`, `D`, `B`, `V`, `N`) round-trips independently via
  its getter/setter, and setting one flag does not disturb the others
  (regression coverage for the `std::bitset<8>` bit-packing in
  `CPU6502::m_pFlags`).
- `P()`/`P(val)`: setting the packed status byte is reflected correctly by
  the individual flag getters, and setting individual flags is reflected
  correctly in `P()`.

### 4. Lint / format

- `.clang-format` at repo root: LLVM base style, 4-space indent (matches
  existing code in `src/`).
- `.clang-tidy` at repo root enabling `bugprone-*`, `modernize-*`,
  `performance-*`, `readability-*`, with a small exclude list for
  modernize checks that don't fit the register-manipulation style used in
  `cpu6502.cpp` (e.g. checks that would push trivial getters toward
  patterns that obscure bit-level intent).
- Two custom targets added to the top-level `CMakeLists.txt`:
  - `format`: runs `clang-format -i` over all tracked `.cpp`/`.h` files
    under `src/` and `test/`.
  - `lint`: runs `clang-tidy` against the compile commands database
    (requires `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` at the top level)
    over the same file set.
- Neither target is a dependency of the default build target.

### 5. Workflow after this change

```
cmake -S . -B build
cmake --build build                       # builds sys6 and sys6_tests
ctest --test-dir build                    # runs unit tests
cmake --build build --target format       # auto-format in place
cmake --build build --target lint         # run clang-tidy, report only
```

## Out of scope

- CI wiring (running `ctest`/`lint`/`format --check` on push) — can follow
  once this lands.
- Testing `executeInstruction()` — no real behavior to test yet.
- Testing `Logger`.
- Blocking the build on lint or format violations.
