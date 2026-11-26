// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <cmath>
#include <limits>
#include <optional>

namespace syspulse::metrics {

struct CpuTicks { quint64 total = 0, idle = 0; };
struct DiskCounters {
    quint64 readSectors = 0, writeSectors = 0, busyMs = 0;
    quint64 readOperations = 0, writeOperations = 0;
};
struct NetworkCounters { quint64 received = 0, transmitted = 0; };
struct Memory {
    quint64 totalKiB = 0, availableKiB = 0, cachedKiB = 0, buffersKiB = 0;
    quint64 reclaimableKiB = 0, swapTotalKiB = 0, swapUsedKiB = 0;
    bool swapKnown = false;
    quint64 usedKiB() const { return totalKiB - availableKiB; }
};

inline std::optional<quint64> unsignedNumber(const QString& text)
{
    if (text.isEmpty()) return {};
    for (const auto character : text)
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) return {};
    bool ok = false;
    const auto value = text.toULongLong(&ok);
    return ok ? std::optional<quint64>(value) : std::nullopt;
}

inline bool isCpuId(const QString& id)
{
    if (id == QStringLiteral("cpu")) return true;
    return id.startsWith(QStringLiteral("cpu")) && unsignedNumber(id.mid(3)).has_value();
}

inline QMap<QString, CpuTicks> parseCpu(const QString& text)
{
    QMap<QString, CpuTicks> result;
    for (const auto& line : text.split(QLatin1Char('\n'))) {
        const auto fields = line.simplified().split(QLatin1Char(' '));
        if (fields.size() < 5 || !isCpuId(fields[0])) continue;
        CpuTicks ticks;
        bool valid = true;
        // Linux includes guest/guest_nice in user/nice already. Count only
        // user, nice, system, idle, iowait, irq, softirq, steal.
        for (int i = 1; i < qMin(9, fields.size()); ++i) {
            const auto value = unsignedNumber(fields[i]);
            if (!value || ticks.total > std::numeric_limits<quint64>::max() - *value) {
                valid = false;
                break;
            }
            ticks.total += *value;
            if (i == 4 || i == 5) ticks.idle += *value;
        }
        if (valid) result.insert(fields[0], ticks);
    }
    return result;
}

inline std::optional<double> cpuPercent(CpuTicks before, CpuTicks after)
{
    if (before.idle > before.total || after.idle > after.total ||
        after.total <= before.total || after.idle < before.idle) return {};
    const auto elapsed = after.total - before.total;
    const auto idle = after.idle - before.idle;
    if (idle > elapsed) return {};
    return 100.0 * static_cast<double>(elapsed - idle) / static_cast<double>(elapsed);
}

inline std::optional<double> rate(quint64 before, quint64 after, double seconds)
{
    if (!(seconds > 0.0) || !std::isfinite(seconds) || after < before) return {};
    const auto value = static_cast<double>(after - before) / seconds;
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
}

inline QMap<QString, DiskCounters> parseDisks(const QString& text)
{
    QMap<QString, DiskCounters> result;
    for (const auto& line : text.split(QLatin1Char('\n'))) {
        const auto fields = line.simplified().split(QLatin1Char(' '));
        if (fields.size() < 14) continue;
        const auto major = unsignedNumber(fields[0]);
        const auto minor = unsignedNumber(fields[1]);
        const auto reads = unsignedNumber(fields[3]);
        const auto readSectors = unsignedNumber(fields[5]);
        const auto writes = unsignedNumber(fields[7]);
        const auto writeSectors = unsignedNumber(fields[9]);
        const auto busy = unsignedNumber(fields[12]);
        if (!major || !minor || !reads || !readSectors || !writes || !writeSectors || !busy) continue;
        const auto& name = fields[2];
        if (name.isEmpty() || name.contains(QLatin1Char('/'))) continue;
        result.insert(name, {*readSectors, *writeSectors, *busy, *reads, *writes});
    }
    return result;
}

inline QMap<QString, NetworkCounters> parseNetwork(const QString& text)
{
    QMap<QString, NetworkCounters> result;
    for (const auto& line : text.split(QLatin1Char('\n'))) {
        const auto colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        const auto name = line.left(colon).trimmed();
        const auto fields = line.mid(colon + 1).simplified().split(QLatin1Char(' '));
        if (name.isEmpty() || name.contains(QLatin1Char('/')) || fields.size() < 16) continue;
        const auto received = unsignedNumber(fields[0]);
        const auto transmitted = unsignedNumber(fields[8]);
        if (received && transmitted) result.insert(name, {*received, *transmitted});
    }
    return result;
}

inline std::optional<Memory> parseMemory(const QString& text)
{
    QMap<QString, quint64> fields;
    for (const auto& line : text.split(QLatin1Char('\n'))) {
        const auto values = line.simplified().split(QLatin1Char(' '));
        if (values.size() != 3 || values[2] != QStringLiteral("kB")) continue;
        const auto value = unsignedNumber(values[1]);
        if (value && values[0].endsWith(QLatin1Char(':')))
            fields.insert(values[0].chopped(1), *value);
    }
    if (!fields.contains(QStringLiteral("MemTotal")) || !fields.contains(QStringLiteral("MemAvailable"))) return {};
    Memory memory;
    memory.totalKiB = fields.value(QStringLiteral("MemTotal"));
    memory.availableKiB = fields.value(QStringLiteral("MemAvailable"));
    if (!memory.totalKiB || memory.availableKiB > memory.totalKiB) return {};
    memory.cachedKiB = fields.value(QStringLiteral("Cached"));
    memory.buffersKiB = fields.value(QStringLiteral("Buffers"));
    memory.reclaimableKiB = fields.value(QStringLiteral("SReclaimable"));
    if (fields.contains(QStringLiteral("SwapTotal"))) {
        memory.swapTotalKiB = fields.value(QStringLiteral("SwapTotal"));
        memory.swapKnown = memory.swapTotalKiB == 0 || fields.contains(QStringLiteral("SwapFree"));
        const auto free = fields.value(QStringLiteral("SwapFree"));
        if (free > memory.swapTotalKiB) memory.swapKnown = false;
        if (memory.swapKnown) memory.swapUsedKiB = memory.swapTotalKiB - free;
    }
    return memory;
}

// /proc/diskstats sectors are always 512 bytes, independent of a drive's
// reported physical or logical sector size. Only aggregate a non-overlapping
// set of devices selected by the caller; never add both a disk and its partition.
inline double sectorsPerSecondToMiB(double value) { return value / 2048.0; }
inline double bytesPerSecondToKiB(double value) { return value / 1024.0; }

} // namespace syspulse::metrics
