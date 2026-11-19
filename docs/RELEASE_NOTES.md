Free open-source C++17/Qt Linux maintenance suite, Task Manager and native Setup. GPL-3.0-or-later; developed by Sudip Das. This is a 1.4.1 public testing beta.

## Reliability fixes in 1.4.1
- Retain the task leader PID until inherited output is drained and process-group cleanup finishes. Exit observation no longer releases that identity early.
- Cover normal completion, timeout, cancellation and output-callback exceptions with real isolated subprocess regressions. Cleanup refuses signalling after lost ownership.
- Save reviewed reports with owner read/write permissions (0600), including replacement of permissive files. Tests exercise a broad umask.
- Fifteen regression suites. All selective-report, cancellation, history, Task Manager, Experimental grouping and setup features retained.

## Download and test
Download the amd64 DEB plus SHA256SUMS for compatible Ubuntu/Mint systems. Verify the matching checksum, then install:
```sh
grep 'amd64.deb$' SHA256SUMS | sha256sum -c -
sudo apt install ./tux-steward_1.4.1.beta1_amd64.deb
```
Open Tux Steward, Tux Task Manager or Tux Steward Setup from the application menu. Remove the system package with `sudo apt remove tux-steward`. Configuration remains.

The x86_64 TGZ requires Qt 5.15 runtime libraries. Extract it and run `usr/bin/tux-setup` for a per-user installation/removal wizard, or launch `usr/bin/tux-steward` directly. Packages are unsigned.

## Validation and limits
Publication requires all fifteen suites on Ubuntu 22.04/GCC Release and Ubuntu 24.04/Clang Debug hosted VMs under Xvfb, plus package checks, upgrade through checksum-pinned 0.9, 1.0, 1.1, 1.2, 1.3 and 1.4 DEBs, startup of all three installed GUIs and removal. The release packaging VM repeats checks on the tag. Consult the linked Actions runs for executed outcomes.

Real hardware, real PolicyKit authorization, Wayland and sign-in recovery remain unvalidated. Experimental tools remain grouped separately. This is a maintenance suite, not a desktop-shell replacement or stable-release guarantee.

Cancellation may leave detached descendants running and can omit unread output. It is not an undo mechanism. Settings-changing tasks cannot be stopped with this control.

## Feedback and contributions
Use Help → Send feedback or repository issue templates. Include version, distro/desktop, affected tab and reproduction steps; review and redact diagnostics before sharing. Feedback is never sent automatically. CONTRIBUTING.md and docs/RELEASE_POLICY.md describe builds, regression checks and forward continuity.
