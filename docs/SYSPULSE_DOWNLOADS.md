# SysPulse 3.0.0-alpha.1 testing downloads

This is an experimental C++17/Qt preview. The application sources passed all 24 suites in an isolated Debian-userspace QEMU VM. Use the checksum supplied with the exact downloaded file. Packages are unsigned.

## Native artifacts: Debian 13 x86_64

| Filename | Contents |
| --- | --- |
| `syspulse_3.0.0~alpha1_amd64.deb` | System package containing SysPulse and the three compatibility applications |
| `syspulse-3.0.0-alpha.1-x86_64.zip` | Native executables under `usr/bin`, desktop entries, icons and notices |
| `syspulse-3.0.0-alpha.1-x86_64.tar.gz` | The same native install tree as a tar archive |
| `SysPulse-3.0.0-alpha.1-source.tar.gz` | Corresponding application sources, build files, tests, packaging tools and documentation |
| `SHA256SUMS` | SHA-256 digests for those exact files |

**The native binaries require Debian 13-compatible ABIs: libc6 >= 2.41 and Qt >= 5.15.15. Do not install this DEB on Linux Mint/Ubuntu as a compatibility test.** Native archives use system runtime libraries; they are not portable bundles.

On a suitable Debian 13 test VM, verify checksums before installation. For an extracted native archive, launch `./usr/bin/syspulse` as the normal desktop user. Use the distribution package manager for a DEB. The SysPulse package replaces the legacy `tux-steward` package and retains personal configuration on removal. Clean package-manager install, upgrade, removal and desktop integration are still open validation gates; extraction and launch checks alone do not establish those results.

## Portable test preview

The separate `SysPulse-3.0.0-alpha.1-Linux-x86_64.zip` contains a bundled runtime, the optional Debian-only DEB, application source, checksums and verification evidence. Extract the **complete** ZIP into a folder, then run its lowercase `./syspulse` launcher as a normal desktop user. Keep `bin`, `lib`, `plugins` and `licenses` beside the launcher. Do not run `bin/syspulse` directly or start the app with sudo.

Its loader and static library dependency closure were verified in the isolated VM. Host fonts, X11/XWayland, graphics integrations, session services and authorization tools remain necessary. Exact third-party source-version provenance is incomplete where the execution image had no package database; dependency notices and the stated gaps accompany the preview. Broader public redistribution of the bundled runtime remains gated on resolving those gaps. Full Linux Mint/XFCE desktop compatibility has not been established.

Opening SysPulse starts no maintenance. Review each operation inside the app. Settings-changing catalog tools remain Experimental. Reports default to redacted summaries and nothing uploads automatically. Real PolicyKit prompts, full XFCE sessions, physical hardware and Wayland remain unvalidated. See [validation](SYSPULSE_VALIDATION.md) for the evidence and limits.

Developed by Sudip Das. Application license: GPL-3.0-or-later.
