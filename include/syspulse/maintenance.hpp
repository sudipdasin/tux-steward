// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDateTime>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>
namespace syspulse::maintenance {
struct File {
    QString directory, name;
    quint64 device=0, inode=0, size=0, parentInode=0;
    qint64 modified=0, modifiedNanos=0, changed=0, changedNanos=0;
};
struct Plan {
    QString root; QStringList categories; QVector<File> files;
    quint64 device=0, inode=0, bytes=0; qint64 created=0;
    int ageDays=30; bool limited=false;
};
struct Outcome { QString run, message; int changed=0, skipped=0; quint64 bytes=0; };
struct Recovery { QString id, created, problem; int files=0; quint64 bytes=0; };
QString cacheRoot();
QStringList categories();
Plan scan(const QString& root,const QStringList& selected,int ageDays,const std::atomic_bool* cancel=nullptr);
Outcome stage(const Plan& plan,const std::function<void()>& beforeMove={});
QVector<Recovery> recoveries(const QString& root);
Outcome restore(const QString& root,const QString& id);
Outcome purge(const QString& root,const QString& id);
}
