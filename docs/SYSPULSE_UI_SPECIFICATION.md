# SysPulse — Approved UI Specification

Design revision: 0.1 · 26 November 2026

This is the frozen design baseline, including mockup descriptions and planned interactions. The implemented 3.0.0-alpha.1 scope and remaining gaps are recorded in [SYSPULSE_MILESTONE.md](SYSPULSE_MILESTONE.md); executed results belong in [SYSPULSE_VALIDATION.md](SYSPULSE_VALIDATION.md). Mockup data and design requirements are not test results.

## Approved direction

The developer selected **concept 1, Precision**, as SysPulse’s visual identity, combined with **concept 3, Pulse Observatory**, for detailed resource monitoring, and **concept 5, Forge Workbench**, for service-management organization.

This selection replaces the earlier recommendation of Aurora Deck. Precision is the shared visual system throughout the application. Observatory and Forge contribute information hierarchy and interaction patterns; their separate dark palettes are not part of this approved direction.

SysPulse is a free, open-source Linux system and administration suite. The initial desktop target is XFCE. Distribution-specific tools must be shown according to detected capabilities, with additional desktop and distribution adapters added later. Tux Steward and the developer’s related Linux tools are candidate foundations for reuse.

These deliverables establish a design baseline. The images are mockups with illustrative data. They do not demonstrate running monitoring, completed diagnostics, tested cleanup, an installer, or a release. No additional design-selection approval is required to retain this chosen direction. Screen refinements can proceed within it.

## Visual identity

| Element | Definition |
| --- | --- |
| Product name | SysPulse |
| Edition label | XFCE Edition |
| Mark | Original crimson pulse-line symbol |
| Canvas | Pale grey, approximately `#F7F8FA` |
| Panels | White, approximately `#FFFFFF` |
| Main text | Charcoal, approximately `#202124` |
| Secondary text | Slate, approximately `#667085` |
| Brand accent | Crimson, approximately `#C8102E` |
| Selected row | Pale crimson, approximately `#FBE9ED` |
| Borders | Cool grey, approximately `#D5DAE3` |
| Positive state | Green with a written state label |
| Caution state | Amber with a written explanation |
| Resource traces | CPU green; memory crimson; disk blue; network purple; secondary traces slate |
| Panel shape | Approximately 10 px corner radius, fine border, restrained shadow |
| Typography | System sans-serif; monospace reserved for unit names, technical identifiers, and aligned values |

Use a consistent spacing scale of 4, 8, 12, 16, 24, and 32 logical pixels. Standard controls should be around 36–40 logical pixels high. Body text should remain readable at the platform’s normal font size; dense tables must offer a comfortable density option. The generated images guide appearance, while these semantic rules take precedence over incidental rendering differences.

Crimson identifies selection, navigation, and primary actions. Actual failures also need a distinct error icon and explanatory text. Never rely on color alone to communicate status. Avoid decorative health scores and unsupported optimization claims.

## Application shell

All screens use the same persistent shell:

- Native window decoration or an accessible desktop-compatible titlebar.
- A roughly 230–250 logical pixel left navigation area with pulse mark, product name, and edition.
- A top utility row with breadcrumb, global tool search, settings/theme access, and help.
- A central workspace containing a heading, module tabs, relevant actions, and main content.
- A persistent activity indicator when a task is running; optional docked activity details.
- A quiet footer with automation state. Mockups additionally show “Design preview · Sample data”.

The sidebar has the following structure:

| Group | Destination | Secondary screens |
| --- | --- | --- |
| Workspace | Overview | Status, analysis, recommendations, recent activity |
| Workspace | Performance | Summary, CPU, Memory, Disks, Network, Processes |
| Workspace | Storage & cleanup | Storage overview, Cleanup, Packages, SSD maintenance |
| System | Startup & services | Startup apps, Services |
| System | Network | Connections, Diagnostics, Throughput |
| System | XFCE desktop | Panel & session, Display & compositor, Input, Power |
| System | Security & privacy | Updates, Firewall, Permissions, Privacy checks |
| Tools | Schedules | Maintenance plans, task status, history |
| Tools | Recovery | Supported undo records, saved configurations, available snapshots |
| Tools | Reports | Analysis reports, change records, export |
| Tools | Experimental | Opt-in previews and unsupported integrations |
| Footer | Settings; Help | Preferences, capabilities, documentation, About |

Global search searches tool names and capabilities. It must not send machine data to an online search service. Selecting a search result opens the relevant module and preserves a clear breadcrumb.

## Screen designs

### Overview

Retain the approved Precision overview: a prominent analysis panel, four resource cards, recommended checks, and a quick-tool grid. The initial status is “Ready for analysis” rather than a fabricated score. Primary action: **Analyze system**. Secondary action: **Open reports**.

