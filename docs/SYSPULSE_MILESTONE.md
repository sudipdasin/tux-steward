# SysPulse 3.0.0-alpha.3

SysPulse is the next major application generation built on the Tux Steward C++ foundation. This is an experimental development milestone, with the approved Precision identity, Observatory monitoring depth, and Forge management layout.

Alpha 2 adds explicit native package platform labels, XFCE launch identification and recommended authorization/desktop helpers. Its native Ubuntu build and installation evidence is recorded separately from the earlier Debian 13 alpha; package compatibility follows the tested target rather than the desktop name alone.

See [the alpha.3 guide](SYSPULSE_ALPHA3.md) for the manual release checker, on-demand 35-tool dependency audit and trusted XFCE launchers. These features preserve explicit user control and do not implement automatic package installation.

## Included

- White-and-crimson persistent navigation, tool search, overview, settings/help, and local reports.
- Live CPU/per-core, memory/swap, disk read/write/busy time, and network receive/transmit history; 1/2/5 second sampling, pause, and device/interface selection.
- The inherited process list/tree, inspect and identity-checked same-user termination controls.
- Startup entry search and filters, source/session details, per-user overrides, and supported undo.
- Service runtime/startup state, user/system scope, search, selected-unit details, reviewed actions and activity.
- All 25 inherited catalog tools plus ten additional read-only network, package, desktop, boot, and security diagnostics.
- Reviewed asynchronous tool execution, bounded output, read-only cancellation, redacted-summary exports, and a separate Experimental destination for settings-changing maintenance.
- Native XFCE settings launchers, local interface inspection with addresses hidden by default, timer inspection, and a recovery entry point for startup overrides.
- The three legacy applications remain included for compatibility.

## Scope and limitations

The new application is not a stable or release-candidate certification. This milestone does not implement a new automatic cleanup engine, snapshot manager, schedule editor, universal undo, antivirus engine, or automatic update mechanism. Storage actions currently use reviewed catalog workflows; the earlier cleanup mockup describes a later richer category-selection interface.

Native desktop configuration requires the relevant XFCE components. APT, UFW, systemd, PolicyKit, X11 input, and gaming integrations are detected at runtime and remain unavailable when missing. A successful diagnostic exit is not a comprehensive health or security assessment.

Service and package actions require deliberate review and may interrupt the system. Settings-changing catalog tools remain Experimental. Startup changes use the existing guarded per-user override backend. Do not assume an action is reversible unless a specific saved state is available.

The service manager keeps a reviewed action busy until its observed helper exits, including a pending authorization prompt. Query deadlines and query cancellation do not release that action. The inherited catalog runner has bounded command deadlines and warns when elevated descendants may remain running; a timeout does not prove that a privileged operation stopped. Inspect the actual system state before retrying. UI admission guards are not a system-wide transaction lock, and another application can change the same resources.

## Validation

See `docs/SYSPULSE_UBUNTU_VALIDATION.md` for the alpha.2 Ubuntu/XFCE results and `docs/SYSPULSE_VALIDATION.md` for the earlier Debian run. They record exact package targets and outstanding desktop/hardware validation. Generated design images are not execution evidence. All application builds and tests for this milestone must run in a real VM; a container, chroot, or offscreen renderer alone does not satisfy that requirement.

GitHub Actions and CircleCI are disabled indefinitely unless the developer explicitly approves them. Archived workflow definitions are historical references only. Source synchronization uses skip-CI commits; no pipeline is dispatched.

## Privacy and feedback

No telemetry, automatic uploads, or background maintenance starts with the app. Reports remain local, summary is the default, and detailed output needs fresh consent. Local detailed diagnostics may contain identifiers and filesystem paths; review them before sharing.

Use Settings & help → Report a problem to open the issue form. Nothing is submitted automatically. Include the SysPulse version, distribution, desktop/session, reproducible steps, and whether the affected feature was Experimental. Exclude confidential computer information from public issues.

Developed by Sudip Das. GNU GPL version 3 or later. Existing tested features are retained through shared source and regression tests; fixes are carried forward in the maintained codebase. Published historical packages are immutable snapshots and do not receive changes automatically.
