# Roadmap

Delivered foundations:
- 0.2.0: C++17 / Qt Widgets migration, CMake and native tests.
- 0.3.0: standalone Tux Task Manager with same-user process controls.
- 0.4.0: per-process details including parent information and local paths at runtime.
- 0.5.0: process tree, pause/speed controls and opt-in detailed reports. The current Task Manager also includes resource charts, startup/service views, Bluetooth controls, GPU inspection and per-process disk rates; hardware support still depends on the host.

Next:
- Reduce UI-thread blocking in optional service and Bluetooth workflows.
- Validate real Linux Mint/XFCE sessions, PolicyKit prompts and hardware behavior.
- Add configurable health reports and reversible maintenance schedules.
- Develop recovery-safe input controls and managed Wine prefixes with atomic registries.
- Measure performance profiles on constrained hardware before adding tuning presets.
- 1.0.0: supported-distribution matrix, packaging, accessibility and public-readiness review.

Filesystem repair remains guidance-only until an offline workflow and strong mount checks are validated. No final release or public visibility change is automatic.
