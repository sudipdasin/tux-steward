// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/setup.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
namespace tux::setup {
namespace {
const QStringList files={"bin/tux-steward","bin/tux-task-manager","bin/tux-setup","share/doc/tux-steward/LICENSE","share/doc/tux-steward/PRIVACY.md"};
const QStringList menus={"tux-steward-user.desktop","tux-task-manager-user.desktop","tux-setup-user.desktop"};
bool safePath(const QString &path) {
    if (path!=QDir::cleanPath(path) || !QDir::isAbsolutePath(path) || path.contains('\n') || path.contains('\r') || path.contains('\t') || path.contains('=')) return false;
    QString part="/";
    for (const auto &component:QDir::cleanPath(path).split('/',Qt::SkipEmptyParts)) {
        part=QDir(part).filePath(component);
        if (QFileInfo(part).isSymLink()) return false;
    }
    return true;
}
QByteArray digest(const QString &path) {
    QFile f(path); const QFileInfo info(path);
    if (!safePath(path) || !info.isFile() || info.size()>64*1024*1024 || !f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return hash.result().toHex();
}
bool write(const QString &path,const QByteArray &data) {
    QSaveFile out(path); out.setDirectWriteFallback(false);
    return out.open(QIODevice::WriteOnly) && out.write(data)==data.size() && out.commit();
}
QString quoted(QString path) {
    path.replace("\\","\\\\"); path.replace("\"","\\\"");
    path.replace("`","\\`"); path.replace("$","\\$"); path.replace("%","%%");
    // Desktop string escaping is decoded before Exec argument unquoting.
    // https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html
    path.replace("\\","\\\\");
    return '"'+path+'"';
}
QString menuPath(const QString &home,const QString &name) { return QDir(home).filePath("applications/"+name); }
}
QString installRoot(const QString &home) { return QDir(home).filePath("tux-steward/user-install"); }
Outcome install(const QString &source,const QString &home) {
    const QString root=installRoot(home);
    if (!safePath(home) || !safePath(root) || !safePath(source)) return {false,"Installation paths must be normalized absolute paths without symbolic links, control characters or equals signs."};
    if (QFileInfo::exists(root)) return {false,"A user installation already exists. Remove it first; existing files will not be overwritten."};
    for (const auto &name:menus)
        if (!safePath(menuPath(home,name)) || QFileInfo::exists(menuPath(home,name))) return {false,"A user menu entry already exists or is unsafe. Nothing was overwritten."};
    QJsonObject hashes;
    for (const auto &name:files) {
        const auto path=QDir(source).filePath(name); const auto hash=digest(path);
        if (hash.isEmpty()) return {false,"The archive is incomplete, unreadable or contains unsafe files."};
        if (name.startsWith("bin/")) {
            QFile binary(path);
            if (!QFileInfo(path).isExecutable() || !binary.open(QIODevice::ReadOnly) || binary.read(4)!=QByteArray("\x7f" "ELF",4))
                return {false,"The archive must contain executable native Linux programs."};
        }
        hashes.insert(name,QString::fromLatin1(hash));
    }
    const QString parent=QFileInfo(root).absolutePath();
    if (!QDir().mkpath(parent) || !QDir().mkpath(QDir(home).filePath("applications"))) return {false,"Cannot create user installation directories."};
    QTemporaryDir staging(parent+"/.setup-XXXXXX");
    if (!staging.isValid()) return {false,"Cannot prepare the user installation."};
    for (const auto &name:files) {
        const QString destination=QDir(staging.path()).filePath(name);
        if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(QDir(source).filePath(name),destination)
            || digest(destination)!=hashes.value(name).toString().toLatin1()) return {false,"Could not stage all archive files. No installation was activated."};
    }
    QJsonObject menuHashes; QList<QByteArray> entries;
    const QStringList names={"Tux Steward (user install)","Tux Task Manager (user install)","Tux Steward Setup"};
    for (int i=0;i<menus.size();++i) {
        const auto binary=files[i];
        const auto data=(QString("[Desktop Entry]\nType=Application\nName=%1\nExec=%2\nTerminal=false\nCategories=System;Utility;\n").arg(names[i],quoted(QDir(root).filePath(binary)))).toUtf8();
        entries.append(data); menuHashes.insert(menus[i],QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()));
    }
    QJsonObject manifest{{"schema",1},{"files",hashes},{"menus",menuHashes}};
    if (!write(QDir(staging.path()).filePath("install.json"),QJsonDocument(manifest).toJson())) return {false,"Cannot write the installation record."};
    if (!QDir().rename(staging.path(),root)) return {false,"Cannot activate the user installation; existing files were not overwritten."};
    staging.setAutoRemove(false);
    for (int i=0;i<menus.size();++i) {
        if (!write(menuPath(home,menus[i]),entries[i])) {
            // Removal uses the private record and deletes only unchanged known files.
            const auto rollback=remove(home);
            return {false,rollback.ok?"Menu creation failed. The new installation was rolled back.":"Menu creation failed; some installation files remain. Use Remove to retry cleanup. Your configuration was not removed."};
        }
    }
    return {true,"Installed for your account. Open the applications from your desktop menu. A menu refresh or sign-in may be needed."};
}
Outcome remove(const QString &home) {
    const QString root=installRoot(home),record=QDir(root).filePath("install.json");
    if (!safePath(home) || !safePath(root) || !safePath(record)) return {false,"Unsafe installation path; nothing was removed."};
    QFile f(record);
    if (f.size()>65536 || !f.open(QIODevice::ReadOnly)) return {false,"No readable user installation record was found. System packages are managed by your package manager."};
    const auto doc=QJsonDocument::fromJson(f.read(65537)); f.close();
    const auto object=doc.object(),hashes=object.value("files").toObject(),menuHashes=object.value("menus").toObject();
    if (object.value("schema").toInt()!=1 || hashes.size()!=files.size() || menuHashes.size()!=menus.size()) return {false,"Invalid installation record; nothing was removed."};
    QStringList paths;
    for (const auto &name:files) {
        const QString path=QDir(root).filePath(name),expected=hashes.value(name).toString();
        if (expected.size()!=64 || !safePath(path) || (QFileInfo::exists(path) && digest(path)!=expected.toLatin1())) return {false,"An installed file was changed or is unsafe. Nothing was removed. Preserve your changes before retrying."};
        paths.append(path);
    }
    for (const auto &name:menus) {
        const QString path=menuPath(home,name),expected=menuHashes.value(name).toString();
        if (expected.size()!=64 || !safePath(path) || (QFileInfo::exists(path) && digest(path)!=expected.toLatin1())) return {false,"A user menu entry was changed or is unsafe. Nothing was removed."};
        paths.append(path);
    }
    for (const auto &path:paths)
        if (QFileInfo::exists(path) && !QFile::remove(path)) return {false,"Some installation files could not be removed. The record is retained for retry. User configuration is untouched."};
    if (!QFile::remove(record)) return {false,"Application files were removed; the installation record could not be removed. Retry cleanup."};
    for (const auto &dir:QStringList{"share/doc/tux-steward","share/doc","share","bin",""}) QDir().rmdir(QDir(root).filePath(dir));
    return {true,"User-installed programs and menu entries removed. User configuration, undo records and unrelated files were retained."};
}
}
