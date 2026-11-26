# SysPulse

**XFCE-first Linux system tools · native C++17 / Qt5 · free and open source**

SysPulse brings performance monitoring, startup and service management, desktop tools, and reviewed maintenance workflows into one white-and-crimson interface. It is the next major generation of the Tux Steward codebase, with the existing tested foundation retained.

Current development milestone: **3.0.0-alpha.1**. This is experimental software, not stable or release-candidate certification. Read [milestone scope](docs/SYSPULSE_MILESTONE.md) and [executed VM validation](docs/SYSPULSE_VALIDATION.md) before testing.

## What works in this milestone

- Overview with live CPU, memory, disk, and network values and direct tool navigation.
- Detailed 60-second monitoring histories, dynamic per-core views, memory/swap detail, disk and network selectors, and 1/2/5-second sampling with pause/resume.
- Process list/tree, inspection, sorting, and identity-checked same-user end/force-stop controls.
- Startup applications with source/state filters, per-user overrides, restore/undo safeguards, and session applicability.
- Service runtime and startup state, user/system scope, filtering, selected-unit inspector, dependencies, journal access, reviewed actions, and activity history.
- Thirty-five reviewed catalog tools: all 25 inherited tools plus ten network, security, package, desktop, and boot diagnostics.
- Responsive task execution, bounded output, read-only cancellation, local reports, and a separate Experimental area for settings-changing catalog tasks.
- Native XFCE settings launchers, interface inspection with local addresses hidden by default, timer inspection, and startup recovery entry points.

The interface follows the selected **Precision** identity, **Observatory** monitoring depth, and **Forge** management layout. See [the UI specification](docs/SYSPULSE_UI_SPECIFICATION.md).

## Testing and installation

Read the [3.0 alpha download guidance](docs/SYSPULSE_DOWNLOADS.md) before choosing a package. It distinguishes Debian 13 native binaries from the separate portable test preview and records the remaining validation and runtime-provenance limits.

Native DEB/ZIP/TGZ packages require compatible system libraries. A package built in the Debian 13 validation VM must not be described as an Ubuntu/Mint-compatible binary without matching ABI verification. The optional portable runtime bundle includes its loader, Qt libraries, and platform plugins; target Mint/XFCE desktop, graphics, and authorization behavior still require testing on those systems.

No new downloadable release is claimed solely by a source commit. Published assets, checksums, tested target, and remaining limitations must be documented together. The previous Tux Steward releases remain historical testing snapshots; the current source is not proof that a similarly named binary exists on Releases.

For a verified native package, use your distribution's package manager. A SysPulse DEB replaces the legacy tux-steward package because it includes the legacy binaries. Do not force conflicting packages to coexist. User settings and private undo records are retained when packages are removed.

For a verified portable archive, extract it and run its lowercase `syspulse` launcher (`./syspulse`) as your normal desktop user. It does not install system libraries or enable background maintenance. Keep its bundled libraries together. Use the checksum supplied with that exact artifact.

The compatibility executables remain `tux-steward`, `tux-task-manager`, and `tux-setup`. The old setup wizard manages those three legacy programs; install the new SysPulse program using its native package or run the portable launcher. It must not be represented as a new SysPulse setup wizard.

## Build and verify in a VM

Use an isolated real VM. The reproducible local harness is documented under [tools/vm](tools/vm/README.md). Inside a compatible VM with CMake, a C++17 compiler, Qt 5.15 Core/Widgets/Network development files, and standard Linux utilities:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/syspulse --version
```

CTest includes inherited regression coverage and new parser/rate, management, tool workflow, navigation/search, report privacy, and install-layout checks. GUI tests use a virtual/offscreen display inside the VM. This does not substitute for full XFCE, PolicyKit, hardware, or cross-distribution testing.

CPack supports DEB, executable ZIP, and TGZ. Debian dependency generation normally uses installed package metadata. If that metadata is absent, configuration requires an explicit verified dependency list; it must never silently produce a dependency-free DEB. `tools/create-portable-bundle.sh` creates the separate bundled-runtime archive inside a VM and stops when required dependency notices are missing.

**GitHub Actions and CircleCI remain disabled indefinitely unless Sudip Das explicitly approves them again.** Their former definitions are archived in [ci/disabled](ci/disabled/README.md). No hosted workflow is required for local VM validation or packaging. Do not dispatch archived workflows or assume an old release event is safe on another branch.

## Scope, privacy, and feedback

There is no telemetry, automatic report upload, automatic cleanup, or background self-update. Opening a page does not perform maintenance. Actions are reviewed in-app, and reports default to summaries that omit command lines and raw diagnostics. Detailed output is local and may contain machine identifiers or filesystem paths: review it before sharing.

The richer category-based cleanup planner, snapshot manager, schedule editor, antivirus integration, and general configuration rollback are future work. Recovery is limited to explicitly supported saved states. Unsupported integrations are disabled or kept separate; a completed check is not a comprehensive security or health guarantee.

Use **Settings & help → Report a problem** or [open an issue](https://github.com/sudipdasin/tux-steward/issues/new/choose). Nothing is submitted automatically. Include the application version, distribution, XFCE/session type, steps, and whether the issue involved Experimental. Keep confidential details out of public issues.

Developers can start with [CONTRIBUTING.md](CONTRIBUTING.md), [AGENTS.md](AGENTS.md), the [project blueprint](docs/PROJECT_BLUEPRINT.md), the [UI specification](docs/SYSPULSE_UI_SPECIFICATION.md), and the [milestone scope](docs/SYSPULSE_MILESTONE.md). Verified fixes are inherited through shared source and regression tests; historical release archives do not update themselves. [Historical Tux Steward notes](docs/TUX_STEWARD_HISTORY.md) are retained separately.

Developed by **Sudip Das**. Licensed under [GNU GPL version 3 or later](LICENSE).
