// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
namespace tux {
// Privileged fixed-catalog actions never resolve executables from user PATH.
// Reject writable files/directories and symlinks resolving outside system bins.
inline QString trustedSystemExecutable(const QString& name) {
    static const QRegularExpression safe(QStringLiteral("\\A[A-Za-z0-9][A-Za-z0-9._+-]*\\z"));
    if(!safe.match(name).hasMatch())return {};
    const QStringList directories={"/usr/bin","/usr/sbin","/bin","/sbin"};
    auto protectedFile=[](const QFileInfo& info) {
        return info.exists()&&info.ownerId()==0&&!(info.permissions()&(QFileDevice::WriteGroup|QFileDevice::WriteOther));
    };
    for(const auto& directory:directories) {
        QFileInfo input(QDir(directory).filePath(name));const auto canonical=input.canonicalFilePath();
        if(canonical.isEmpty())continue;
        bool allowed=false;for(const auto& bin:directories)if(canonical.startsWith(bin+"/"))allowed=true;
        QFileInfo file(canonical);if(!allowed||!file.isFile()||!file.isExecutable()||!protectedFile(file))continue;
        auto parent=file.dir();bool trusted=true;
        while(true) {
            if(!protectedFile(QFileInfo(parent.absolutePath()))){trusted=false;break;}
            if(parent.isRoot()||!parent.cdUp())break;
        }
        if(trusted)return canonical;
    }
    return {};
}
}
