# Validation — 0.2.0 C++ migration

Executed on Linux with GCC 14.2, Qt 5.15.15 and CMake 3.31.6:

- C++17 Debug build completed for the application, core library, UI library and both native test executables.
- CTest: 2/2 suites passed. The core executable reported 55 checks, including actual subprocess integration tests and a real storage diagnostic.
- Core coverage: all 25 action IDs, mutation confirmation, admin classification, missing tools, Wayland gating, case-insensitive search, merged stderr, nonzero/signal exits, large-output bounds, live output before completion, partial results on timeout, descendants holding the output pipe, closed output streams, split UTF-8, callback errors, literal arguments, invalid commands and exact fake PolicyKit argument handling.
- Offscreen Qt GUI tests passed: 25 tools, seven tabs, search filtering, default-cancel review, cancellation without execution, real read-only diagnostic completion, local JSON export and widget rendering. The rendered default window was visually inspected.
- Installation into a temporary prefix succeeded for the executable, desktop entry, license and privacy document.
- AddressSanitizer + UndefinedBehaviorSanitizer build: both CTest suites passed. Leak detection was disabled for this run; this is not a leak-free certification.
- Application dynamic dependencies resolved; no Python dependency was found. Current source and test files are C++.

Reproduce the normal checks:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

For sanitizer checks, configure a separate build directory with `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"` and `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"`, then build and run CTest with `ASAN_OPTIONS=detect_leaks=0`.

Not executed: real Linux Mint/XFCE display-server sessions, real PolicyKit prompts, privileged system mutations, hardware tuning or real Wine game lifecycle. The offscreen render does not establish real desktop or hardware compatibility. GitHub Actions is configured for GCC and Clang; local results are not a claim that remote CI has passed.

The repository remains a development alpha. Reports can contain private system data; only synthetic test fixtures belong in source control. No public release has been published.
