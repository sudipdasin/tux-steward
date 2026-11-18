# Beta readiness — 0.8.0-beta.1

This is a private beta preparation checkpoint, not a certified release candidate.
No release, tag, repository visibility change or automatic upload is performed by packaging.

## Evidence and remaining gates

| Area | Automated coverage | Required desktop validation before a public beta |
| --- | --- | --- |
| Process control | Stable pidfd handle, UID/start identity checks, protected targets, TERM/KILL against owned test children | Normal-user controls on supported Mint/Ubuntu kernels; unavailable-kernel refusal |
| Services | Synthetic query cancellation, timeout/output bounds, duplicate-action prevention, failed-start recovery | Real user/system units and PolicyKit accept, deny and cancel paths |
| Startup Apps | Isolated XDG fixtures, precedence, malformed data, overrides and undo | Actual XFCE sign-in, recovery after interrupted writes |
| Interface | Offscreen tabs, refresh/pause lifecycle, process tree and export tests | Mint/XFCE X11 and Wayland sessions, scaling, keyboard navigation |
| Packaging | Staged install, desktop entries, headless help/version; CPack DEB/TGZ | Clean supported-distribution install, upgrade and uninstall |
| Hardware | Read-only fallback logic | Bluetooth pairing, GPU counters, network/disk accuracy on real devices |

Local development validation uses Debian 13 with GCC 14 and Qt 5.15. This environment lacks the distribution package database, so local DEB dependency metadata cannot be certified; the package verifier rejects it. Primary CI targets Ubuntu 22.04; consult the commit's CI status for its result. Offscreen tests do not establish desktop or hardware compatibility. The eight CTest suites cover fixtures and selected live read-only interfaces; they are not a security audit.

## Build private packages

```sh
sudo apt install build-essential cmake qtbase5-dev dpkg-dev file
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake
cmake -DPACKAGE_DIR="$PWD/build/packages" -DRELEASE_VERSION="$(./build/tux-steward --version | awk '{print $NF}')" -P tests/verify_packages.cmake
```

Packages appear in `build/packages`. The Debian version is `0.8.0~beta1`, allowing a later stable 0.8.0 upgrade. Dependencies are detected from the build distribution: a Debian-built package is not automatically compatible with Mint. The TGZ also needs compatible Qt/system libraries; it is not a standalone application. CI stores final packages and SHA256SUMS as private job artifacts. Checksums verify file integrity, not publisher authenticity; packages are unsigned.

Run either installed binary with `--version` or `--help` without a display. Run the actual GUI as a normal desktop user. Never run the GUI as root.

## Operational limits

Process termination requires kernel pidfd support and permission. Unsupported or blocked facilities refuse the action; there is no numeric-PID fallback. A stable handle prevents signaling a replacement process after PID reuse, while the kernel still checks signaling permissions.

Service mutations disable overlapping actions and retain only a bounded output tail. They do not have a forced authentication timeout: a desktop authentication agent must complete or cancel the request. Maintenance timeouts cannot guarantee that an elevated descendant stopped or that partial changes were undone.

Keep exports and startup undo records private. Default report redaction does not make all process names or metrics anonymous. Review diagnostics before sharing. Developer attribution remains in the project; machine logs, personal configuration and local source paths must not be shipped.

Before marking a release candidate, record real desktop results above, resolve discovered blockers, verify package privacy and dependencies on the supported baseline, and repeat the full automated suite. Publication remains an explicit owner decision.
