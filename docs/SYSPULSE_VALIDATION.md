# SysPulse 3.0.0-alpha.1 — executed validation

Date: 26 November 2026. Application source checkpoint: `c2150885c82bb202df8b82f6da760cc41fcd4f99`. Later documentation changes do not change the tested application sources. This is an experimental preview, not release-candidate or full XFCE/Mint certification.

## Execution environment

All application compilation, regression execution, runtime measurements and package checks run in a real isolated QEMU TCG virtual machine: six virtual CPUs, 4 GiB assigned RAM, Debian 13 userspace, Linux `6.12.107+deb13-amd64`, GNU C++ 14.2.0, Qt 5.15.15 and a Release build. Applications and tests run as an unprivileged guest user. Offscreen Qt tests run inside that VM; they are not host/container substitutes for a VM.

Source/toolchain mounts are read-only and only the dedicated VM output directory is writable. No user home, credentials, physical storage device or host procfs is supplied. A guest-only writable overlay supplies correctly root-owned protected system paths; the host's synthetic file ownership is not accepted by weakening production checks. The guest root directory is mode 0755.

## Regression evidence

| Check | Executed result |
| --- | --- |
| Core milestone plus isolated health-fixture correction | 24/24 passed, 105.40 seconds |
| Idle-notification branch, management and shell suites | 2/2 passed, 21.09 seconds |
| Service-action lifetime branch composed with idle changes | 4/4 passed, 39.73 seconds |
| Final merged application at `c215088` | 24/24 passed, 92.51 seconds |

Branch changes were committed separately and merged only after their affected VM regressions passed. The lifetime regression uses a synthetic helper and a ready/release gate: cancelling a query cannot complete or release the action while that helper is still alive. It does not exercise real privileged service changes.

Coverage includes resource parsing/rates/reset gaps, navigation/search, catalog review and execution, report privacy/permissions, process identity and subprocess lifecycle, startup precedence/overrides/undo, bounded asynchronous service queries, action admission, and install layout. Native package contents/version/privacy verification is an additional post-CTest check. Passing fixtures establish the tested behavior, not every distribution integration.

Earlier failed runs are retained as failed evidence: an ambiguous fuzzy-search fixture was narrowed; a health timeout fixture was isolated from unrelated command startup; guest file ownership/root-directory mode were corrected; and font configuration was repaired. Production trust checks and command deadlines were not relaxed to make these pass.

## Runtime observations

The same native C++ probe measured live and paused Overview windows before and after removing the 100 ms admission timer. In matched twelve-second offscreen VM samples, delivered application timer events changed from 132 to 12 while live, and 120 to zero while paused. Endpoint RSS was approximately 45 MiB, including probe instrumentation. See [SYSPULSE_BENCHMARKS.md](SYSPULSE_BENCHMARKS.md) for raw-result filenames, CPU values and limitations. These are single-run observations, not physical-hardware performance or peak-memory guarantees.

## Download and package checks

Native DEB, ZIP and TGZ archives were produced and checked inside the VM. Extracted native and portable executables reported `SysPulse 3.0.0-alpha.1`; the DEB contained its desktop entry, icon, license and privacy notice. Installed ELF metadata contained no build-directory RPATH/RUNPATH, and all four stripped application executables passed a separate scan for private host workspace paths.

The portable archive passed its recursive static dependency-closure check using the bundled loader with the system loader cache inhibited. Each resolved linked library stayed inside the bundle. Final ZIP extraction, every `CONTENTS.sha256` entry and the extracted launcher's version were checked in the guest. Host graphics integrations, fonts, desktop services and authorization tools remain separate runtime capabilities.

| Additional executed check | Result |
| --- | --- |
| Native DEB/ZIP/TGZ content and version verification | Passed |
| Portable static dependency closure and extracted-file hashes/version | Passed |
| Extracted portable launcher, real X11 main window under Xvfb inside QEMU | Passed; 1366 x 768 capture visually reviewed |
| Extracted native DEB executable, real X11 main window in the same guest | Passed; 1366 x 768 capture visually reviewed |
| Settled resource UI, navigation/search/review/privacy checks | 46 checks passed; Overview, Performance, Storage and Experimental captured |

The first X11 capture selected a 1 x 1 Qt helper window and was rejected on visual review. The capture helper was corrected to match the versioned main-window title and require at least 640 x 480 pixels; both package launches were repeated successfully. This was a verification-harness correction. The retained evidence includes the corrected helper and a note about the rejected capture; the rejected image is not used as a preview.

Native packages from this environment target **Debian 13 x86_64**, with libc6 >= 2.41 and Qt >= 5.15.15. They must not be described as Linux Mint/Ubuntu packages. The portable preview includes a matching loader/runtime and keeps its library path out of system-helper environments. Full target-desktop compatibility still needs independent verification, and exact third-party source-version provenance remains a gate for broader bundled-runtime redistribution. See [download guidance](SYSPULSE_DOWNLOADS.md).

## Outstanding release gates

- Full Linux Mint/XFCE login and desktop behavior, real PolicyKit prompts, user/system service mutation and startup behavior in actual sessions.
- Physical disk/network/GPU/Bluetooth/input/power integrations, Wayland/XWayland variants and representative hardware.
- Clean supported-distribution package-manager installation, upgrades from released versions, removal and desktop-menu integration. Archive extraction and install-layout tests do not establish these results.
- Exact third-party source-version provenance and corresponding-source delivery for broader bundled-runtime redistribution.

Default guest screenshots legitimately show disk/network unavailable when the VM exposes no eligible device. Synthetic fixtures or design mockups are not presented as hardware observations. System-changing catalog tasks remain Experimental; command timeout does not prove elevated descendants stopped. See the [milestone limitations](SYSPULSE_MILESTONE.md).

GitHub Actions and CircleCI remain disabled. Local commits and successful package checks do not by themselves establish a published GitHub release; remote upload/publication must be verified separately.

## Executed publication verification

The native alpha was published on 26 November 2026 at 15:36 UTC (21:06 IST).
The public tag `v3.0.0-alpha.1` resolves to source revision
`f0fe320677e40c05e0c176a2fd3efdbe961d9dcd`. Six uploaded assets' names, byte
sizes, upload states and SHA-256 digests match the prepared files. This includes
four payload files and canonical/build-name checksum metadata. GitHub's two
automatically generated source links are not uploaded native assets.

The offline development verifier in `tools/verify-release-assets.py` was then
run against the saved release/tag metadata and actual prepared public files in
the real isolated QEMU VM. It verified all six assets. Its thirteen independent
regression cases also passed, including wrong repository/tag target/maturity,
draft or incomplete publication, duplicate names, same-size tampering, size
mismatch, unexpected files, path escapes, symlink inputs and the actual
GitHub DEB filename-normalization failure mode. These are additional development
tool checks; the application CTest suite remains the previously passed 24 suites.

GitHub normalizes the public DEB's `~` to `.`. The published canonical checksum
list was corrected to match the public name before publication; the original
build-name checksum list is separately labeled. See the [publication receipt](releases/3.0.0-alpha.1.json)
and [current release status](RELEASE_STATUS.md). Application source and binary
bytes were not changed by the documentation, filename mapping or verifier work.
