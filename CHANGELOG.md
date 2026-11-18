# Changelog

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

