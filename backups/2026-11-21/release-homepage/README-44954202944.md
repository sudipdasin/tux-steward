<!-- LATEST-RELEASE:START -->
## Latest published release: v2.3.0-beta.1
**[Release notes and downloads](https://github.com/sudipdasin/tux-steward/releases/tag/v2.3.0-beta.1)** · Prerelease

| Asset | Download |
| --- | --- |
| SHA256SUMS | [Download](https://github.com/sudipdasin/tux-steward/releases/download/v2.3.0-beta.1/SHA256SUMS) |
| tux-steward-2.3.0-beta.1-x86_64.tar.gz | [Download](https://github.com/sudipdasin/tux-steward/releases/download/v2.3.0-beta.1/tux-steward-2.3.0-beta.1-x86_64.tar.gz) |
| tux-steward-2.3.0-beta.1-x86_64.zip | [Download](https://github.com/sudipdasin/tux-steward/releases/download/v2.3.0-beta.1/tux-steward-2.3.0-beta.1-x86_64.zip) |
| tux-steward_2.3.0.beta1_amd64.deb | [Download](https://github.com/sudipdasin/tux-steward/releases/download/v2.3.0-beta.1/tux-steward_2.3.0.beta1_amd64.deb) |
<!-- LATEST-RELEASE:END -->

# ◉ Tux Steward

**A calmer way to care for Linux.** Tux Steward includes system diagnostics and maintenance tools, plus a separate lightweight **Tux Task Manager** application for process monitoring. Both are native C++17 / Qt Widgets software, initially targeting Linux Mint / XFCE. Developed by **Sudip Das**. Version **2.4.0-beta.1** is a beta development checkpoint under GPL-3.0-or-later. This public testing beta is experimental; report issues and contribute improvements through GitHub.

## Build and run

On Linux Mint, Ubuntu or Debian:

```sh
sudo apt install build-essential cmake qtbase5-dev desktop-file-utils
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/tux-steward
./build/tux-task-manager
```

CMake 3.16+, a C++17 compiler, and Qt 5.15+ Core/Widgets development packages are required. There is **no Python application, build or test dependency**. `./run.sh` configures, builds, then starts the C++ application. Limit parallel build jobs on low-memory machines.

Run the GUI as your normal desktop user. Administrator tasks use PolicyKit (`pkexec`) and require a desktop authentication agent. The application rejects root startup. Individual tools need the corresponding distribution commands installed; missing dependencies are displayed. Nothing installs packages or changes settings on startup.

Optional installation:

```sh
cmake --install build --prefix "$HOME/.local"
```

Ensure the install prefix's `bin` directory is on PATH for the desktop menu launcher. Use the application binary directly if your desktop does not refresh its menu immediately.

## Integrated Task Manager navigation

The Windows-style page sidebar opens Processes, Performance, App history, Users, Details, Startup apps and Services directly. Performance has a resource sidebar for Overview, CPU, RAM, Disks and Network. Startup apps provides search, user/system-source filtering, enable/disable at sign-in, details and undo without editing system defaults. Services provides a searchable system/user table with separate startup and runtime states, start/stop/restart/enable/disable, explicit refresh and confirmation. System mutations retain PolicyKit authorization. Bluetooth and GPU remain in Experimental. The whole application remains a public testing beta; real desktop/hardware authorization is not certified.

## Live Task Manager resource pages

Open **Task Manager · CPU, RAM, disks and network graphs** from the main window, the desktop application menu, or `tux-task-manager`. Under **Performance**, select **CPU**, **RAM**, **Disks** or **Network**. CPU shows total and per-core usage; RAM shows memory/swap; disk and network selectors graph individual devices/interfaces. Refresh every 1/2/5 seconds, pause/resume or refresh once. History is bounded to 90 samples and starts when Performance is visible. Disk active time is a kernel counter, not a latency or drive-health measurement. Virtual/stacked devices can represent overlapping I/O; select the device you intend to inspect.

## Tools and interface

Eight tabs retain all 25 tools: Diagnostics, Storage, Maintenance, Performance, Input, Gaming, Schedules and Experimental. Search by tool name, group or description. Each tab scrolls; a resizable output pane streams task output. File and Help menus provide JSON export with a review preview and project information.

The separately launched **Tux Task Manager** monitors Linux processes without needing administrator access. It shows CPU share, resident memory, PID, account name, and process state; search and sort the list; and offers 1-, 2- or 5-second refresh while visible, plus pause/resume and one-shot manual refresh. The Process tree page groups the same snapshot by parent/child identity, supports search and expand/collapse, and opens details on double-click. Missing or reused parents become roots; the tree never offers recursive process termination. Open process details with the Details button or double-click a row to inspect its parent, executable, working directory, and command line. End Task sends `SIGTERM`; Force stop sends `SIGKILL` after its own warning. Both controls are limited to the current user's processes, exclude the task manager itself, and recheck process start identity and all UID fields, then signal through a stable kernel pidfd handle. Unsupported pidfd facilities refuse termination safely. Standard local JSON reports omit usernames and command lines. Detailed exports require an explicit checkbox on each export. Process names and metrics can still reveal activity; review reports before sharing.

Tools include storage and mount inspection, journal diagnostics, failed services, APT removal simulation and cache cleaning, journal retention, RAM/zRAM/TLP inspection, reversible XFCE compositor controls, X11 device inspection, Wine/Lutris/Vulkan checks, timer inspection and the distribution SSD trim timer.

Every command is shown in a default-cancel review dialog. Commands execute as argument arrays using `posix_spawnp`, never through a shell. A bounded background worker captures merged stdout/stderr. The local session retains the latest 250,000 output bytes per task and 20 completed tasks. Detailed exports include these records; default summary exports omit command lines and output. UTF-8 tails can begin with a replacement character when truncation cuts a code point. The live display may skip bursts when its buffer fills; the report retains the independently collected output tail. Export uses an atomic file replacement.

Tasks have a 120-second deadline. Timeout cleanup targets only that invocation's process group. Elevated descendants may resist termination and remain running. System changes can be partially applied; a timeout does not undo them. The GUI blocks normal closing while a task is active.

Service lists load asynchronously with one query per requested scope. The report and selector share results; controls are disabled during loading. Queries have a three-second timeout and a 512 KiB output limit per scope. Leaving the page, hiding the window or changing scope cancels obsolete queries. Truncated or failed scope results do not populate action choices. These limits apply to read-only listing; service mutations and Bluetooth workflows have separate lifecycles.

## Startup applications

The Startup Apps tab has a searchable, sortable table, effective user/system source filters, eligibility explanations, details, enable/disable and **Undo last change**. It honors absolute `XDG_CONFIG_HOME` / `XDG_CONFIG_DIRS`, filename precedence, section-scoped `Hidden`, `OnlyShowIn`, `NotShowIn` and `TryExec`. It also identifies GNOME's disabled-autostart extension. These are eligibility checks, not proof that a program will start successfully. Enable/disable changes `Hidden`; other desktop restrictions stay in effect. Entries are never launched by this manager.

Changes affect only the user's override directory; original system defaults are not edited. Each successful change replaces that entry's previous one-step undo record. Records contain private configuration contents and are stored with owner-only access under the user's Tux Steward configuration directory. Undo restores prior contents or removes a newly created override, and refuses a target edited outside the app. Symlink write targets, malformed entries and files above 256 KiB are refused. A failed/interrupted write can require manual recovery from the retained journal; this is not a full configuration backup system.

The implementation follows the [Desktop Application Autostart Specification](https://specifications.freedesktop.org/autostart/latest/). User entries and system overrides are distinguished by source; source does not establish an application's vendor or trustworthiness.

## Scope and privacy

Input toggling, game installation/removal, custom weekly maintenance plans and filesystem repair remain **planned**, as in the previous version. No broad deletion, process killing by name, kernel purge, third-party repository setup, or security-update disabling is added. Journal retention permanently removes archived logs after explicit review. Capability and permissions vary by distribution and session.

Reports contain real computer information. Export is local; there is no automatic telemetry or upload feature. Developer attribution is retained while personal machine paths, configurations and real logs stay outside source control. See [PRIVACY.md](PRIVACY.md) and [source review](docs/SOURCE_REVIEW.md).

## C++ architecture and testing

- `src/core.cpp`: action catalog, search, dependency checks, confirmation boundary and JSON schema.
- `src/runner.cpp`: bounded POSIX subprocess runner with deadlines and scoped cleanup.
- `src/window.cpp`: Qt Widgets GUI; a timer drains bounded worker output without accessing widgets from the worker.
- `src/process_manager.cpp`: standalone process table, same-user signal controls, and local report export for Tux Task Manager.
- `src/autostart.cpp`: bounded XDG entry discovery, user overrides and conservative one-step undo.
- `tests/test_autostart.cpp`: startup parsing, override/undo edge cases and offscreen GUI tests.
- `tests/test_core.cpp`: real subprocess integration tests and isolated fake PolicyKit checks.
- `include/tux/process_tree.hpp`: iterative parent resolution with cycle, duplicate and reused-parent protection.
- `tests/test_process_explorer.cpp`: hierarchy fixtures, real snapshots, pause/speed lifecycle and both export modes.
- `tests/test_ui.cpp`: offscreen GUI construction, search, review cancellation, tab and render checks.

`ctest` runs nineteen test suites. GUI tests use Qt's offscreen platform; they do not prove real desktop PolicyKit or hardware compatibility. See [validation](docs/VALIDATION.md), [roadmap](docs/ROADMAP.md), and [contributing](CONTRIBUTING.md).


## Beta packages and readiness

See [Beta readiness](docs/BETA_READINESS.md) for the validation matrix, package instructions and remaining desktop gates. `cpack --config build/CPackConfig.cmake` creates DEB and TGZ artifacts after a Release build. These packages are distribution-specific and unsigned. All three executables support display-free `--help` and `--version`. The generated version header keeps the GUI, CLI and package version aligned. GitHub Actions automatically validates pull requests and main on two hosted VMs; CircleCI also checks main.

Task Manager blocks normal window closing while a service mutation is active. Complete or cancel pending desktop authorization, then wait for the command result. Session shutdown or forced termination can still interrupt the interface; this is not an undo guarantee.

## Download, install and give feedback

Download the amd64 `.deb` from [GitHub Releases](https://github.com/sudipdasin/tux-steward/releases). The package is built on Ubuntu 22.04 and targets compatible Ubuntu/Mint systems; compatibility with other distributions is not guaranteed.

```sh
grep 'amd64.deb$' SHA256SUMS | sha256sum -c -
sudo apt install ./tux-steward_2.4.0.beta1_amd64.deb
tux-steward
tux-task-manager
# Uninstall the applications (your private user configuration is retained):
sudo apt remove tux-steward
```

Check the package checksum from the same release. SHA256SUMS covers both binary assets; download both or check only the matching line. Packages are unsigned. The TGZ is an alternative for compatible systems with Qt 5.15 runtime libraries; extract it and run `usr/bin/tux-steward` or `usr/bin/tux-task-manager` within the extracted directory. It is not a self-contained AppImage.

Regular tabs focus on inspection and monitoring. Settings-changing maintenance tools are grouped in **Experimental**. Task Manager's **Experimental** tab contains Bluetooth, Startup apps, Services and GPU pages, with explicit incomplete-validation notices. Process End Task remains available with confirmation and identity checks.

Use **Help → Send feedback** or [open an issue](https://github.com/sudipdasin/tux-steward/issues/new/choose). Include version, distribution, desktop, reproduction steps and whether you used an Experimental page. Review and redact diagnostics before sharing. No feedback is sent automatically. See [CONTRIBUTING.md](CONTRIBUTING.md) to build and improve the project.

## Native setup and removal wizard

Version 0.9 adds **Tux Steward Setup** (`tux-setup`), also available from the maintenance application's Help menu. To install an extracted archive for your account, run `usr/bin/tux-setup` inside its extracted folder. Qt 5 runtime libraries must already be available. The wizard previews the operation and requires confirmation. It copies all three programs into a dedicated user-data directory and creates separate user menu entries. No administrator access is requested.

Run Setup again and choose **Remove my user installation** to remove those copied programs and menu entries. Configuration, startup undo records and unrelated files are retained. Modified files, changed menu entries and symlink targets are refused. Existing user installations are not overwritten: remove an unchanged installation before installing a new archive. Interrupted operations may require retrying removal; this is not a transaction across arbitrary filesystem failures.

Setup does not uninstall a system DEB. Use `sudo apt remove tux-steward` for that. System and user installations can coexist and have separate menu labels. The archive and copied installation still require compatible Qt/system libraries.

The automatic VM validation workflow builds on Ubuntu 22.04/GCC and Ubuntu 24.04/Clang hosted virtual machines, runs every CTest suite under a virtual X server, verifies packages and checks installed GUI startup/removal. This virtual display validates GUI behavior, not physical Bluetooth/GPU hardware or a real desktop authentication agent. See each release's executed results.

## 1.0 public beta and forward continuity

Tool search now shows counts on each tab, moves to a matching category and explains empty results. Ctrl+F focuses search; Esc clears it. This is a 1.0 **testing beta**, not stable certification. The hardware/authorization limits above still apply.

Every future main-branch change and pull request runs the complete regression suite on two hosted Ubuntu VMs. New versions descend from the tested public foundation and retain its source and regression tests; this is inheritance through Git history, not copying binaries between old releases. Intentional changes still require review. Tags remain immutable checkpoints in the development process. No background self-update or automatic installation happens on users' computers.

An owner-created beta tag on main triggers dual-VM validation and package checks, then publication of DEB/TGZ/checksums. Failed checks block that publication job. Released asset files are not silently overwritten. See [release policy](docs/RELEASE_POLICY.md).

## Reviewed maintenance reports

File → Export session report opens a snapshot preview. The default summary includes only known tool names, exit codes, timings and truncation flags; command arguments and diagnostic output are omitted entirely. Select **Include commands and diagnostic output** to include full recorded details for this export only. Review the exact JSON before saving. The choice resets on every export. Cancel writes no report. No report is uploaded automatically. Summary still reveals which tools ran and their outcomes; it is not a guarantee of anonymity. Existing integrations needing raw output must explicitly choose detailed mode. The report includes `schema: 1` and `mode: summary` or `detailed`.

## Session history (1.2)

Open **History** above the output pane, **View → Session history**, or Ctrl+H. Inspect the newest 20 completed tasks in reverse order, filter by tool/result and revisit their captured command/output. Double-clicking or selecting a row never runs a command. **Review and run again** closes history and opens the normal default-cancel review; dependency and authorization checks still apply. The button is unavailable while a task is active or a record does not exactly match the current command catalog.

History is a snapshot: reopen it after a task finishes to refresh. It is kept only in memory and disappears on exit. Local details can reveal private machine information. Export still goes through the separate reviewed, summary-by-default report flow. No history is uploaded or restored from disk.

### Current CI resource policy

Automatic PR/main validation was paused by newer repository CI policy while reusable build images are prepared. This release preserves that policy. Use the manual **C++ VM validation** workflow on the feature branch before merge; owner-created beta tags still invoke the full VM matrix and package gates. Earlier references to automatic PR/main runs describe the former configuration. No CircleCI/GHCR image is published or substituted by this feature change.

## Stop a diagnostic (1.3)

**Stop diagnostic** becomes available while a read-only maintenance task runs. It requests cancellation, signals that invocation's process group while its leader identity is retained and keeps the UI busy until the worker returns. Settings-changing/admin tasks cannot be cancelled here, including through the core execute API. After completion, the next task gets a fresh cancellation token.

Cancelled results use code 130 and an additive `cancelled: true` field in summary/detailed exports; timeout remains code 124 with `cancelled: false`. History labels the result Cancelled and retains the captured output tail. Cancellation can discard unread output. Detached descendants can escape the process group, so this is a cancellation request, not proof that every descendant stopped. It does not undo changes or affect Task Manager service actions. Closing remains blocked while the task is active.

### Selective feedback reports in 1.4

Export session report now lets you include only relevant completed tasks. Check tasks in the newest-first list, review the exact JSON, and save locally. Select all/none and a live count help avoid sharing unrelated results. Summary omits commands and diagnostics; detailed output needs a fresh opt-in on every export. An empty selection cannot be saved. Reports use an immutable snapshot of at most 20 tasks and keep chronological ordering.

### Reliability and local report privacy (1.4.1)

Task execution retains its child leader identity until inherited output handling and cleanup finish, then reaps it. Dedicated subprocess fixtures cover normal completion, timeout, cancellation and callback errors. Reviewed report exports are saved owner-only (0600), including replacements. Filesystem ACLs, privileged users and the privacy of the chosen destination remain outside this permission guarantee.

## Health Check workspace (2.0 beta)

Open **Health Check** to select read-only diagnostics and review every command before starting. Four checks are selected initially; kernel and system logs are optional. Checks run sequentially in the background with progress, Stop and a stop-on-failure option. Close during a run requests cancellation and waits; pending checks are skipped. Completed results enter History and the existing report review, where you choose tasks and whether to include raw output.

The fixed six-check catalog cannot run arbitrary commands or system mutations. Missing selected dependencies block GUI start; nothing installs automatically. Completion records exit status and findings, not a health score. This major workflow beta retains the 25 individual tools and all three native applications. Reports and settings remain compatible with 1.4.1.

### Executable ZIP download

The Releases page includes a ZIP with all three native executables, alongside the DEB, TGZ and SHA256SUMS. Extract the ZIP and run `./usr/bin/tux-setup` for per-user installation or `./usr/bin/tux-steward` directly. Compatible Qt 5.15 runtime libraries are required; this is not a self-contained AppImage. Both archives are extracted and checked for executable permissions, versions and private build paths on VMs; the release pipeline also launches all three GUIs from the ZIP.

## Health Check cockpit (2.1)

Choose Balanced, Storage, Logs or All to prepare a plan; review commands before running. The Status column distinguishes completed, failed, timed-out, cancelled and unexecuted checks. Select a row to inspect its latest output. **Review incomplete checks…** prepares only unfinished/unsuccessful checks; it never restarts them automatically. Click Run selected checks after reviewing. Results from earlier runs remain available in History and Review report, capped at 20 records. Completion is not a guarantee of system health.
