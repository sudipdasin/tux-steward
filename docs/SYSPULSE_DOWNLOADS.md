# SysPulse 3.0.0-alpha.3: Ubuntu / XFCE testing downloads

[Open the published alpha.3 release](https://github.com/sudipdasin/tux-steward/releases/tag/v3.0.0-alpha.3). This is an experimental native C++17/Qt application, not a stable release. The source tag is `4851c3bae00093cafe0a843af80a11d72699d9bb`. All six uploaded assets were verified against their exact prepared names, sizes and SHA-256 digests; the canonical checksum list covers five payloads. See [publication receipt](releases/3.0.0-alpha.3.json).

## Choose the native amd64 package

| Filename | Contents |
| --- | --- |
| `syspulse-3.0.0-alpha.3-x86_64-ubuntu-22.04.deb` | Installed SysPulse plus the three legacy compatibility applications |
| `syspulse-3.0.0-alpha.3-x86_64-ubuntu-22.04.zip` | Native executable install tree, desktop entries, icons and notices |
| `syspulse-3.0.0-alpha.3-x86_64-ubuntu-22.04.tar.gz` | The same native executable install tree as TGZ |
| `SysPulse-3.0.0-alpha.3-source.tar.gz` | Corresponding application source, build/test/packaging files and documentation |
| `SysPulse-3.0.0-alpha.3-XFCE-VM-evidence.zip` | Guest logs, desktop launch/capture helpers, source identity and both actual main-window captures |
| `SHA256SUMS` | SHA-256 digests for the five payload files above |

The filename identifies the older **Ubuntu 22.04 build ABI**, not a restriction to that distribution version. The exact same DEB passed fresh installation, library/version checks, desktop-entry launch in a real XFCE session, removal and reinstallation on both Ubuntu 22.04 and Ubuntu 24.04 QEMU VMs. These native packages use system runtime libraries and do not bundle Qt/libc.

The owner uses **Linux Mint 22.3 XFCE 64-bit**. [Mint's release notes](https://linuxmint.com/rel_zena.php) identify its Ubuntu 24.04 base. The Ubuntu 24.04 runtime check is relevant compatibility evidence; actual Mint 22.3, the owner's hardware, complete login integration and live authorization prompts have not been tested. Do not describe the package as certified for those environments.

## Verify and install

Download the DEB and SHA256SUMS from the same release into one directory. When only the DEB is downloaded, ignore checksum entries for the other artifacts:

```sh
sha256sum --ignore-missing -c SHA256SUMS
sudo apt install ./syspulse-3.0.0-alpha.3-x86_64-ubuntu-22.04.deb
```

The DEB must report `OK` before installation. Its SHA-256 is `14cc7aedf8a55b2f28e3977081410b39e115a779003ab99393cfaad9507a7c9d`, and its byte size is 736940. The internal Debian package version is `3.0.0~alpha3`. Packages and checksum manifests are unsigned; hashes detect a mismatch and are not a signing system.

Open SysPulse from the desktop application menu as your normal user, or run `syspulse`. Do not start the GUI with sudo. APT resolves compatible dependencies. The package replaces the legacy `tux-steward` package because it contains its compatibility executables; do not force both conflicting packages to coexist. Removal retains personal settings and supported undo records. This run checked alpha.2 → alpha.3 upgrades, exact installed binaries and user-file preservation, followed by removal and fresh reinstallation on both Ubuntu bases. Other historical upgrade paths remain unverified.

For a native ZIP/TGZ, extract it and launch `./usr/bin/syspulse` on a system with the required libraries. It is not a bundled-runtime portable launcher. The legacy setup wizard still manages the three compatibility programs; it is not a new SysPulse installer.

## Manual update checks

Open Settings & help → Updates → Check for updates after installation. This reads public project release metadata only on an explicit check and opens an eligible release page for review. It does not download or install a package. Read [the alpha.3 guide](SYSPULSE_ALPHA3.md) for the bounded request, version policy, on-demand 35-tool dependency audit and trusted native launchers.

## Evidence and limits

All 26 application suites passed in the Ubuntu 22.04 build VM. Thirteen publication regressions and the final offline published-asset verifier passed in an isolated VM. Both Ubuntu guests ran actual xfce4-session/xfwm4 and session D-Bus under Xvfb. See [executed Ubuntu validation](SYSPULSE_ALPHA3_VALIDATION.md) for toolchain, guest/kernel identity, corrected environment failures and exact scope.

Actual Mint hardware, live PolicyKit/systemd mutations, Wayland and complete desktop login remain unverified. Settings-changing workflows remain Experimental and require scoped in-app review. Opening the app starts no maintenance. No telemetry, automatic uploads, background updater or universal undo is provided. GitHub Actions and CircleCI remain disabled.

## Earlier releases

[Alpha.2 downloads](https://github.com/sudipdasin/tux-steward/releases/tag/v3.0.0-alpha.2) remain an immutable previous Ubuntu/XFCE testing snapshot.

### Alpha.1

[Alpha.1 downloads](https://github.com/sudipdasin/tux-steward/releases/tag/v3.0.0-alpha.1) are immutable historical snapshots. Its native DEB/ZIP/TGZ target Debian 13 (libc6 >= 2.41 / Qt >= 5.15.15); they must not be used as Ubuntu/Mint packages. Its separate portable test preview has additional runtime-provenance limitations recorded in [the original validation](SYSPULSE_VALIDATION.md). Prefer the alpha.3 native package for the tested Ubuntu ABI targets.

Developed by Sudip Das. Application license: GPL-3.0-or-later.
