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

## Formatting and linting

    cmake --build build --target format   # auto-format via clang-format
    cmake --build build --target lint     # run clang-tidy (report-only)

Both targets require LLVM's `clang-format`/`clang-tidy` to be installed
(e.g. via Homebrew: `brew install llvm`). The `lint` target reports
findings to the console; it does not modify files or fail the build.
