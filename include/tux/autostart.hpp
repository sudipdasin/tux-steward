// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace tux::autostart {
struct Paths {
    QString configHome;
    QStringList configDirs;
    QStringList desktops;
    static Paths environment();
};
struct Entry {
    QString id, name, command, path, reason;
    bool valid=false, hidden=false, eligible=false;
    bool userOverride=false, hasSystemDefault=false;
    QByteArray contents;
};
struct Change { bool ok=false; QString message; };
QVector<Entry> discover(const Paths& paths);
Change setEnabled(const Paths& paths,const QString& id,bool enabled);
Change undo(const Paths& paths,const QString& id);
bool hasUndo(const Paths& paths,const QString& id);
}
