# Roadmap

Delivered foundations:
- 0.2.0: C++17 / Qt Widgets migration, CMake and native tests.
- 0.3.0: standalone Tux Task Manager with same-user process controls.
- 0.4.0: per-process details including parent information and local paths at runtime.
- 0.7.0: XDG-aware startup table, user-only overrides and one-step undo.
- 0.6.0: nonblocking bounded service-list queries and stale-result cancellation.
- 0.5.0: process tree, pause/speed controls and opt-in detailed reports. The current Task Manager also includes resource charts, startup/service views, Bluetooth controls, GPU inspection and per-process disk rates; hardware support still depends on the host.

- 2.0 beta: guided read-only Health Check plans, sequential execution, progress, cancellation, history/report integration.

- 2.1 beta: read-only health presets, per-check result inspection and review-first incomplete recovery.

- 2.2 beta: outcome triage, filtered report review and reusable Health Check plans.

Next:
- Reduce UI-thread blocking in optional service and Bluetooth workflows.
- Validate real Linux Mint/XFCE sessions, PolicyKit prompts and hardware behavior.
- Validate guided health reports on supported desktops; design reversible maintenance schedules separately.
- Develop recovery-safe input controls and managed Wine prefixes with atomic registries.
- Measure performance profiles on constrained hardware before adding tuning presets.
- 1.0.0: supported-distribution matrix, packaging, accessibility and public-readiness review.

Filesystem repair remains guidance-only until an offline workflow and strong mount checks are validated. Releases have standing owner approval under [the project blueprint](PROJECT_BLUEPRINT.md); publish the appropriate maturity level after its verification gates pass. Repository visibility changes are separate from routine release publication.

