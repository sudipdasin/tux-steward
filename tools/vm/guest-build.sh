#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Execute INSIDE the isolated guest, never directly on the host.
set -eu
if [ ! -r /proc/cmdline ] || ! grep -q 'syspulse.validation=1' /proc/cmdline; then
    echo 'Refusing to build outside the SysPulse validation guest.' >&2
    exit 2
fi
build_dir=${SYSPULSE_BUILD_DIR:-/out/build}
build_jobs=${SYSPULSE_BUILD_JOBS:-4}
mkdir -p "$build_dir" /out/logs
{
    uname -a
    id
    cmake --version
    g++ --version
} > /out/logs/environment.log 2>&1
cmake -S /src -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/builddeps/usr -DBUILD_TESTING=ON \
    > /out/logs/configure.log 2>&1 || {
        cat /out/logs/configure.log
        echo 1 > /out/build.exit
        exit 1
    }
cmake --build "$build_dir" -j "$build_jobs" > /out/logs/build.log 2>&1 || {
    tail -100 /out/logs/build.log
    echo 1 > /out/build.exit
    exit 1
}
export QT_QPA_PLATFORM=offscreen
ctest --test-dir "$build_dir" --output-on-failure \
    > /out/logs/ctest.log 2>&1 || {
        cat /out/logs/ctest.log
        echo 1 > /out/build.exit
        exit 1
    }
cat /out/logs/ctest.log
echo 0 > /out/build.exit
