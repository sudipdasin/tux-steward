# SysPulse 3.0.0-alpha.3: executed validation

Validated 27 November 2026. GitHub Actions and CircleCI were not used. All application builds, tests, package installation and GUI checks ran in real isolated QEMU TCG VMs.

## Exact application source

Final application checkpoint: local `6b1bbdb618090aac846c6a9de0af23ba48695e72`, identical remote tree at `b1609de4ad0292e1027da0eda5804dced71d34a6`. The application checkpoint tree is `1eb56121b903fe7145e3b2f9d06e56018abe446f`. Later validation/publication documentation does not change the application sources, headers, tests or CMake packaging. The version tag and supplied source archive identify the complete release documentation tree.

The guest verified the SHA-256 of a Git archive before applying it to its RAM-backed source tree. Every extracted source file was compared byte-for-byte with the read-only `/src` input. Only changed files received fresh timestamps. The final Release build used GCC 11.4.0 / Qt 5.15.3 on Ubuntu 22.04.5, four GiB RAM, six virtual CPUs and three compilation jobs. The separate runtime guest used Ubuntu 24.04.5 / Qt 5.15.13 / three GiB RAM. Both booted their own `6.12.107+deb13-amd64` kernel; this is an explicitly supplied kernel and Ubuntu Base/APT userspace, not a stock full distribution login installation.

## Results

| Check | Executed result |
| --- | --- |
| Final full application suite | 26/26 passed in 59.39 seconds |
| Manual update regression suite | 45 identity/version/channel/transport/privacy/lifecycle checks passed in the final Ubuntu run |
| Tool availability suite | 16 audit/filter/selection/trusted-launch checks passed; all 35 catalog tools retained |
| Publication helper regressions | 13/13 passed |
| Native DEB/ZIP/TGZ | Dependency, executable/version/layout and private-build-path checks passed |
| Previous alpha.2 → final alpha.3 DEB upgrade | Passed on Ubuntu 22.04 and 24.04 |
| Installed binary vs candidate package contents | Byte comparison passed in both guests |
| Actual installed desktop-entry launch | Passed in XFCE 4.16 and 4.18, as UID 1000, with xfwm4 and session D-Bus on Xvfb |
| Removal and fresh reinstallation | Passed in both guests; final dpkg audit clean |
| User-owned configuration marker | Contents and UID preserved through upgrade/removal/reinstall |
| Visual review | Actual Updates card, availability audit and installed main-window captures reviewed |
| VM source-mount safeguard | Separate real boot: read-only source mount and 148 tracked file hashes matched |
| Source-cache regression | Atomic file replacement during a real guest run refreshed both size and contents correctly |

The update transport tests use injected replies rather than Internet access from the guests. They cover malformed/draft/foreign releases, numeric prerelease ordering, stable-channel policy, downgrade prevention, fixed anonymous HTTPS GET, redirect refusal, response-size bounds, failure/stale-offer clearing, cancellation, absolute deadline and deferred network-manager creation. An additional offline replay of current public project metadata accepted 16 published release identities and correctly declined a downgrade to alpha.2. The source feed was retrieved through an authorized read-only connection; only published identity fields were transferred into the guest. This is not a live guest TLS/browser handoff certification.

## Native idle measurement

A real QApplication Overview probe warmed up for two seconds, then measured 4.005 seconds with monitoring paused and the offscreen renderer. It recorded **0 delivered application timer events**, 0 CPU ticks and **42,964 KiB RSS at the endpoint** (41.96 MiB). The probe's own stop timer is subtracted. This is one short emulated run with tick-quantized CPU counters and endpoint RSS, not peak memory, kernel wakeups, statistical significance or an Intel i3 hardware benchmark. Raw JSON accompanies the evidence archive.

## Corrected validation environment issue

The first combined build encountered stale/truncated source reads after a host branch switch under loose 9p caching. That attempt failed and was not promoted. Its compiler output is retained. A verified immutable Git archive, RAM-backed read-only source mount and full rebuild resolved it; the application suites then passed. The maintained VM generator now uses `cache=none` for source inputs, and the workflow requires fixed worktrees or immutable snapshots and explicit source identity. Two separate real boots verified initial source hashes/read-only access and atomic replacement coherence. No production test was removed or weakened.

Visual review then found excessive vertical spacing in Updates. A compact Precision card and corrected tab mnemonic were committed; all 26 suites, package checks, metadata replay and both distro upgrade/desktop checks were repeated on that final build.

## Limits and installation

The owner's target is Linux Mint 22.3 XFCE 64-bit. Its Ubuntu 24.04 package base is relevant to the executed runtime check; actual Mint hardware remains untested. Complete desktop login, real PolicyKit agents/prompts, production systemd mutations, hardware, Wayland and all historical upgrade paths remain unverified. The minimal sessions may log absent optional file-manager/session helpers; the actual UID-1000 app window and XFCE/window-manager processes were explicitly checked.

This remains an alpha. Settings-changing tools remain Experimental and require in-app review. Dependency presence is not an authorization or health certificate. The manual release checker opens reviewed project release notes only; it does not download, authenticate, install or roll back a package. There is no background updater, automatic maintenance, telemetry or automatic diagnostic upload. Native packages use system libraries, not bundled Qt/libc.

Verify the exact release checksum, then use `sudo apt install ./syspulse-3.0.0-alpha.3-x86_64-ubuntu-22.04.deb`. Open SysPulse as the normal desktop user. The filename identifies the older build ABI; the identical file was tested on Ubuntu 24.04. Read [the alpha.3 guide](SYSPULSE_ALPHA3.md) and current [download guidance](SYSPULSE_DOWNLOADS.md).
