// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/metrics.hpp"

#include <QCoreApplication>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
int checks = 0;
void expect(bool condition, const char* description)
{
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}
bool near(double first, double second) { return std::abs(first - second) < 0.00001; }
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    using namespace syspulse::metrics;

    expect(unsignedNumber(QStringLiteral("0")) == quint64(0), "zero is a valid cumulative counter");
    expect(unsignedNumber(QStringLiteral("18446744073709551615")) == std::numeric_limits<quint64>::max(), "largest counter is preserved");
    expect(!unsignedNumber(QStringLiteral("18446744073709551616")), "counter overflow is rejected");
    expect(!unsignedNumber(QStringLiteral("-1")) && !unsignedNumber(QStringLiteral("+1")), "signed counters are rejected");
    expect(!unsignedNumber(QStringLiteral("")) && !unsignedNumber(QStringLiteral("1.2")), "empty and fractional counters are rejected");

    const auto cpu = parseCpu(QStringLiteral(
        "cpu 10 0 10 70 10 0 0 0 999 999\n"
        "cpu0 5 0 5 35 5 0 0 0\n"
        "cpu10 5 0 5 35 5 0 0 0\n"
        "cpu1oops 1 0 0 1\n"
        "cpux 1 0 0 1\n"
        "cpu3 -1 0 0 1\n"
        "intr 4 5 6 7\n"));
    expect(cpu.size() == 3, "only complete numeric CPU identities are accepted");
    expect(cpu.value(QStringLiteral("cpu")).total == 100, "guest and guest_nice are not double-counted");
    expect(cpu.value(QStringLiteral("cpu")).idle == 80, "CPU idle includes iowait");
    const auto usage = cpuPercent({100, 80}, {200, 150});
    expect(usage && near(*usage, 30.0), "CPU usage uses the cumulative busy delta");
    expect(!cpuPercent({100, 80}, {99, 80}), "CPU total rollback produces a gap");
    expect(!cpuPercent({100, 80}, {101, 79}), "CPU idle rollback produces a gap");
    expect(!cpuPercent({100, 80}, {100, 80}), "CPU zero elapsed ticks are not reported as zero usage");
    expect(!cpuPercent({100, 80}, {110, 100}), "CPU impossible idle delta is rejected");
    expect(!cpuPercent({100, 120}, {200, 130}) && !cpuPercent({100, 80}, {200, 210}),
           "CPU snapshots with idle greater than total are invalid");
    const auto overflowCpu = parseCpu(QStringLiteral("cpu 18446744073709551615 1 0 1\n"));
    expect(overflowCpu.isEmpty(), "CPU counter sum overflow is rejected");

    const auto validRate = rate(100, 600, 2.5);
    expect(validRate && near(*validRate, 200), "rate uses actual elapsed seconds");
    const auto idleRate = rate(100, 100, 2.5);
    expect(idleRate && *idleRate == 0, "unchanged valid counters report an actual zero");
    expect(!rate(100, 99, 1), "counter resets do not produce negative traffic");
    expect(!rate(1, 2, 0) && !rate(1, 2, -1), "non-positive sample intervals are invalid");
    expect(!rate(1, 2, std::numeric_limits<double>::infinity()), "infinite sample intervals are invalid");
    expect(!rate(1, 2, std::numeric_limits<double>::quiet_NaN()), "NaN sample intervals are invalid");
    expect(!rate(0, std::numeric_limits<quint64>::max(), std::numeric_limits<double>::denorm_min()), "non-finite calculated rates are invalid");

    const auto disks = parseDisks(QStringLiteral(
        "8 0 sda 3 0 2048 0 7 0 4096 0 0 500 0\n"
        "8 1 sda1 2 0 1024 0 3 0 2048 0 0 250 0\n"
        "8 2 bad 1 0 -2 0 1 0 5 0 0 5 0\n"
        "8 3 truncated 1 2\n"));
    expect(disks.size() == 2, "malformed or truncated diskstats lines are excluded");
    const auto disk = disks.value(QStringLiteral("sda"));
    expect(disk.readSectors == 2048 && disk.writeSectors == 4096 && disk.busyMs == 500,
           "diskstats read/write/busy columns are mapped correctly");
    expect(disk.readOperations == 3 && disk.writeOperations == 7, "diskstats I/O operation columns are mapped correctly");
    expect(near(sectorsPerSecondToMiB(2048), 1.0), "512-byte diskstats sectors convert to MiB independent of device sector size");
    expect(near(sectorsPerSecondToMiB(4096), 2.0), "write throughput uses the same sector conversion");

    const auto network = parseNetwork(QStringLiteral(
        "Inter-| Receive | Transmit\n"
        "eth0: 2048 0 0 0 0 0 0 0 4096 0 0 0 0 0 0 0\n"
        "lo: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
        "invalid: 2 3 4\n"
        "broken: -1 0 0 0 0 0 0 0 1 0 0 0 0 0 0 0\n"));
    expect(network.size() == 2, "network headers and malformed lines are rejected");
    expect(network.value(QStringLiteral("eth0")).received == 2048 && network.value(QStringLiteral("eth0")).transmitted == 4096,
           "network RX and TX use the correct independent counters");
    expect(near(bytesPerSecondToKiB(2048), 2.0), "network uses binary KiB/s with matching unit label");

    const auto memory = parseMemory(QStringLiteral(
        "MemTotal: 8388608 kB\nMemAvailable: 5033165 kB\nCached: 1048576 kB\n"
        "Buffers: 1024 kB\nSReclaimable: 2048 kB\nSwapTotal: 2097152 kB\nSwapFree: 1048576 kB\n"));
    expect(memory.has_value(), "complete memory snapshot parses");
    expect(memory && memory->usedKiB() == 3355443, "used memory subtracts available rather than free memory");
    expect(memory && memory->cachedKiB == 1048576 && memory->reclaimableKiB == 2048, "cache and reclaimable slab remain distinct");
    expect(memory && memory->swapKnown && memory->swapUsedKiB == 1048576, "swap used subtracts free from total");
    expect(!parseMemory(QStringLiteral("MemTotal: 10 kB\n")), "missing MemAvailable does not become a fabricated full-memory reading");
    expect(!parseMemory(QStringLiteral("MemTotal: 10 kB\nMemAvailable: 11 kB\n")), "available memory cannot exceed total");
    expect(!parseMemory(QStringLiteral("MemTotal: 0 kB\nMemAvailable: 0 kB\n")), "zero total memory is unavailable");
    expect(!parseMemory(QStringLiteral("MemTotal: 10 MB\nMemAvailable: 1 MB\n")), "unexpected memory units are rejected");
    const auto noSwap = parseMemory(QStringLiteral("MemTotal: 10 kB\nMemAvailable: 1 kB\nSwapTotal: 0 kB\n"));
    expect(noSwap && noSwap->swapKnown && noSwap->swapUsedKiB == 0, "no swap is a valid state distinct from unavailable");
    const auto badSwap = parseMemory(QStringLiteral("MemTotal: 10 kB\nMemAvailable: 1 kB\nSwapTotal: 1 kB\nSwapFree: 2 kB\n"));
    expect(badSwap && !badSwap->swapKnown, "invalid swap counters do not underflow");

    std::cout << checks << " SysPulse metric checks, " << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
