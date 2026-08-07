# Unit Testing + Lint/Format Infrastructure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add GoogleTest-based unit testing and clang-format/clang-tidy tooling to the `sys6` CMake project, with initial tests covering `CPU6502`'s register/flag API and `reset()`.

**Architecture:** GoogleTest is pulled via CMake `FetchContent` behind a `BUILD_TESTING` option; a `test/` subtree (mirroring `src/`) builds a separate `sys6_tests` binary that links the project's non-`main` sources directly. `clang-format`/`clang-tidy` are wired as opt-in custom CMake targets (`format`, `lint`) that never run as part of the default build.

**Tech Stack:** CMake 3.20+, GoogleTest v1.15.2 (via FetchContent), clang-format, clang-tidy, ctest.

## Global Constraints

- C++ standard stays at C++17 (`CMAKE_CXX_STANDARD 17`, already set) — do not change.
- GoogleTest version is pinned to `v1.15.2` via `FetchContent`. Do not use `find_package`, a system package, or a submodule.
- `format` and `lint` are separate, opt-in CMake targets. Neither may become a dependency of the default build target (`sys6`, `sys6_tests`, or `all`).
- Initial test scope is `CPU6502`'s register/flag getters-setters and `reset()` only. Do not add tests for `executeInstruction()` (still a stub) or `Logger` in this plan.
- Test files live under `test/`, mirroring the `src/` layout (e.g. `test/cpu/cpu6502_test.cpp` for `src/cpu/cpu6502.*`).

---

### Task 1: GoogleTest wiring + smoke test

**Files:**
- Modify: `CMakeLists.txt`
- Create: `test/CMakeLists.txt`
- Create: `test/cpu/cpu6502_test.cpp`

**Interfaces:**
- Produces: CMake target `sys6_tests` (executable), CMake option `BUILD_TESTING` (default `ON`). Later tasks add `TEST()` cases to `test/cpu/cpu6502_test.cpp` and rebuild the same target.

- [ ] **Step 1: Replace `CMakeLists.txt` with the version below**

```cmake
cmake_minimum_required(VERSION 3.20)
project(sys6 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(sys6
    src/main.cpp
    src/cpu/cpu6502.cpp
    src/utils/log.cpp
)

target_include_directories(sys6 PRIVATE src)

option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(test)
endif()
```

- [ ] **Step 2: Create `test/CMakeLists.txt`**

```cmake
include(GoogleTest)

add_executable(sys6_tests
    cpu/cpu6502_test.cpp
    ${CMAKE_SOURCE_DIR}/src/cpu/cpu6502.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/log.cpp
)

target_include_directories(sys6_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(sys6_tests PRIVATE GTest::gtest_main)

gtest_discover_tests(sys6_tests)
```

- [ ] **Step 3: Create `test/cpu/cpu6502_test.cpp` with a smoke test**

```cpp
#include <gtest/gtest.h>

#include "cpu/cpu6502.h"

TEST(CPU6502Smoke, GTestWiringWorks) {
    EXPECT_TRUE(true);
}
```

- [ ] **Step 4: Configure and build**

Run:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
Expected: configure step fetches GoogleTest (first run only, takes a bit longer); build produces both `sys6` and `sys6_tests` binaries with no errors.

- [ ] **Step 5: Run the test suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: `100% tests passed, 0 tests failed out of 1` — the `CPU6502Smoke.GTestWiringWorks` test.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt test/CMakeLists.txt test/cpu/cpu6502_test.cpp
git commit -m "test: wire up GoogleTest via FetchContent with a smoke test"
```

---

### Task 2: `reset()` tests

**Files:**
- Modify: `test/cpu/cpu6502_test.cpp`

**Interfaces:**
- Consumes: `CPU6502::reset()`, `A()`, `X()`, `Y()`, `PC()`, `SP()`, `IFlag()`, `DFlag()`, `BFlag()` (all pre-existing, from `src/cpu/cpu6502.h`).

- [ ] **Step 1: Append the failing/new test**

Add to `test/cpu/cpu6502_test.cpp` (after the smoke test):

```cpp
TEST(CPU6502Reset, SetsRegistersAndFlagsToPowerOnState) {
    CPU6502 cpu;
    cpu.reset();

    EXPECT_EQ(cpu.A(), 0);
    EXPECT_EQ(cpu.X(), 0);
    EXPECT_EQ(cpu.Y(), 0);
    EXPECT_EQ(cpu.PC(), 0);
    EXPECT_EQ(cpu.SP(), 0xFF);
    EXPECT_TRUE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_TRUE(cpu.BFlag());
}
```

- [ ] **Step 2: Build and run**

Run:
```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 2`.

- [ ] **Step 3: Commit**

```bash
git add test/cpu/cpu6502_test.cpp
git commit -m "test: cover CPU6502::reset() power-on state"
```

---

### Task 3: Register getter/setter round-trip tests

**Files:**
- Modify: `test/cpu/cpu6502_test.cpp`

**Interfaces:**
- Consumes: `A()`/`A(uint8_t)`, `X()`/`X(uint8_t)`, `Y()`/`Y(uint8_t)`, `PC()`/`PC(uint16_t)`, `SP()`/`SP(uint8_t)`.

- [ ] **Step 1: Append the tests**

Add to `test/cpu/cpu6502_test.cpp`:

```cpp
TEST(CPU6502Registers, ARoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.A(0x00);
    EXPECT_EQ(cpu.A(), 0x00);
    cpu.A(0xFF);
    EXPECT_EQ(cpu.A(), 0xFF);
}

