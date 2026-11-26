# Isolated VM validation

SysPulse application builds and checks run in a real QEMU virtual machine. A
container or a chroot is not counted as a VM. GitHub Actions and CircleCI are
not used by this workflow.

The validation harness uses QEMU's TCG CPU emulation, a Debian Linux kernel,
and a small BusyBox initramfs. The guest has its own kernel, `/proc`, `/sys`,
`/dev`, temporary directory, home directory, and unprivileged test user. No
network adapter is attached. It mounts the host's `/usr`, an extracted native
CMake/Qt toolchain, and the project source **read-only** using virtio-9p. Only a
dedicated output directory is writable from the guest.

This verifies Linux/Qt behavior under a separate guest kernel. It does not
constitute a full XFCE desktop-session, PolicyKit-agent, GPU, or hardware test.
Those checks still require an appropriate XFCE VM installation.

## Run a prepared VM

Obtain a trusted Linux kernel, a compatible BusyBox static executable, and QEMU.
Keep third-party binaries and the generated initramfs outside the repository.
The lab's `run-vm.sh` launches the virtual machine and passes it the source
folder as its first optional argument. Its `results/job.sh` is executed as
UID/GID 1000 inside the guest.

Guest paths:

| Path | Access | Purpose |
| --- | --- | --- |
| `/usr` | Guest metadata overlay over read-only host lower | Debian userspace and compiler |
| `/opt/builddeps` | Read-only | Extracted CMake and Qt development toolchain |
| `/src` | Read-only | Project source snapshot |
| `/out` | Read/write | Dedicated build, log, image, package, and result directory |
| `/tmp`, `/home/tester` | Guest-only | Ephemeral state |

The launch harness should use `-accel tcg,thread=multi`, a serial console, no
network adapter, and `readonly=on` on all virtio-9p inputs. Never expose a user's
home directory, host `/proc`, credentials, or an unrestricted writable host
root to the guest.

The guest records its kernel identity and runs `results/job.sh`, records the
command status in `results/job.exit`, and powers off. The QEMU process exiting
successfully by itself is not a test success: inspect the job exit marker and
the test results. The project source stays read-only, preventing tests from
changing the checked-out worktree.

## Guest build and tests

Use the provided `guest-build.sh` as `results/job.sh`, with optional environment
variables `SYSPULSE_BUILD_DIR` and `SYSPULSE_BUILD_JOBS` supplied by the guest
launcher. It configures and builds from `/src`, runs CTest, and writes logs and
an explicit status marker under `/out`. Set the job count for the emulated CPU
budget. Qt GUI tests use the offscreen platform in the guest.

A successful run establishes only the checks listed in its logs. Preserve
failed test output, known limitations, the guest kernel, toolchain versions,
and source commit identity in the release verification notes. Never report
an unexecuted installation, GUI session, hardware operation, or service mutation
as tested.

## Incremental VM session

Use `guest-session.sh` as the guest job when several fix/test cycles are needed.
The guest writes `queue/guest.ready` under the dedicated output directory.
Write numbered shell jobs (`queue/1.sh`, `queue/2.sh`, …) atomically after their
contents are complete. The guest executes each job as the unprivileged tester,
writes its log and exit status alongside it, and retains the RAM-backed source
and build directories between jobs. Create `queue/stop` to finish the session
and power off. Exit markers are per-job, so an old successful run cannot be
mistaken for a newer pending job. Do not start a second writer for the same
queue or build directory.

## Bootstrap files in this repository

`create-initramfs.py --lab LAB_DIR` builds the small initramfs from extracted
Debian packages under `LAB_DIR/root`. It discovers the kernel modules, includes
only virtio-9p dependencies, supplies a generic font configuration, and creates
a private guest environment. It does not run the application or any tests.

`run.sh LAB_DIR SOURCE_DIR OUTPUT_DIR` is the host launcher. Set
`SYSPULSE_BUILDDEPS` to the extracted CMake/Qt development sysroot and put a guest
job script in `OUTPUT_DIR/job.sh`. An optional fourth argument selects a kernel
image explicitly. The launcher neither installs packages on the host nor
modifies the source. Keep all package archives, extracted files, initramfs, and
output outside the repository. Paths containing commas are unsupported by the
underlying QEMU `-virtfs` option and should be avoided.

The lab needs extracted `qemu-system-x86`, `qemu-system-common`,
`qemu-system-data`, `seabios`, `busybox-static`, a matching `linux-image` package,
and QEMU's runtime library dependencies. Obtain them through the distribution's
package manager and preserve package versions and checksums with the validation
evidence. The host `/usr` and development sysroot must be ABI-compatible Debian
userspace. This is a validation harness, not a portable end-user runtime.

If package ownership metadata is available in a prepared guest, use it only to
resolve redistributable runtime license notices. This minimal guest does not
provide a package database; packaging must explicitly resolve and verify notices
from the available files or report missing provenance. Do not export a complete
package database or machine inventory in artifacts.

## Optional device fixture

`run-device-fixture.sh LAB_DIR SOURCE_DIR OUTPUT_DIR` launches a separate guest
with a **fresh 64 MiB disposable raw virtual disk** (QEMU snapshot writes) and
a disconnected virtio network interface. It never accepts a host block-device
path. The disconnected NIC has no backend or network connectivity, so it can
validate interface discovery and idle counters without contacting anything.
The generated initramfs includes matching virtio block/network modules.

This fixture permits real `/proc/diskstats` and non-loopback interface discovery.
It does not validate physical storage health, Wi-Fi, real network throughput,
full XFCE sessions, or user-service authorization. Keep its output and the
sparse fixture disk outside the repository and out of release bundles.

The execution image may expose stock system files as UID65534 rather than root.
The guest uses an ephemeral overlay to normalize protected system-bin ownership
and directory metadata to UID0 for strict command-resolution tests. Only the
guest overlay is changed; the host lower remains read-only. Production ownership
guards are not relaxed, and the tests do not execute privileged mutations.

## Offline publication verification

`tools/verify-release-assets.py` compares saved GitHub release and tag-ref JSON
with an exact local folder containing the prepared public assets. Obtain the
metadata through an authorized read-only connection after publication. The
helper performs no network access, upload, workflow dispatch or app launch.
Use the known verified source SHA, not a SHA inferred from the release being
checked. Its tag JSON must retain `ref` and an `object` resolved to a commit;
annotated tags need external dereferencing first.

Inside the isolated VM:

```sh
python3 tests/test_release_assets.py
python3 tools/verify-release-assets.py \
  --assets /out/release-publication/assets \
  --release-json /out/release-publication/release.json \
  --tag-json /out/release-publication/tag.json \
  --repository sudipdasin/tux-steward --tag v3.0.0-alpha.1 \
  --source-commit f0fe320677e40c05e0c176a2fd3efdbe961d9dcd
```

The helper checks publication/maturity, repository/tag/commit identity, the
exact set of uploaded filenames, byte sizes, SHA-256 digests, upload state and
canonical checksum contents. It refuses duplicate names, path escapes and
symlinked inputs. In particular, canonical checksums must name GitHub's actual
download filenames after normalization. Build-name checksums may be retained as
separate metadata; they are not the canonical download checksum list. These
publication checks supplement application/installation verification.
