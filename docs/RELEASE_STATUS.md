# Release validation status

Checked 2026-11-26.

## 3.0.0-alpha.1: SysPulse experimental preview

The SysPulse application checkpoint `c2150885c82bb202df8b82f6da760cc41fcd4f99` passed all 24 regression suites in an isolated QEMU VM. The maintained source retains the inherited tools and legacy applications. The source synchronization fallback was verified against the exact local Git tree; it does not replace VM validation with hosted CI.

Native DEB, executable ZIP and TGZ archives were produced in that VM, checked for version, executable layout, documentation and private build-path exclusion. They target **Debian 13 x86_64**, with libc6 >= 2.41 and Qt >= 5.15.15. These are not Mint/Ubuntu packages. Clean package-manager installation, upgrades, removal and desktop integration remain unverified for this alpha.

A separate portable test preview passed recursive bundled-loader dependency checks and extracted-file checksum/version verification. Its runtime notices are included, but exact third-party source-version provenance remains incomplete. Keep broader bundled-runtime redistribution gated on that provenance; native packages and the corresponding application source do not bundle those runtime libraries.

See [executed validation](SYSPULSE_VALIDATION.md) and [download guidance](SYSPULSE_DOWNLOADS.md). An alpha label permits exploratory testing with these explicit limits; it is not stable, release-candidate, full XFCE, PolicyKit or hardware certification. Publication must be checked independently of local package creation.

GitHub Actions and CircleCI remain disabled. Manual VM packaging and source synchronization use no hosted workflow.

## 2.4.0-beta.1: source available, binary publication blocked

The integrated Task Manager source is merged at `a551dfa5d1a2f6506e5d40b992197ca7156a9d53`. It adds main/resource sidebars, first-class Startup apps and Services, separate service startup/runtime states, search and confirmed restart support.

Executed evidence:
- [Pre-release VM run](https://github.com/sudipdasin/tux-steward/actions/runs/44953839512): all 19 suites and package/upgrade/installed-GUI/removal checks passed on Ubuntu 22.04/GCC Release and Ubuntu 24.04/Clang Debug.
- [Tagged pipeline](https://github.com/sudipdasin/tux-steward/actions/runs/44954202865): Ubuntu 22.04 passed. Ubuntu 24.04 passed build/tests and package verification but later failed an historical-package download with curl exit 28 (connection timeout). The final packaging/upload job was skipped.
- GitHub release metadata currently contains zero binary/checksum assets for 2.4. Source archives are not executable downloads.

The timeout hardening committed after the tag adds connection, transfer, retry-budget and step limits to all eleven historical-DEB upgrade checks. It preserves checksum verification and all release gates. This workflow change has not been executed on a VM. No new GitHub Actions or CircleCI run was requested during this continuation because CI usage is restricted.

## Available verified binaries

[2.3.0-beta.1 downloads](https://github.com/sudipdasin/tux-steward/releases/tag/v2.3.0-beta.1): DEB, executable ZIP, TGZ and SHA256SUMS are uploaded. This version includes CPU/per-core, RAM, individual disk and network graphs; startup/services integration from 2.4 is source-only until its packaging gates pass.

## Remaining work

The 2.4 status above is historical. Continue current SysPulse verification in an isolated local VM without GitHub Actions or CircleCI. Do not replace a historical tag or describe the blocked 2.4 release as a verified executable release. Physical hardware, real PolicyKit prompts and Wayland remain unvalidated. Startup/service action regressions use isolated fixtures.
