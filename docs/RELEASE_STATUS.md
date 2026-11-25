# Release validation status

Checked 2026-11-25.

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

Run the hardened workflow on an available VM when CI usage is authorized, resolve any failures, and repeat tagged packaging plus installed/ZIP GUI smoke checks before publishing new binaries. Do not replace a historical tag or describe the blocked 2.4 release as a verified executable release. Physical hardware, real PolicyKit prompts and Wayland remain unvalidated. Startup/service action regressions use isolated fixtures.
