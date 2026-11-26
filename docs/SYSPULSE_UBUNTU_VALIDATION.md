# SysPulse 3.0.0-alpha.2: executed Ubuntu / XFCE validation

Executed 26 November 2026 in two real isolated QEMU TCG virtual machines. This is an alpha testing release. GitHub Actions and CircleCI were not used.

## Source and packaging

Application/package candidate: local commit `c578e933171cb4da7f95ae7c2d2fa18f4ecf074a`, mirrored with the identical Git tree at remote commit `2904f0baf10cf455e1de05800922d69b8fd5c6bb`. Subsequent validation documentation does not change application sources. The published version tag identifies the final source/documentation tree; its corresponding source archive is supplied with the release.

The package was compiled in the Ubuntu 22.04 guest with GCC 11.4.0, Qt 5.15.3 and CMake Release configuration. All four executables retain version/execute checks. Native DEB, ZIP and TGZ contain application executables, desktop integration and license/documentation files; they use system libraries and do not bundle Qt or libc. The Debian package version is `3.0.0~alpha2`; the public filename avoids characters normalized by the release host.

`syspulse-3.0.0-alpha.2-x86_64-ubuntu-22.04.deb` requires amd64, libc6 >= 2.34, libgcc-s1 >= 3.0, libstdc++6 >= 6, Qt5 Core >= 5.15.1, GUI >= 5.8.0 (or its GLES alternative), Network >= 5.0.2 and Widgets >= 5.14.1. APT resolves the distribution's package aliases, including Noble's t64 transition. Recommended helpers are pkexec and xdg-utils; suggested XFCE components are xfce4-settings and xfce4-power-manager. Installing a package does not certify privileged actions.

## Results

| Check | Ubuntu 22.04 guest | Ubuntu 24.04 guest |
| --- | --- | --- |
| Ubuntu userspace | 22.04.5 LTS / Jammy | 24.04.5 LTS / Noble |
| Runtime Qt | 5.15.3 | 5.15.13 |
| Desktop session | XFCE 4.16 with xfwm4 | XFCE 4.18 with xfwm4 |
| Release compilation | Passed, GCC 11.4 | Same Jammy-built binary, no rebuild |
| Application CTest suites | 24/24 passed, 65.94 seconds | Not repeated; runtime compatibility checks below |
| Publication verifier regressions | 13/13 passed | Not repeated |
| Native package/archive verification | Passed | Same DEB used |
| Clean native DEB install | Passed | Passed |
| Runtime library closure / executable version | Passed | Passed |
| Desktop-entry launch as UID 1000 | Passed | Passed |
| Main window capture and visual review | Passed | Passed |
| Package removal and reinstallation | Passed | Passed |

Both guests booted a separate `6.12.107+deb13-amd64` kernel under QEMU. Ubuntu Base userspaces and signed Ubuntu APT metadata supplied the distribution packages. This is a real VM, not a host-side container or chroot; the kernel is explicitly supplied and is not a stock Ubuntu kernel. Source and package-cache mounts were read-only. Guest package installation used authenticated offline APT metadata; the guests had no network backend. Ubuntu 22.04 had 4 GiB RAM / six virtual CPUs and a three-job compilation; Ubuntu 24.04 had 3 GiB RAM / four virtual CPUs.

Desktop checks started a real xfce4-session and xfwm4 on Xvfb inside each VM with a session D-Bus. GIO launched the installed `/usr/share/applications/syspulse.desktop`. The harness checked the versioned main-window title and process, active XFCE/window-manager processes and a minimum-size main-window capture. The two captured application windows were visually reviewed. This establishes installed X11 desktop launch in those guest sessions, not a complete physical desktop/login/graphics certification.

## Failures retained and resolved

An initial minimal guest bootstrap attempted unpacking packages before satisfying Pre-Depends; proper APT installation replaced that bootstrap. The first application run had 23/24 passing suites because the minimal guest lacked desktop-file-validate. Installing Ubuntu's desktop-file-utils resolved the environment dependency; all 24 original suites then passed unchanged. Initial desktop harness attempts lacked GIO and then its matching libglib2.0-data package. Installing the complete official GIO package pair enabled the successful desktop-entry checks. Failed attempts were not promoted or counted as passing. Production tests and safety checks were not weakened.

Some minimal-session probes print expected diagnostics for missing display/session helpers or absent optional desktop applications. The successful installed checks explicitly verify the actual UID-1000 session and app window. These sessions do not exercise a complete distribution login manager, running systemd services or live authorization prompts.

## Install and limits

Verify the exact downloaded file with the release's SHA256SUMS, then install using the package manager on a compatible amd64 test system:

```sh
sudo apt install ./syspulse-3.0.0-alpha.2-x86_64-ubuntu-22.04.deb
```

Open SysPulse from the desktop application menu as your normal user, or run `syspulse`. Do not run the GUI with sudo. The package replaces legacy tux-steward binaries; personal settings and undo records are retained on removal. This run tested fresh installation/removal/reinstallation, not an upgrade from every historical package version.

Linux Mint 21/22 share the tested Ubuntu bases, but an actual Mint installation and the owner's hardware have not been tested. PolicyKit prompts, real systemd service mutations, hardware devices, production package changes, Wayland and complete desktop login integration remain unverified. Settings-changing catalog workflows remain Experimental and require in-app review. There is no automatic cleanup, telemetry, background updater or universal undo. Preserve these limits in release notes.

The release evidence archive contains guest build/test/install logs, capture helpers, desktop-check scripts and both screenshots. It excludes private host paths and host diagnostic logs. The source archive contains the corresponding C++ source and build/test/packaging files. No application was built or installed on the host.
