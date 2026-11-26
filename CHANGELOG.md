# Changelog

## 3.0.0-alpha.1 — 2026-11-26

- Introduced SysPulse, the XFCE-first successor interface with Precision identity, Observatory monitoring depth, and Forge management organization.
- Added persistent navigation, global tool search, resource overview, CPU/per-core/memory/swap/disk/network histories and the preserved process manager.
- Added unified startup/service inspectors, async queries, scoped action review, startup overrides/recovery, and activity records.
- Retained all 25 catalog tools and added ten read-only diagnostics, with local reviewed reports and settings-changing tools kept in Experimental.
- Serialized reviewed actions across workspaces and resolved privileged fixed commands from protected system locations; retained legacy history recognition.
- Replaced idle admission polling with synchronous busy-state notifications; added a native C++ measurement probe and recorded matched VM observations.
- Kept reviewed service actions busy until the observed helper exits; query cancellation cannot release a running action. Added a release-gated subprocess regression.
- Archived GitHub Actions and CircleCI definitions. All application verification uses a real isolated VM. See SYSPULSE_VALIDATION.md for executed results and remaining gates.
- Added native package support, portable-runtime packaging tooling, desktop entry/icon and developer handoff. Alpha status does not certify full XFCE, Mint, PolicyKit, hardware or upgrade compatibility.

## 2.4.0-beta.1 — 2026-11-22
- Integrated Windows-style page and resource sidebars; first-class Startup apps and Services.
- Searchable service table distinguishes startup/runtime states; bounded async queries and confirmed restart support with scope-safe selection.
- Expanded runtime/search/restart/navigation tests, startup/service screenshots and pinned upgrade through 2.3.

## 2.3.0-beta.1 — 2026-11-22
- Dedicated CPU/RAM/disk/network Performance pages, per-core graphs, per-device/interface read/write rates, memory/swap and bounded histories.
- Selectable 1/2/5-second refresh, pause/resume/manual refresh and main-app Task Manager launcher.
- Validated counter delta logic, baseline/reset handling and CPU guest-time exclusion; nineteenth VM suite plus GUI control regressions.

## 2.2.0-beta.1 — 2026-11-22
- Outcome triage combines session-history search with attention, completed, cancelled and timeout filters and a live matching count.
- Review matching report opens the existing privacy-first report dialog with only matching tasks; nothing is exported automatically.
- Prepare new plan unlocks Health Check after completion, preserving bounded history and requiring explicit command review/run.
- Expanded GUI regressions and checksum-pinned upgrade through 2.1; all application validation remains VM-only.

## 2.1.0-beta.1 — 2026-11-22

Health Check cockpit: fixed Balanced/Storage/Logs/All presets prepare exact command review without execution. Each diagnostic shows Pending/Running/Completed/Needs attention/Timed out/Cancelled/Not run status; row selection inspects that check’s latest bounded output. Review incomplete checks selects only failed, cancelled or unexecuted entries for a fresh explicit run. Prior results remain in bounded history/report snapshots. Adds strict plan/result matching and recovery regressions, bringing CTest to eighteen suites; extends pinned upgrades through 2.0. Retains binary ZIP/DEB/TGZ publication, 25 tools and manual CI policy.

## 2.0.0-beta.1 — 2026-11-21

Introduces the guided Health Check workspace: select from six read-only diagnostics, review all exact commands, run sequentially with progress, stop an active run and its remaining queue, and choose stop-on-failure or continue. Local results flow into bounded session history and existing selective privacy-reviewed reports saved owner-only. Selection runs nothing automatically; unavailable dependencies block GUI start. Core validates the entire fixed-ID plan before execution and refuses mutations/duplicates/unknown IDs. Adds core plan and GUI/integration suites (seventeen total), pinned upgrade through 1.4.1 and VM screenshots. Retains all 25 individual tools, Task Manager, setup, cancellation, Experimental grouping and manual CI policy. Major workflow milestone remains a testing beta, not hardware/desktop certification. Adds directly downloadable executable ZIP with checksums, archive executable/version/privacy verification and ZIP GUI startup checks.

## 1.4.1-beta.1 — 2026-11-19

