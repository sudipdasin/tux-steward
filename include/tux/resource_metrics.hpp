// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>
namespace tux {
struct CpuTicks {quint64 total=0,idle=0;};
inline QMap<QString,CpuTicks> cpuTicks(const QString& text) {
    QMap<QString,CpuTicks> out;
    for(const auto& line:text.split('\n')) {
        const auto f=line.simplified().split(' '); const auto id=f.value(0);
        if(id!="cpu" && (!id.startsWith("cpu") || id.mid(3).isEmpty() || !id.mid(3).at(0).isDigit())) continue;
        if(f.size()<5) continue; CpuTicks t; bool valid=true;
        // guest/guest_nice are already counted in user/nice; use only first eight counters.
        for(int i=1;i<qMin(9,f.size());++i) {bool ok=false;auto n=f[i].toULongLong(&ok);valid=valid&&ok;t.total+=n;if(i==4 || i==5)t.idle+=n;}
        if(valid) out.insert(id,t);
    } return out;
}
inline std::optional<double> cpuUsage(CpuTicks before,CpuTicks now) {
    if(now.total<=before.total || now.idle<before.idle) return {};
    auto total=now.total-before.total,idle=now.idle-before.idle;if(idle>total)return {};
    return 100.0*double(total-idle)/double(total);
}
inline std::optional<double> counterRate(quint64 before,quint64 now,double seconds) {
    if(seconds<=0 || now<before) return {}; return double(now-before)/seconds;
}
struct IoCounters {quint64 read=0,write=0,busyMs=0;};
inline QMap<QString,IoCounters> diskCounters(const QString& text) {
    QMap<QString,IoCounters> out;
    for(const auto& line:text.split('\n')) {auto f=line.simplified().split(' ');if(f.size()<14)continue;
        bool a=false,b=false,c=false;IoCounters n{f[5].toULongLong(&a),f[9].toULongLong(&b),f[12].toULongLong(&c)};
        if(a&&b&&c)out.insert(f[2],n);
    }return out;
}
inline QMap<QString,IoCounters> networkCounters(const QString& text) {
    QMap<QString,IoCounters> out;
    for(const auto& line:text.split('\n')) {int colon=line.indexOf(':');if(colon<0)continue;auto f=line.mid(colon+1).simplified().split(' ');if(f.size()<16)continue;
        bool a=false,b=false;IoCounters n{f[0].toULongLong(&a),f[8].toULongLong(&b),0};if(a&&b)out.insert(line.left(colon).trimmed(),n);
    }return out;
}
inline QMap<QString,quint64> memoryCounters(const QString& text) {
    QMap<QString,quint64> out;for(const auto& line:text.split('\n')) {auto f=line.simplified().split(' ');bool ok=false;auto n=f.value(1).toULongLong(&ok);if(ok)out.insert(f[0].section(':',0,0),n);}return out;
}
}
