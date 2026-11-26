# Contributing

Use a feature branch and pull request. Describe behavior, compatibility, risk, rollback and executed tests. Use C++17, CMake and Qt Widgets. Keep the core independent of GUI widgets. Never execute shell interpolation or private reference scripts. Mutations require command review; administrator tasks use PolicyKit. Keep personal machine data out of commits while retaining developer attribution.

Build with CMake, run CTest, and verify GUI behavior before merging. Add real subprocess tests for lifecycle changes. Keep semantic versions, CHANGELOG.md and validation evidence current. Before public distribution, test supported Linux Mint/XFCE desktops and review packaging, licensing and privacy. Never report unexecuted tests as passing.


## Development workflow

1. Fork the repository and create a focused feature branch.
2. Inside an isolated real VM, install `build-essential cmake qtbase5-dev desktop-file-utils`; configure a Debug build, build with two jobs on low-memory VMs, and run `ctest --test-dir build --output-on-failure`. See tools/vm/README.md. Do not build or run app tests on the developer host.
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

GitHub Actions and CircleCI are disabled indefinitely unless the developer explicitly approves them again. Do not dispatch workflows, enable archived definitions, or use tags/release events to trigger an older branch. Run builds, all regression suites, install/layout checks, and screenshots in a real isolated VM. Record exact environment and outcomes, including failures. Preserve existing regression coverage. See docs/RELEASE_POLICY.md.

Use issue templates for bug reports and proposals. Reviewers should verify privacy, permissions, compatibility and failure recovery. Do not put credentials or private machine logs in issues or pull requests.

Setup code lives in `src/setup.cpp` (fixed inventory and removal safeguards), `src/setup_wizard.cpp` (review/consent/result UI) and `src/setup_main.cpp`. Tests must isolate all installation paths and never modify the developer's real user installation.

Session-history UI lives in `src/history_dialog.cpp`. Its action button requests the existing review flow; do not execute stored command text. Tests cover immutable snapshots, bounds, selection/filter behavior and the second command-review boundary.

Current CI policy is permanent opt-out until renewed developer approval. Workflow definitions are historical archives under ci/disabled and have non-executable filenames. Use skip-CI source commits and local VM validation. Do not restore automatic or manual hosted CI as part of routine development.

Report review lives in `src/report_dialog.cpp`. Keep the summary allowlist and fresh per-export consent. Test excluded tasks in both modes and verify saved bytes match the preview. Never add automatic diagnostic uploads.

Health Check core lives in `src/health.cpp`; the GUI is `src/health_dialog.cpp`. Keep the fixed read-only allowlist, validate every plan before running any entry, and keep worker callbacks free of UI access. Completed checks enter existing history/report review. Fixture helpers must be isolated and all new application checks run on VMs.

Health presets and incomplete selection are fixed read-only helpers in `src/health.cpp`. Match result title and exact argument arrays to the reviewed plan before preparing another run. Never execute on preset selection or incomplete-review actions. Keep cumulative history capped at 20 and separate it from the current run inspected in the table.

## SysPulse interface

The approved visual identity is Precision (white/crimson), using Observatory monitoring depth and Forge service table/inspector/activity organization. Native UI sources are `src/syspulse_*`, public headers are under `include/syspulse`, and the full screen contract is docs/SYSPULSE_UI_SPECIFICATION.md. Preserve the shared collectors, startup backend, subprocess guards, fixed command catalog, and summary-report privacy. The three legacy entry points are compatibility applications, not the new main UI.