Reliability/privacy patch: observe child exit with waitid/WNOWAIT, retaining the task leader identity until output draining and group cleanup finish. Reap exactly once on scope exit, including timeout, cancellation and output-callback exceptions. Refuse signalling if ownership is lost. Save reviewed reports with owner read/write permissions, including replacement of permissive reports. New subprocess lifecycle suite and report-permission fixtures bring CTest to fifteen suites. Retain all prior features and manual CI policy; extend pinned upgrade checks through 1.4.

## 1.4.0-beta.1 — 2026-11-19

Added task-selective report review with newest-first checkboxes, selection count, select all/none, disabled save for empty selections and an exact JSON preview. Export retains chronological ordering, summary-by-default privacy and fresh detailed-output consent. A bounded immutable snapshot excludes unselected tasks even in detailed reports; atomic save writes the captured reviewed bytes. Added dedicated GUI subset/privacy/save regressions, bringing CTest to fourteen suites. Existing tools, cancellation, history and manual CI policy remain; pinned upgrade validation extends through 1.3.

## 1.3.0-beta.1 — 2026-11-19

Added Stop diagnostic for read-only maintenance actions, cooperative cancellation token plumbing and scoped process-group cleanup. Settings-changing/admin execute paths ignore cancellation tokens. Added distinct cancelled status/metadata in UI, history and exports, token reset for subsequent tasks, real owned-child/isolated-helper tests and UI capture. CTest now has thirteen suites. Preserve manual CI policy, homepage release automation, all prior tools and setup safeguards; extend pinned upgrades through 1.2.

## 1.2.0-beta.1 — 2026-11-19

Added local session-history explorer with newest-first results, filtering, captured command/output details and a review-first rerun request. Added History count, View menu and Ctrl+H. History snapshots retain only the newest 20 completed tasks, stay in memory and never execute stored command text. Added standalone history fixtures plus normal-review cancellation integration coverage, bringing CTest to eleven suites. Preserve current manual CI and image-preparation policy; extend pinned package upgrades through 1.1.

## 1.1.0-beta.1 — 2026-11-19

Added maintenance report review with exact JSON preview, summary-by-default allowlist, explicit per-export detailed consent, cancel and atomic save. Added report privacy fixtures and UI preview/toggle/reset/save coverage. Existing detailed diagnostics remain available locally and by opt-in export. Added bounded network timeouts/retries for VM dependency installation. All prior tools and setup safeguards retained; hardware and authorization limits remain experimental.

## 1.0.0-beta.1 — 2026-11-18

Public testing milestone retaining the complete 0.9 foundation: cross-tab search counts, automatic result navigation, empty-result guidance and keyboard search. Automatic dual-VM regression validation now runs for pull requests and main. Owner-created beta tags run the same VM matrix before package/publication checks; asset uploads refuse overwriting. Added explicit forward-continuity and release policy. This remains a beta with real hardware, authorization and desktop-session validation outstanding.

## 0.9.0-beta.1 — 2026-11-18

Added native Setup and removal wizard for per-user archive installs, separate menu entries, explicit consent, hashed fixed-file records, collision/symlink/tamper refusal and retention of user configuration/unrelated files. Added a GUI integration suite that installs and launches copied native programs and tests removal safeguards. Added two hosted-VM validation targets with virtual X desktops; release packaging also validates all three installed GUIs on a virtual X desktop. Hardware and real authorization remain experimental.

## 0.8.0-beta.3 — 2026-11-18

Public testing beta preparation: grouped unvalidated Bluetooth/startup/service/GPU workflows in Task Manager Experimental tabs and settings-changing maintenance tools in Experimental. Added feedback/contribution menus, issue templates, install/uninstall guidance and contributor code map. Added manual Ubuntu release packaging with verification and asset upload to an existing prerelease. No hardware validation claims.

## 0.8.0-beta.2 — 2026-11-18

Task Manager now refuses normal window closing during an active service mutation or pending authorization. A clear notice explains completing/cancelling authorization before exit. Added regression coverage using a real top-level window and synthetic service commands, including close recovery after command startup failure. Unified prerelease counters for application and Debian package versions. The published beta.1 tag remains unchanged.

## 0.8.0-beta.1 — 2026-11-18

