Free open-source C++17/Qt Linux maintenance suite, Task Manager and native Setup. GPL-3.0-or-later; developed by Sudip Das. This is a 2.0 public testing beta.

## New in 2.0
- Guided Health Check workspace, opened from the main button or View menu. Select from storage, memory, disks, failed services, kernel warnings and system warnings.
- Review exact read-only commands before starting. Selection does not execute anything. Unavailable selected dependencies block GUI start.
- Sequential background execution, per-check progress, stop-on-failure or continue, and Stop check. Closing during a run requests stop and waits for completion; remaining checks do not run.
- Results enter the latest-20 session history and existing selective report review. Summary remains default; diagnostic output is opt-in and saved reports are owner-only. Nothing uploads automatically.
- All 25 individual tools, Task Manager, native setup/removal and Experimental grouping retained. Existing report schema and command review remain compatible.
- Seventeen regression suites, including isolated real helper processes, invalid-plan refusal, failure policies, cancellation, timeout, missing dependencies and main-window history integration.

## Download and test
Download the amd64 DEB plus SHA256SUMS for compatible Ubuntu/Mint systems. Verify the matching checksum, then install:
```sh
grep 'amd64.deb$' SHA256SUMS | sha256sum -c -
sudo apt install ./tux-steward_2.0.0.beta1_amd64.deb
```
Open Tux Steward, Tux Task Manager or Tux Steward Setup from the application menu. Remove the system package with `sudo apt remove tux-steward`. Configuration remains.

The x86_64 TGZ requires Qt 5.15 runtime libraries. Extract it and run `usr/bin/tux-setup` for a per-user installation/removal wizard, or launch `usr/bin/tux-steward` directly. Packages are unsigned.

## Validation and limits
Publication requires all seventeen suites on Ubuntu 22.04/GCC Release and Ubuntu 24.04/Clang Debug hosted VMs under Xvfb, plus package checks, upgrade through checksum-pinned 0.9, 1.0, 1.1, 1.2, 1.3, 1.4 and 1.4.1 DEBs, startup of all three installed GUIs and removal. The release packaging VM repeats checks on the tag. Consult the linked Actions runs for executed outcomes.

Real hardware, real PolicyKit authorization, Wayland and sign-in recovery remain unvalidated. Experimental tools remain grouped separately. This is a maintenance suite, not a desktop-shell replacement or stable-release guarantee.

Cancellation may leave detached descendants running and can omit unread output. It is not an undo mechanism. Settings-changing tasks cannot be stopped with this control.

## Feedback and contributions
Use Help → Send feedback or repository issue templates. Include version, distro/desktop, affected tab and reproduction steps; review and redact diagnostics before sharing. Feedback is never sent automatically. CONTRIBUTING.md and docs/RELEASE_POLICY.md describe builds, regression checks and forward continuity.

Health Check records diagnostic command results, not a health score or assurance that a system is healthy. Read-only output may contain private information. Cancelled checks can leave detached/elevated descendants running. No automatic repairs, dependency installation or unattended schedules are enabled.
