# ◉ Tux Steward

**A calmer way to care for Linux.** Native C++17 / Qt Widgets desktop tools, initially targeting Linux Mint / XFCE. Developed by **Sudip Das**. Version **0.2.0** is a development alpha under GPL-3.0-or-later. The repository is initially private; a public release requires the owner's instruction.

## Build and run

On Linux Mint, Ubuntu or Debian:

```sh
sudo apt install build-essential cmake qtbase5-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/tux-steward
```

CMake 3.16+, a C++17 compiler, and Qt 5.15+ Core/Widgets development packages are required. There is **no Python application, build or test dependency**. `./run.sh` configures, builds, then starts the C++ application. Limit parallel build jobs on low-memory machines.

Run the GUI as your normal desktop user. Administrator tasks use PolicyKit (`pkexec`) and require a desktop authentication agent. The application rejects root startup. Individual tools need the corresponding distribution commands installed; missing dependencies are displayed. Nothing installs packages or changes settings on startup.

Optional installation:

```sh
cmake --install build --prefix "$HOME/.local"
```

Ensure the install prefix's `bin` directory is on PATH for the desktop menu launcher. Use the application binary directly if your desktop does not refresh its menu immediately.

## Tools and interface

Seven tabs retain all 25 tools from the original application: Diagnostics, Storage, Maintenance, Performance, Input, Gaming, and Schedules. Search by tool name, group or description. Each tab scrolls; a resizable output pane streams task output. File and Help menus provide JSON export and project information.

Tools include storage and mount inspection, journal diagnostics, failed services, APT removal simulation and cache cleaning, journal retention, RAM/zRAM/TLP inspection, reversible XFCE compositor controls, X11 device inspection, Wine/Lutris/Vulkan checks, timer inspection and the distribution SSD trim timer.

Every command is shown in a default-cancel review dialog. Commands execute as argument arrays using `posix_spawnp`, never through a shell. A bounded background worker captures merged stdout/stderr. Reports retain the latest 250,000 output bytes per task and 20 completed tasks. UTF-8 tails can begin with a replacement character when truncation cuts a code point. The live display may skip bursts when its buffer fills; the report retains the independently collected output tail. Export uses an atomic file replacement.

Tasks have a 120-second deadline. Timeout cleanup targets only that invocation's process group. Elevated descendants may resist termination and remain running. System changes can be partially applied; a timeout does not undo them. The GUI blocks normal closing while a task is active.

## Scope and privacy

Input toggling, game installation/removal, custom weekly maintenance plans and filesystem repair remain **planned**, as in the previous version. No broad deletion, process killing by name, kernel purge, third-party repository setup, or security-update disabling is added. Journal retention permanently removes archived logs after explicit review. Capability and permissions vary by distribution and session.

Reports contain real computer information. Export is local; there is no automatic telemetry or upload feature. Developer attribution is retained while personal machine paths, configurations and real logs stay outside source control. See [PRIVACY.md](PRIVACY.md) and [source review](docs/SOURCE_REVIEW.md).

## C++ architecture and testing

- `src/core.cpp`: action catalog, search, dependency checks, confirmation boundary and JSON schema.
- `src/runner.cpp`: bounded POSIX subprocess runner with deadlines and scoped cleanup.
- `src/window.cpp`: Qt Widgets GUI; a timer drains bounded worker output without accessing widgets from the worker.
- `tests/test_core.cpp`: real subprocess integration tests and isolated fake PolicyKit checks.
- `tests/test_ui.cpp`: offscreen GUI construction, search, review cancellation, tab and render checks.

`ctest` runs both test executables. GUI tests use Qt's offscreen platform; they do not prove real desktop PolicyKit or hardware compatibility. See [validation](docs/VALIDATION.md), [roadmap](docs/ROADMAP.md), and [contributing](CONTRIBUTING.md).