Private beta preparation: process termination now uses stable pidfd handles with start-time and UID verification; unavailable facilities fail closed. Service actions prevent overlap, bound output and recover from failed process startup. Added real owned-child signal tests and synthetic service mutation recovery coverage. Added headless help/version, generated version metadata, staged install checks, stripped DEB/TGZ packaging and private CI artifacts with checksums. Added an explicit desktop readiness matrix; no public release or release-candidate certification.

## 0.7.0 — 2026-11-18

Rebuilt Startup Apps as a searchable, sortable table with effective XDG directory precedence, user/system source filters, sign-in eligibility reasons and plain-text entry details. Added user-only Hidden overrides and a private one-step undo journal. Undo refuses externally changed targets; malformed/oversized entries and symbolic write targets are rejected. Added backend and GUI fixture tests without touching real sign-in configuration.

## 0.6.0 — 2026-11-17

Replaced blocking, duplicate service-list queries with bounded asynchronous queries shared by the report and action selector. Added three-second query deadlines, 512 KiB scope limits, cancellation on page exit/hide, generation checks against stale results, disabled controls while loading and correct unit/scope selection restoration. Added a reusable one-shot async query helper and responsiveness/lifecycle integration tests using synthetic service executables.


## 0.5.0 — 2026-11-17

Added a read-only Process tree with PID/start-time-aware parent relationships, orphan/cycle handling, search, expand/collapse and details access. Added process snapshot pause/resume and 1/2/5-second refresh choices. Process reports now omit usernames and command lines by default, with an explicit opt-in for detailed local reports. Added deterministic tree fixtures and offscreen integration coverage for snapshots, controls and both export modes.


- Unreleased: Fixed resource charts remaining static: live read-only tabs now refresh every two seconds while visible, pause when hidden, and reset Performance sampling after a visibility/page gap. Added timer lifecycle regression coverage; validation pending CI.

- Unreleased: Added direct BlueZ Bluetooth scanning and device actions (pair, connect, disconnect, trust, forget), with pairing prompts, bounded scan duration, and a desktop-settings fallback.
- Unreleased: Added a GPU tab that reads DRM/sysfs device identity, utilization, and video-memory counters when the active driver exposes them.
- Unreleased: Added Parent PID to the process table and exported process report so parent/child relationships are visible and reusable in diagnostics.
- Unreleased: Added per-process disk read/write rates from `/proc/<pid>/io`, with unavailable values identified and rates included in JSON exports.

## 0.4.0 — 2026-11-16

Added read-only process details with parent PID/name, executable target, working directory and command line. Details open from the selected process or by double-clicking a row. Unavailable `/proc` fields are reported as unavailable; no process controls are offered from the details view.

## 0.3.0 — 2026-11-16

Added **Tux Task Manager**, a separate lightweight process-monitor application alongside Tux Steward's existing system tools. It reads `/proc`, shows CPU, resident memory, PID, user and process state, supports search and sorting, and refreshes only while visible. End Task and Force stop are limited to the current user's processes, with PID-reuse checks and separate confirmation. Added local JSON export with a privacy warning and atomic save. Existing Tux Steward tools and the bounded, shell-free command runner remain available.

## 0.2.0 — 2026-11-15

Replaced the Python/Tk implementation, tests and packaging with C++17, Qt Widgets and CMake. Retained all 25 tools, seven grouped tabs, search, live bounded output, PolicyKit authentication, default-cancel command review and JSON reports. Added scrollable tool pages, resizable output, atomic report export, desktop installation and C++ subprocess/GUI tests. No Python runtime remains.

## Privacy cleanup — 2026-11-10

Removed account-specific references from validation documentation. Developer attribution is retained. Added exclusions and guidance to keep private machine data, local configuration and diagnostic exports out of source control.

## 0.1.1 — 2026-11-10

Live command output, case-insensitive tool search, bounded output/history memory, partial-output retention on timeout, and deadline handling for child-held output pipes. Added real subprocess regression tests and Python 3.10/3.12 CI matrix. Desktop visual testing remains pending.

## 0.1.0 — 2026-11-10

Initial development alpha: native dark GUI, seven grouped tabs, 25 explicit actions, PolicyKit authentication per admin action, command review, worker queue, dependency checks, JSON report export, source audit, tests and CI configuration.

