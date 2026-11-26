# SysPulse

**XFCE-first Linux system tools · native C++17 / Qt5 · free and open source**

SysPulse brings performance monitoring, startup and service management, desktop tools, and reviewed maintenance workflows into one white-and-crimson interface. It is the next major generation of the Tux Steward codebase, with the existing tested foundation retained.

Current development milestone: **3.0.0-alpha.2**. This is experimental software, not stable or release-candidate certification. Read [milestone scope](docs/SYSPULSE_MILESTONE.md) and [executed VM validation](docs/SYSPULSE_VALIDATION.md) before testing. The alpha.2 native package passed [Ubuntu 22.04 / 24.04 XFCE VM validation](docs/SYSPULSE_UBUNTU_VALIDATION.md); its publication status is recorded below.

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

The [3.0.0-alpha.2 Ubuntu / XFCE testing release](https://github.com/sudipdasin/tux-steward/releases/tag/v3.0.0-alpha.2) is live with a native amd64 DEB, executable ZIP/TGZ, corresponding source, VM evidence and checksums. All six published assets were independently verified. Read [download guidance](docs/SYSPULSE_DOWNLOADS.md) and [the Ubuntu validation matrix](docs/SYSPULSE_UBUNTU_VALIDATION.md).

The DEB was built on Ubuntu 22.04 and the same file passed installed XFCE launch, removal and reinstallation on Ubuntu 22.04 and 24.04 real VMs. It is the native testing download for the owner's Mint 22.3 XFCE 64-bit system; actual Mint hardware remains untested. Verify the checksum before installation:

```sh
sudo apt install ./syspulse-3.0.0-alpha.2-x86_64-ubuntu-22.04.deb
```

Open SysPulse from the desktop menu as your normal user, or run `syspulse`. Native packages use system runtime libraries. They replace the legacy tux-steward package and retain personal settings on removal. The compatibility executables remain tux-steward, tux-task-manager and tux-setup; the old wizard manages those legacy programs, not a new SysPulse installation.

The previous alpha.1 native packages target Debian 13 and must not be installed as Mint/Ubuntu packages. Historical releases remain immutable. Full Mint/hardware, login, PolicyKit and Wayland verification remains open; see the exact evidence before testing settings-changing workflows.

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