CPU, memory, disk, and network cards link to their detailed Performance tabs. Startup, services, disk usage, and XFCE settings have direct shortcuts. Recommendations identify whether they are unscanned, informational, or based on a completed check. Automation begins off and schedules begin unconfigured.

### Performance

Use Observatory’s monitoring depth in Precision’s light shell. A large CPU history panel contains total utilization and per-core plots. A right-hand watch panel links to startup apps and services and shows a compact process list. Memory, disk activity, and network history remain visible beneath the CPU plot.

The Summary, CPU, Memory, Disks, Network, and Processes tabs provide progressively deeper detail. Per-core layouts adapt to the actual processor rather than assuming four cores. Device and interface selectors appear when multiple devices exist.

Provide sampling choices of 1, 2, and 5 seconds, plus pause/resume and a clear sampling timestamp. Pausing stops visible sampling and labels the frozen view. Unavailable metrics display “Unavailable” with a reason, not a false zero. Do not draw new samples across a missing-data gap.

CPU percentages use a clearly defined scale. Memory shows used/total with documented semantics, cached/reclaimable memory, and swap separately. Disk distinguishes throughput, I/O operations, and busy time when available. Network separates receive/transmit and interface totals. Use consistent units and timestamps across cards, chart legends, and tables. When showing an aggregate throughput, compute it from the same samples as its component traces.

The process table supports search, sort, optional tree view, and a selected-process inspector. Show ownership and process identity before presenting an end-process action. Normal termination and force termination are distinct choices; force termination needs an explicit warning. Preserve existing process-identity checks from reused code.

### Services

Use Forge’s three-part composition: **service table → selected-unit inspector → activity dock**, translated entirely into Precision colors and navigation.

The top-level destination remains Startup & services. Its two tabs are Startup apps and Services. Service controls include scope, state filter, search, and refresh. Columns distinguish unit name, runtime state, startup configuration, and description; lower-level substates belong in the inspector when useful.

The inspector shows the unit identity, user/system scope, runtime state, startup state, dependencies, and journal access. Start, stop, restart, enable, and disable are separate commands. Disable controls that do not apply to the selected state. A running service can still be disabled for the next boot; make the difference clear.

Read-only selection does not request elevated privileges. An action that changes system state previews its unit, scope, expected effect, and privilege requirement. Refresh the resulting state after completion. Authorization cancellation is a normal outcome. Querying, action progress, and journal retrieval must remain asynchronous and bounded.

The dock separates Activity, Pending changes, and Reports. Recorded events distinguish read-only queries, requested actions, successful changes, partial results, and failures. “Clear view” must not imply undoing a system action.

### Startup apps

Use the same table/inspector/dock pattern as Services. Provide source and state filters, search, refresh, and columns for application, enabled state, source, and desktop applicability.

The inspector shows the selected application’s description, login state, source, applicable session, and override status. System defaults are preserved through per-user overrides. Offer a reviewable way to enable or disable the entry and restore the default when an override exists. Link related services when known.

Do not confuse a tray applet with its underlying service. Do not label an unmeasured startup cost as a measured impact. Only show timing or impact claims when they have a documented measurement source.

### Storage & cleanup

Separate storage visualization, cleanup, package maintenance, and SSD maintenance. Cleanup follows **Analyze → Review → Apply → Report**.

Review presents categories, estimated sizes, item counts, and selected scope. An item inspector explains what will be removed and what applications may regenerate. The summary recalculates from the current selection. Sample previews may show a reviewed selection; the real first-run selection must be explicit and inspectable.

The final action names the operation and summarizes irreversible effects. Cleanup, trash emptying, journal retention changes, package removal, and SSD trimming are separate operations. Show estimates as estimates; report actual results after completion. Deletion must never be described as universally undoable.

Package maintenance exposes an inspectable plan and dependency/removal summary before applying changes. Show APT-specific tools only on systems where the adapter is available. SSD tools use the relevant trim capabilities; do not offer a Windows-style registry cleaner or treat SSD defragmentation as a routine optimization.

### Network

Connection and throughput views share Performance’s resource vocabulary. Diagnostics show the target and operation before execution, followed by bounded results with timestamps. Connectivity checks, DNS checks, and firewall state remain distinguishable. Technical connection details can be revealed locally when necessary; exported summaries redact identifiers by default.

### XFCE desktop

Group panel/session behavior, display/compositor settings, input devices, and power controls. Read the current value before editing, display the proposed value, and retain the previous value for supported reversal. Clearly explain settings that need a new session or have limited Wayland/X11 support. Open an existing native XFCE configuration tool when that is the appropriate integration; do not imply SysPulse owns that component.

### Security & privacy