TEST(CPU6502Registers, XRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.X(0x00);
    EXPECT_EQ(cpu.X(), 0x00);
    cpu.X(0xFF);
    EXPECT_EQ(cpu.X(), 0xFF);
}

TEST(CPU6502Registers, YRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.Y(0x00);
    EXPECT_EQ(cpu.Y(), 0x00);
    cpu.Y(0xFF);
    EXPECT_EQ(cpu.Y(), 0xFF);
}

TEST(CPU6502Registers, PCRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.PC(0x0000);
    EXPECT_EQ(cpu.PC(), 0x0000);
    cpu.PC(0xFFFF);
    EXPECT_EQ(cpu.PC(), 0xFFFF);
}

TEST(CPU6502Registers, SPRoundTripsBoundaryValues) {
    CPU6502 cpu;
    cpu.SP(0x00);
    EXPECT_EQ(cpu.SP(), 0x00);
    cpu.SP(0xFF);
    EXPECT_EQ(cpu.SP(), 0xFF);
}
```

- [ ] **Step 2: Build and run**

Run:
```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 6`.

- [ ] **Step 3: Commit**

```bash
git add test/cpu/cpu6502_test.cpp
git commit -m "test: cover CPU6502 register getter/setter round-trips"
```

---

### Task 4: Flag getter/setter round-trip and independence tests

**Files:**
- Modify: `test/cpu/cpu6502_test.cpp`

**Interfaces:**
- Consumes: `P(uint8_t)`, `CFlag()`/`CFlag(bool)`, `ZFlag()`/`ZFlag(bool)`, `IFlag()`/`IFlag(bool)`, `DFlag()`/`DFlag(bool)`, `BFlag()`/`BFlag(bool)`, `VFlag()`/`VFlag(bool)`, `NFlag()`/`NFlag(bool)`.

- [ ] **Step 1: Append the tests**

Add to `test/cpu/cpu6502_test.cpp`:

```cpp
TEST(CPU6502Flags, EachFlagRoundTripsIndependently) {
    CPU6502 cpu;
    cpu.P(0x00);

    cpu.CFlag(true);
    EXPECT_TRUE(cpu.CFlag());
    cpu.CFlag(false);
    EXPECT_FALSE(cpu.CFlag());

    cpu.ZFlag(true);
    EXPECT_TRUE(cpu.ZFlag());
    cpu.ZFlag(false);
    EXPECT_FALSE(cpu.ZFlag());

    cpu.IFlag(true);
    EXPECT_TRUE(cpu.IFlag());
    cpu.IFlag(false);
    EXPECT_FALSE(cpu.IFlag());

    cpu.DFlag(true);
    EXPECT_TRUE(cpu.DFlag());
    cpu.DFlag(false);
    EXPECT_FALSE(cpu.DFlag());

    cpu.BFlag(true);
    EXPECT_TRUE(cpu.BFlag());
    cpu.BFlag(false);
    EXPECT_FALSE(cpu.BFlag());

    cpu.VFlag(true);
    EXPECT_TRUE(cpu.VFlag());
    cpu.VFlag(false);
    EXPECT_FALSE(cpu.VFlag());

    cpu.NFlag(true);
    EXPECT_TRUE(cpu.NFlag());
    cpu.NFlag(false);
    EXPECT_FALSE(cpu.NFlag());
}

