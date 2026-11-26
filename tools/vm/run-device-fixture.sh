#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Optional runtime fixture; creates only a NEW disposable virtual disk file.
set -eu
if [ "$#" -ne 3 ]; then
    echo 'Usage: run-device-fixture.sh LAB_DIR SOURCE_DIR OUTPUT_DIR' >&2
    exit 2
fi
lab=$(CDPATH= cd -- "$1" && pwd)
project=$(CDPATH= cd -- "$2" && pwd)
output=$(CDPATH= cd -- "$3" && pwd)
: "${SYSPULSE_BUILDDEPS:?Set SYSPULSE_BUILDDEPS to the extracted CMake/Qt sysroot.}"
deps=$(CDPATH= cd -- "$SYSPULSE_BUILDDEPS" && pwd)
[ -f "$output/job.sh" ] || { echo 'OUTPUT_DIR must contain job.sh' >&2; exit 2; }
disk=$(mktemp "$output/fixture-disk.XXXXXX")
truncate -s 64M "$disk"
for kernel in "$lab"/root/boot/vmlinuz-*; do break; done
rm -f "$output/job.exit"
export LD_LIBRARY_PATH="$lab/root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# No host block device is accepted; the only disk is the freshly created file.
# snapshot=on keeps guest writes in QEMU's temporary copy-on-write layer.
# The NIC has no backend, no connectivity, and cannot contact host or internet.
exec "$lab/root/usr/bin/qemu-system-x86_64" \
    -machine q35 -accel tcg,thread=multi -cpu max -smp 4 -m 3072 \
    -nodefaults -no-user-config -display none -serial stdio -monitor none -no-reboot \
    -bios "$lab/root/usr/share/seabios/bios-256k.bin" \
    -L "$lab/root/usr/share/qemu" -kernel "$kernel" \
    -initrd "$lab/initramfs.cpio.gz" \
    -append 'console=ttyS0 quiet loglevel=3 panic=1 init=/init syspulse.validation=1' \
    -drive "if=none,id=fixture-disk,file=$disk,format=raw,snapshot=on" \
    -device virtio-blk-pci,drive=fixture-disk \
    -device virtio-net-pci \
    -virtfs 'local,path=/usr,mount_tag=usr,security_model=none,readonly=on' \
    -virtfs "local,path=$deps,mount_tag=deps,security_model=none,readonly=on" \
    -virtfs "local,path=$project,mount_tag=source,security_model=none,readonly=on" \
    -virtfs "local,path=$output,mount_tag=output,security_model=none"
