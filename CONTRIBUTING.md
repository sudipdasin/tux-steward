# Contributing

Use a feature branch and pull request. Describe behavior, compatibility, risk, rollback and executed tests. Use C++17, CMake and Qt Widgets. Keep the core independent of GUI widgets. Never execute shell interpolation or private reference scripts. Mutations require command review; administrator tasks use PolicyKit. Keep personal machine data out of commits while retaining developer attribution.

Build with CMake, run CTest, and verify GUI behavior before merging. Add real subprocess tests for lifecycle changes. Keep semantic versions, CHANGELOG.md and validation evidence current. Before public distribution, test supported Linux Mint/XFCE desktops and review packaging, licensing and privacy. Never report unexecuted tests as passing.


## Development workflow

1. Fork the repository and create a focused feature branch.
2. Install `build-essential cmake qtbase5-dev desktop-file-utils`; configure a Debug build, build with two jobs on low-memory computers, and run `ctest --test-dir build --output-on-failure`.
3. Keep unvalidated settings changes under Experimental. Preserve command review, safe defaults and private report handling. Tests must use isolated configuration and synthetic service executables.
4. Open a pull request describing the problem, resulting behavior and tests actually executed. Link an issue when available. Small, focused contributions are welcome.

## Code map

- `src/core.cpp`: maintenance action catalog and dependency checks.
- `src/runner.cpp`: bounded subprocess execution.
- `src/window.cpp`: maintenance GUI and command review.
- `src/process_manager.cpp`: process table/tree and controls.
- `include/tux/system_monitor_tabs.hpp`: resource pages and Experimental workflows.
- `src/autostart.cpp`: XDG discovery, user overrides and private undo.
- `tests/`: isolated core, subprocess and offscreen GUI checks.

Explicitly dispatched GitHub Actions runs all fifteen suites and package install/startup/removal checks on Ubuntu 22.04/GCC Release and Ubuntu 24.04/Clang Debug VMs. Automatic PR/main runs and normal CircleCI checks are paused under the current resource policy. Beta tags trigger the same dual-VM checks before release packaging/publication. Do not delete existing regression coverage to obtain a green build. See docs/RELEASE_POLICY.md for forward continuity and publication rules.

Use issue templates for bug reports and proposals. Reviewers should verify privacy, permissions, compatibility and failure recovery. Do not put credentials or private machine logs in issues or pull requests.

Setup code lives in `src/setup.cpp` (fixed inventory and removal safeguards), `src/setup_wizard.cpp` (review/consent/result UI) and `src/setup_main.cpp`. Tests must isolate all installation paths and never modify the developer's real user installation.

Session-history UI lives in `src/history_dialog.cpp`. Its action button requests the existing review flow; do not execute stored command text. Tests cover immutable snapshots, bounds, selection/filter behavior and the second command-review boundary.

Current CI policy: automatic PR/main runs are paused pending reusable image preparation. Preserve `.circleci/config.yml` and the manual-only test triggers. Explicit VM validation and the beta-tag package gates remain available; invoke them and record outcomes before public downloads. The new version retains all existing regression suites.

Report review lives in `src/report_dialog.cpp`. Keep the summary allowlist and fresh per-export consent. Test excluded tasks in both modes and verify saved bytes match the preview. Never add automatic diagnostic uploads.
