Free open-source C++17/Qt Linux maintenance suite, Task Manager and native Setup. GPL-3.0-or-later; developed by Sudip Das. This is a 1.0 public testing beta.

## New
- Cross-tab tool search with per-category counts, automatic result navigation, empty-result guidance, Ctrl+F and Esc.
- Retains all 25 maintenance tools, separate Task Manager, Experimental pages and per-user setup/removal wizard.
- Automatic dual-VM regression checks for pull requests and main; beta publication waits for VM tests and package verification.
- Future versions inherit tested features and regression coverage from main. Existing release tags and installed user applications are not rewritten automatically.

## Download and test
Download the amd64 DEB plus SHA256SUMS for compatible Ubuntu/Mint systems. Verify the matching checksum, then install:
```sh
grep 'amd64.deb$' SHA256SUMS | sha256sum -c -
sudo apt install ./tux-steward_1.0.0.beta1_amd64.deb
```
Open Tux Steward, Tux Task Manager or Tux Steward Setup from the application menu. Remove the system package with `sudo apt remove tux-steward`. Configuration remains.

The x86_64 TGZ requires Qt 5.15 runtime libraries. Extract it and run `usr/bin/tux-setup` for a per-user installation/removal wizard, or launch `usr/bin/tux-steward` directly. Packages are unsigned.

## Validation and limits
Publication requires all nine suites on Ubuntu 22.04/GCC Release and Ubuntu 24.04/Clang Debug hosted VMs under Xvfb, plus package checks and actual installation, startup of all three installed GUIs and removal. The release packaging VM repeats checks on the tag. Consult the linked Actions runs for executed outcomes.

Real hardware, real PolicyKit authorization, Wayland and sign-in recovery remain unvalidated. Experimental tools remain grouped separately. This is a maintenance suite, not a desktop-shell replacement or stable-release guarantee.

## Feedback and contributions
Use Help → Send feedback or repository issue templates. Include version, distro/desktop, affected tab and reproduction steps; review and redact diagnostics before sharing. Feedback is never sent automatically. CONTRIBUTING.md and docs/RELEASE_POLICY.md describe builds, regression checks and forward continuity.
