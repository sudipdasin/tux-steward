# Beta readiness — 2.1.0-beta.1

This is a public testing beta. Automated checks cover key behavior, but the real desktop gates below remain unverified. Experimental workflows are grouped separately in both applications.
Local CPack does not publish. Owner-created beta tags trigger VM-gated prerelease publication; repository visibility is unchanged.

## Evidence and remaining gates

| Area | Automated coverage | Required desktop validation before a public beta |
| --- | --- | --- |
| Process control | Stable pidfd handle, UID/start identity checks, protected targets, TERM/KILL against owned test children | Normal-user controls on supported Mint/Ubuntu kernels; unavailable-kernel refusal |
| Services | Synthetic query cancellation, timeout/output bounds, duplicate-action prevention, failed-start recovery | Real user/system units and PolicyKit accept, deny and cancel paths |
| Startup Apps | Isolated XDG fixtures, precedence, malformed data, overrides and undo | Actual XFCE sign-in, recovery after interrupted writes |
| Interface | Offscreen tabs, refresh/pause lifecycle, process tree and export tests | Mint/XFCE X11 and Wayland sessions, scaling, keyboard navigation |
| Packaging | Staged install, desktop entries, headless help/version; CPack DEB/TGZ | Clean supported-distribution install, upgrade and uninstall |
| Hardware | Read-only fallback logic | Bluetooth pairing, GPU counters, network/disk accuracy on real devices |

Current application builds and checks run on disposable hosted Ubuntu VMs with GCC/Clang and Qt 5.15. All eighteen suites must pass for this version; consult the release/PR for executed results. These fixtures and selected live read-only interfaces do not establish hardware compatibility or constitute a security audit.

## Build packages

```sh
sudo apt install build-essential cmake qtbase5-dev dpkg-dev file
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake
cmake -DPACKAGE_DIR="$PWD/build/packages" -DRELEASE_VERSION="$(./build/tux-steward --version | awk '{print $NF}')" -P tests/verify_packages.cmake
```

Packages appear in `build/packages`. The Debian version is `2.1.0~beta1`, allowing a later stable 2.1.0 upgrade. Dependencies are detected from the build distribution: a Debian-built package is not automatically compatible with Mint. The TGZ also needs compatible Qt/system libraries; it is not a standalone application. The release workflow publishes verified packages and SHA256SUMS as public prerelease assets. Checksums verify file integrity, not publisher authenticity; packages are unsigned.

Run all three installed binaries with `--version` or `--help` without a display. Run the actual GUI as a normal desktop user. Never run the GUI as root.

## Operational limits

Process termination requires kernel pidfd support and permission. Unsupported or blocked facilities refuse the action; there is no numeric-PID fallback. A stable handle prevents signaling a replacement process after PID reuse, while the kernel still checks signaling permissions.

Service mutations disable overlapping actions, retain only a bounded output tail and block normal window closing until completion or failure. Forced termination or session shutdown is outside this guard. They do not have a forced authentication timeout: a desktop authentication agent must complete or cancel the request. Maintenance timeouts cannot guarantee that an elevated descendant stopped or that partial changes were undone.

Keep exports and startup undo records private. Default report redaction does not make all process names or metrics anonymous. Review diagnostics before sharing. Developer attribution remains in the project; machine logs, personal configuration and local source paths must not be shipped.

Before marking a release candidate, record real desktop results above, resolve discovered blockers, verify package privacy and dependencies on the supported baseline, and repeat the full automated suite. Publication remains an explicit owner decision.

## 0.9 setup and VM validation

The native setup wizard manages only a fixed inventory of per-user files. It refuses existing installations and menu collisions, checks hashes before removing any known file, rejects symlink ancestors and retains unrelated files. A missing/corrupt record requires manual inspection; no recursive user-directory deletion is performed. Failed operations report incomplete cleanup and retain records when possible. It does not replace the distribution package manager or change the desktop shell.

The explicitly dispatched or release-tag-invoked GitHub workflow runs all eighteen suites on separate Ubuntu 22.04/GCC Release and Ubuntu 24.04/Clang Debug VMs with an Xvfb virtual X desktop, then builds/verifies packages and checks actual system installation, GUI startup and removal. Release packaging repeats the VM checks for the published tag. Execution status is recorded in the release/PR; configuration alone is not evidence of a pass. VM virtual desktops do not validate real PolicyKit interaction or hardware behavior.

## 1.2 history and current CI policy

The local history snapshot is capped at 20 records. Only an exact current catalog title/argument-array match can request rerun, and the normal default-cancel review remains mandatory. There is no persistence or automatic command replay. Manual VM validation is used while automatic PR/main checks remain paused by the repository resource policy. Tag publication retains the full dual-VM gate.

## Diagnostic cancellation

Cancellation is available only for read-only maintenance actions. Tests use owned child processes and isolated diagnostic/configuration helpers to check cancellation, cleanup, unrelated-process retention, token reset and mutation refusal. Timeout and cancellation remain distinct. Detached processes are outside the group cleanup boundary; cancellation is not an undo mechanism or a real hardware/authorization validation.

## 1.4 selective report gate

The report-selection suite verifies exact subsets in summary/detailed modes, exclusion of private fixture fields, chronological output, empty-state refusal, bounded snapshots, fresh consent, file-picker cancellation and byte-exact atomic save. All eighteen suites and installed-package upgrade/launch/removal checks must pass on both hosted Ubuntu VMs before release. Real hardware, interactive authorization and Wayland remain unverified.

## 1.4.1 lifecycle and report-permission gate

The runner-lifecycle suite uses an isolated subreaper fixture to assert that an exited leader remains waitable while descendants hold the output pipe, then verifies leader/descendant cleanup for completion, timeout, cancellation and callback failure. Report selection tests check 0600 permissions for new files and replacement of permissive files under a broad umask. POSIX permissions do not prevent access by privileged users or guarantee privacy on remote destinations or via filesystem ACLs.

## 2.0 Health Check gate

Core health-plan and GUI health-workspace suites validate the full read-only allowlist before execution, ordered results, stop/continue policy, active and pre-start cancellation, timeout, missing dependencies, explicit review, close guard, summary report review and main-window history import. Fixtures use isolated helpers and never real configuration mutations. Every existing suite remains. Health Check completion is not a health guarantee; machine output must be reviewed before sharing.

## 2.1 guided recovery gate

The health-recovery suite verifies fixed presets, ordered incomplete selection, successful exclusion, cancellation/timeout handling and refusal of duplicate/altered/unrelated results. GUI checks verify prepared plans execute nothing, status rows and selected-result details, review-before-rerun, preserved prior results and a successful follow-up run. Existing health, cancellation, report privacy, setup and process-control suites remain.
