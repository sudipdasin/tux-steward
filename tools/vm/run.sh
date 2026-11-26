#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Host launcher for the isolated guest. No application code runs on the host.
set -eu
if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo 'Usage: run.sh LAB_DIR SOURCE_DIR OUTPUT_DIR [KERNEL_IMAGE]' >&2
    exit 2
fi
lab=$(CDPATH= cd -- "$1" && pwd)
project=$(CDPATH= cd -- "$2" && pwd)
output=$(CDPATH= cd -- "$3" && pwd)
kernel=${4:-}
if [ -z "$kernel" ]; then
    for candidate in "$lab"/root/boot/vmlinuz-*; do
        kernel=$candidate
        break
    done
fi
: "${SYSPULSE_BUILDDEPS:?Set SYSPULSE_BUILDDEPS to the extracted CMake/Qt sysroot.}"
deps=$(CDPATH= cd -- "$SYSPULSE_BUILDDEPS" && pwd)
[ -f "$output/job.sh" ] || { echo 'OUTPUT_DIR must contain job.sh' >&2; exit 2; }
[ -f "$kernel" ] || { echo 'Kernel image not found' >&2; exit 2; }
rm -f "$output/job.exit"
export LD_LIBRARY_PATH="$lab/root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$lab/root/usr/bin/qemu-system-x86_64" \
    -machine q35 -accel tcg,thread=multi -cpu max -smp 4 -m 3072 \
    -nodefaults -no-user-config -display none -serial stdio -monitor none -no-reboot \
    -bios "$lab/root/usr/share/seabios/bios-256k.bin" \
    -L "$lab/root/usr/share/qemu" -kernel "$kernel" \
    -initrd "$lab/initramfs.cpio.gz" \
    -append 'console=ttyS0 quiet loglevel=3 panic=1 init=/init syspulse.validation=1' \
    -virtfs 'local,path=/usr,mount_tag=usr,security_model=none,readonly=on' \
    -virtfs "local,path=$deps,mount_tag=deps,security_model=none,readonly=on" \
    -virtfs "local,path=$project,mount_tag=source,security_model=none,readonly=on" \
    -virtfs "local,path=$output,mount_tag=output,security_model=none"
