Free open-source C++17/Qt Linux maintenance suite, Task Manager and native Setup. GPL-3.0-or-later; developed by Sudip Das. This is a 2.4 public testing beta.

## New in 2.4
- Windows-style main page sidebar and Performance resource sidebar. Startup apps and Services are first-class pages; Bluetooth/GPU remain Experimental.
- Startup apps retains user-scoped enable/disable, source filters, search, details and undo while preserving system defaults.
- Services now has a searchable table with unit, scope, startup state, runtime state and substate. Independent bounded asynchronous queries distinguish enabled from running. Start/stop/restart/enable/disable remain explicit confirmed operations; system actions use PolicyKit. Refresh never mutates services.
- No-match filters disable actions; stale queries are cancelled; actions stay single-flight with close protection. Added restart and runtime/search regression coverage and VM screenshots.
- Existing CPU/RAM/per-core/disk/network graphs, process controls, maintenance tools and privacy-reviewed reports retained. Nineteen suites and pinned upgrades through 2.3.

## Download and test
Download the amd64 DEB plus SHA256SUMS for compatible Ubuntu/Mint systems. Verify the matching checksum, then install:
```sh
grep 'amd64.deb$' SHA256SUMS | sha256sum -c -
sudo apt install ./tux-steward_2.4.0.beta1_amd64.deb
```
Open Tux Steward, Tux Task Manager or Tux Steward Setup from the application menu. Remove the system package with `sudo apt remove tux-steward`. Configuration remains.

The x86_64 TGZ requires Qt 5.15 runtime libraries. Extract it and run `usr/bin/tux-setup` for a per-user installation/removal wizard, or launch `usr/bin/tux-steward` directly. Packages are unsigned.

## Validation and limits
Publication requires all nineteen suites on Ubuntu 22.04/GCC Release and Ubuntu 24.04/Clang Debug hosted VMs under Xvfb, plus package checks, upgrade through checksum-pinned 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.4.1 and 2.0 DEBs, startup of all three installed GUIs and removal. The release packaging VM repeats checks on the tag. Consult the linked Actions runs for executed outcomes.

Real hardware, real PolicyKit authorization, Wayland and sign-in recovery remain unvalidated. Experimental tools remain grouped separately. This is a maintenance suite, not a desktop-shell replacement or stable-release guarantee.

Cancellation may leave detached descendants running and can omit unread output. It is not an undo mechanism. Settings-changing tasks cannot be stopped with this control.

## Feedback and contributions
Use Help → Send feedback or repository issue templates. Include version, distro/desktop, affected tab and reproduction steps; review and redact diagnostics before sharing. Feedback is never sent automatically. CONTRIBUTING.md and docs/RELEASE_POLICY.md describe builds, regression checks and forward continuity.

Health Check records diagnostic command results, not a health score or assurance that a system is healthy. Read-only output may contain private information. Cancelled checks can leave detached/elevated descendants running. No automatic repairs, dependency installation or unattended schedules are enabled.

### Executable ZIP download

The Releases page includes a ZIP with all three native executables, alongside the DEB, TGZ and SHA256SUMS. Extract the ZIP and run `./usr/bin/tux-setup` for per-user installation or `./usr/bin/tux-steward` directly. Compatible Qt 5.15 runtime libraries are required; this is not a self-contained AppImage. Both archives are extracted and checked for executable permissions, versions and private build paths on VMs; the release pipeline also launches all three GUIs from the ZIP.
