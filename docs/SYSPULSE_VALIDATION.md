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

Final native and portable artifact checks are in progress. Native packages from this environment target Debian 13 x86_64 and require compatible system ABIs. They must not be described as Linux Mint/Ubuntu packages. The portable launcher includes a matching loader/runtime and keeps its library path out of system-helper environments; target desktop compatibility still needs independent verification.

## Outstanding release gates

- Full Linux Mint/XFCE login and desktop behavior, real PolicyKit prompts, user/system service mutation and startup behavior in actual sessions.
- Physical disk/network/GPU/Bluetooth/input/power integrations, Wayland/XWayland variants and representative hardware.
- Clean supported-distribution package-manager installation, upgrades from released versions, removal and desktop-menu integration. Archive extraction and install-layout tests do not establish these results.
- Exact third-party source-version provenance and corresponding-source delivery for broader bundled-runtime redistribution.

Default guest screenshots legitimately show disk/network unavailable when the VM exposes no eligible device. Synthetic fixtures or design mockups are not presented as hardware observations. System-changing catalog tasks remain Experimental; command timeout does not prove elevated descendants stopped. See the [milestone limitations](SYSPULSE_MILESTONE.md).

GitHub Actions and CircleCI remain disabled. Local commits and successful package checks do not by themselves establish a published GitHub release; remote upload/publication must be verified separately.