TEST(CPU6502Flags, SettingOneFlagDoesNotDisturbOthers) {
    CPU6502 cpu;
    cpu.P(0x00);

    cpu.CFlag(true);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_FALSE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_FALSE(cpu.BFlag());
    EXPECT_FALSE(cpu.VFlag());
    EXPECT_FALSE(cpu.NFlag());

    cpu.NFlag(true);
    EXPECT_TRUE(cpu.CFlag());
    EXPECT_TRUE(cpu.NFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_FALSE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_FALSE(cpu.BFlag());
    EXPECT_FALSE(cpu.VFlag());
}
```

- [ ] **Step 2: Build and run**

Run:
```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 8`.

- [ ] **Step 3: Commit**

```bash
git add test/cpu/cpu6502_test.cpp
git commit -m "test: cover CPU6502 flag round-trips and independence"
```

---

### Task 5: `P()`/`P(val)` packed status byte tests

**Files:**
- Modify: `test/cpu/cpu6502_test.cpp`

**Interfaces:**
- Consumes: `P()`, `P(uint8_t)`, `CFlag`, `ZFlag`, `IFlag`, `DFlag`, `BFlag`, `VFlag`, `NFlag` (getters and setters).

- [ ] **Step 1: Append the tests**

Add to `test/cpu/cpu6502_test.cpp`. Bit layout (per `src/cpu/cpu6502.cpp` offsets): bit0=C, bit1=Z, bit2=I, bit3=D, bit4=B, bit5=unused, bit6=V, bit7=N.

```cpp
TEST(CPU6502StatusByte, SettingPIsReflectedByIndividualFlags) {
    CPU6502 cpu;

    // N=1 V=1 unused=0 B=1 D=0 I=1 Z=0 C=1
    cpu.P(0b11010101);

    EXPECT_TRUE(cpu.CFlag());
    EXPECT_FALSE(cpu.ZFlag());
    EXPECT_TRUE(cpu.IFlag());
    EXPECT_FALSE(cpu.DFlag());
    EXPECT_TRUE(cpu.BFlag());
    EXPECT_TRUE(cpu.VFlag());
    EXPECT_TRUE(cpu.NFlag());
}

TEST(CPU6502StatusByte, SettingIndividualFlagsIsReflectedByP) {
    CPU6502 cpu;
    cpu.P(0x00);

    cpu.CFlag(true);
    cpu.IFlag(true);
    cpu.BFlag(true);
    cpu.VFlag(true);
    cpu.NFlag(true);

    EXPECT_EQ(cpu.P(), 0b11010101);
}
```

- [ ] **Step 2: Build and run**

Run:
```bash
cmake --build build
ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 10`.

- [ ] **Step 3: Commit**

```bash
git add test/cpu/cpu6502_test.cpp
git commit -m "test: cover CPU6502 packed status byte (P) round-trips"
```

---

### Task 6: `clang-format` config and `format` target

**Files:**
- Create: `.clang-format`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: CMake target `format` (only defined if `clang-format` is found on `PATH`).

- [ ] **Step 1: Create `.clang-format` at the repo root**

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
AccessModifierOffset: -4
```

- [ ] **Step 2: Add the `format` target to `CMakeLists.txt`**

Append to the end of `CMakeLists.txt` (after the `BUILD_TESTING` block from Task 1):

```cmake
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE FORMAT_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/test/*.cpp
        ${CMAKE_SOURCE_DIR}/test/*.h
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_SOURCE_FILES}
        COMMENT "Running clang-format on all source files"
    )
endif()
```

- [ ] **Step 3: Reconfigure, build the `sys6` target (unaffected), then run `format`**

Run:
```bash
cmake -S . -B build
cmake --build build
cmake --build build --target format
git status
```
Expected: `cmake --build build` still succeeds unchanged (format isn't a dependency of it). `cmake --build build --target format` runs clang-format over `src/` and `test/`; `git status` shows either no changes (code was already compliant) or formatting diffs — if there are diffs, review them with `git diff` to confirm they're pure style changes, nothing semantic.

- [ ] **Step 4: Commit**

```bash
git add .clang-format CMakeLists.txt
git add -u src test   # picks up any files clang-format reformatted in place
git commit -m "build: add .clang-format and a format CMake target"
```

---

### Task 7: `clang-tidy` config and `lint` target

**Files:**
- Create: `.clang-tidy`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: CMake target `lint` (only defined if `clang-tidy` is found on `PATH`).

- [ ] **Step 1: Create `.clang-tidy` at the repo root**

```yaml
Checks: >
  bugprone-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers
WarningsAsErrors: ''
HeaderFilterRegex: '^.*/src/.*'
```

- [ ] **Step 2: Enable compile command export and add the `lint` target**

In `CMakeLists.txt`, add this line near the top, right after `set(CMAKE_CXX_STANDARD_REQUIRED ON)`:

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

Then append to the end of `CMakeLists.txt` (after the `format` block from Task 6):

```cmake
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_TIDY_EXE)
    file(GLOB_RECURSE LINT_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/test/*.cpp
    )
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${LINT_SOURCE_FILES}
        COMMENT "Running clang-tidy on all source files"
    )
endif()
```

- [ ] **Step 3: Reconfigure (to generate `compile_commands.json`) and run `lint`**

Run:
```bash
cmake -S . -B build
cmake --build build
cmake --build build --target lint
```
Expected: `cmake --build build` still succeeds unchanged. `cmake --build build --target lint` runs clang-tidy against every file in `src/*.cpp` and `test/*.cpp` and prints any findings to the console — this is report-only, a non-zero clang-tidy exit or warnings do not fail the target's containing build (Since `lint` is invoked on its own, findings are just printed; treat any findings as follow-up cleanup, not a blocker for this plan).

- [ ] **Step 4: Commit**

```bash
git add .clang-tidy CMakeLists.txt
git commit -m "build: add .clang-tidy and a lint CMake target"
```
