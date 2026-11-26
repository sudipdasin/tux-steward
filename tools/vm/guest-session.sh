#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Run only as the unprivileged user inside the validation VM.
set -eu
if [ ! -r /proc/cmdline ] || ! grep -q 'syspulse.validation=1' /proc/cmdline; then
    echo 'Refusing to start outside the SysPulse validation guest.' >&2
    exit 2
fi
mkdir -p /out/queue
printf '%s\n' "$(uname -r)" > /out/queue/guest.ready
sequence=1
while [ ! -e /out/queue/stop ]; do
    job=/out/queue/$sequence.sh
    if [ ! -f "$job" ]; then
        sleep 1
        continue
    fi
    printf 'Running VM job %s\n' "$sequence"
    status=0
    sh "$job" > /out/queue/$sequence.log 2>&1 || status=$?
    printf '%s\n' "$status" > /out/queue/$sequence.exit.tmp
    mv /out/queue/$sequence.exit.tmp /out/queue/$sequence.exit
    printf 'VM job %s exited %s\n' "$sequence" "$status"
    sequence=$((sequence+1))
done