Present concrete checks: update state, firewall status, relevant permission findings, and supported privacy cleanup. Describe evidence and scope for each finding. Do not imply that a dashboard badge proves the entire machine is secure, or promise a bundled antivirus engine that has not been implemented. Unsupported security integrations remain unavailable or Experimental.

### Schedules

A plan editor exposes task selection, cadence, idle/power conditions where supported, required privileges, and missed-run behavior. Show the next run and recent outcomes. Automation starts off. Enabling a schedule is an explicit user action. Background cleanup must use the previously reviewed scope and surface partial failures.

### Recovery

Group supported configuration reversals, available snapshots, and saved change records. Show exactly which action can be reversed and whether its prerequisite still exists. Cache/file deletion, stopped processes, and arbitrary external actions must not receive a misleading universal Undo button. Snapshot features are capability-dependent.

### Reports

Provide analysis findings, selected plans, actual outcomes, errors, and timestamps. Default to a concise redacted summary with optional detailed local views and explicit export selection. Explain sensitive detail inclusion before export. Preserve the developer attribution while excluding private computer identifiers, local paths, and personal environment information from public examples and release files.

### Experimental

Maintain a visibly separate destination. Each tool identifies preview status, supported systems, purpose, and known limitation. Experiments are opt-in. A successful experiment does not automatically become a stable feature; promotion requires its validation evidence and compatibility review.

## Shared interaction rules

1. **Read before changing.** Opening a module or inspecting an entry performs bounded read-only queries.
2. **Show the action scope.** Preview the selected target, affected settings, likely interruption, privilege requirement, and recovery availability.
3. **Keep the interface responsive.** Run system work outside the UI thread, expose progress, handle timeouts, and allow cancellation where the operation supports it.
4. **Report the actual outcome.** Preserve partial results and refresh state. A cancelled or failed action must not receive a success badge.
5. **Offer recovery precisely.** Retain previous configuration values for supported changes and distinguish them from irreversible operations.
6. **Preserve context.** Retain tab selection, filters, table sorting, and selected entry where it remains valid after refresh.

Every module needs designed states for loading, no results, not installed, unsupported, permission denied, offline/unavailable, stale readings, cancellation, partial completion, and failure. Error messages include the operation and a useful next step without exposing sensitive details by default.

## Accessibility and adaptation

- Keyboard navigation and visible focus cover the sidebar, tabs, controls, table, inspector, and activity dock.
- Charts include textual values, labels, units, legends, and a pause control. State is never encoded only by color.
- Text and control contrast must be measured during implementation; the mockups alone are not an accessibility test.
- Respect desktop font scaling, high-DPI settings, and reduced-motion preferences. Provide tooltips and accessible names for icon-only buttons.
- Design for a 1366×768 desktop without hiding critical actions. At smaller usable widths, collapse the inspector into a details drawer and allow the sidebar to compact while retaining labels on demand. Do not shrink text to fit.
- Tables scroll within their workspace. The app must remain usable with large lists and long translated labels.

## Reuse and implementation handoff

The planned implementation remains native C++, using the existing Qt foundation where suitable. Separate presentation, capability discovery, read-only collectors, change planning, privileged actions, and report generation. Keep adapters replaceable so additional desktops and distributions do not require a new shell design.

Inventory existing Tux Steward implementations before porting them: metrics, process identity protection, startup overrides, service controls, task catalog, reports, and permission boundaries are candidate modules. Audit any other developer-owned project separately; this design does not authorize publishing unrelated private code or Pro modules.

Verified existing features and regression fixes must be carried forward through a shared maintained core and documented versioned changes. Do not promise automatic synchronization across independent repositories without an implemented mechanism. New features must not silently replace or drop previously tested behavior.

## Validation and release constraints

- GitHub Actions and CircleCI remain unused unless the developer explicitly approves them again. No push, tag, release, or workflow dispatch is part of this design deliverable.
- Application builds, installation checks, privilege flows, destructive-operation checks, and feature verification must use an isolated VM as previously directed.
- Use reproducible sample data for visual review. Real telemetry and machine-identifying details are not public design fixtures.
- Preserve developer credit. Remove private machine information from examples, reports, screenshots, generated artifacts, and eventual release contents.
- Completing this design does not establish release readiness. Implementation, VM verification, installation/uninstallation, and documented known limitations are separate acceptance stages.

## Current deliverables and next work

The selected Precision overview is the base reference. Four new matching previews cover Performance, Services, Startup apps, and Cleanup review. They show the combined visual identity, monitoring depth, management layout, and primary review flow.

The screen map above specifies the remaining modules, but they do not yet have individual high-fidelity previews or a working UI. Next work is to translate the shared shell into reusable C++ UI components, apply this design to the remaining modules, integrate validated functionality, and verify the resulting application in a VM. These previews should not be described as implemented screens.
