// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/autostart.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTextCodec>
#include <algorithm>

namespace tux::autostart {
namespace {
constexpr int FileLimit=256*1024;
constexpr int JournalLimit=768*1024;
constexpr auto PrivateFile=QFileDevice::ReadOwner|QFileDevice::WriteOwner;
constexpr auto PrivateDir=QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner;
QString root(const Paths& paths) { return QDir(paths.configHome).filePath("autostart"); }
bool validId(const QString& id) {
    if (id.isEmpty() || !id.endsWith(".desktop") || QFileInfo(id).fileName()!=id
        || id.contains('/') || id.contains('\\')) return false;
    for (const auto character:id) if (character.isNull() || character.unicode()<32) return false;
    return true;
}
QString hash(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
QString journalRoot(const Paths& paths) { return QDir(paths.configHome).filePath("tux-steward/autostart-undo"); }
QString journalPath(const Paths& paths,const QString& id) {
    return QDir(journalRoot(paths)).filePath(hash(id.toUtf8())+".json");
}
bool readBounded(const QString& path,QByteArray& bytes,int limit=FileLimit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    bytes=file.read(limit+1);
    return bytes.size()<=limit && file.atEnd();
}
bool atomicWrite(const QString& path,const QByteArray& bytes,QFileDevice::Permissions permissions=PrivateFile) {
    if (QFileInfo(path).isSymLink()) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (!file.setPermissions(permissions) || file.write(bytes)!=bytes.size()) return false;
    return file.commit();
}
bool secureJournalDirectory(const Paths& paths) {
    const auto parent=QFileInfo(journalRoot(paths)).absolutePath();
    if (QFileInfo(parent).isSymLink() || QFileInfo(journalRoot(paths)).isSymLink()) return false;
    if (!QDir().mkpath(journalRoot(paths))) return false;
    return QFile::setPermissions(parent,PrivateDir) && QFile::setPermissions(journalRoot(paths),PrivateDir);
}
QString decodeValue(QString value) {
    QString out;
    for (int i=0;i<value.size();++i) {
        if (value[i]!='\\' || i+1==value.size()) { out+=value[i]; continue; }
        const auto next=value[++i];
        if (next=='s') out+=' '; else if (next=='n') out+='\n';
        else if (next=='t') out+='\t'; else if (next=='r') out+='\r'; else out+=next;
    }
    return out;
}
Entry parse(const Paths& paths,const QString& id,const QString& path,bool user,bool system) {
    Entry entry; entry.id=id; entry.name=id; entry.path=path;
    entry.userOverride=user; entry.hasSystemDefault=system;
    if (!readBounded(path,entry.contents)) { entry.reason="Unreadable or larger than 256 KiB"; return entry; }
    QTextCodec::ConverterState state;
    const auto text=QTextCodec::codecForName("UTF-8")->toUnicode(entry.contents.constData(),entry.contents.size(),&state);
    if (state.invalidChars || text.contains(QChar('\0'))) { entry.reason="Invalid UTF-8 desktop file"; return entry; }
    QMap<QString,QString> values;
    bool inGroup=false, seenGroup=false;
    for (const auto& raw:text.split('\n')) {
        const auto line=raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.startsWith('[')) {
            inGroup=line=="[Desktop Entry]";
            if (inGroup && seenGroup) { entry.reason="Duplicate Desktop Entry group"; return entry; }
            if (inGroup) seenGroup=true;
            continue;
        }
        if (!inGroup) continue;
        const auto equal=line.indexOf('=');
        if (equal<=0) { entry.reason="Malformed Desktop Entry key"; return entry; }
        const auto key=line.left(equal).trimmed();
        if (values.contains(key)) { entry.reason="Duplicate Desktop Entry key"; return entry; }
        values.insert(key,line.mid(equal+1).trimmed());
    }
    if (!seenGroup) { entry.reason="Missing Desktop Entry group"; return entry; }
    entry.name=decodeValue(values.value("Name",id));
    entry.command=values.value("Exec");
    entry.hidden=values.value("Hidden")=="true";
    if ((!entry.hidden && (values.value("Type")!="Application" || entry.command.isEmpty()))
        || (values.contains("Type") && values.value("Type")!="Application")) {
        entry.reason="Requires an Application entry and launch command"; return entry;
    }
    entry.valid=true;
    if (entry.hidden) { entry.reason="Disabled by Hidden=true"; return entry; }
    const auto only=values.value("OnlyShowIn").split(';',Qt::SkipEmptyParts);
    const auto notIn=values.value("NotShowIn").split(';',Qt::SkipEmptyParts);
    if (!only.isEmpty() && !notIn.isEmpty()) { entry.reason="Conflicting desktop restrictions"; return entry; }
    bool onlyMatches=only.isEmpty();
    for (const auto& desktop:paths.desktops) {
        if (only.contains(desktop)) onlyMatches=true;
        if (notIn.contains(desktop)) { entry.reason="Excluded on this desktop"; return entry; }
    }
    if (!onlyMatches) { entry.reason="Only enabled for another desktop"; return entry; }
    const auto executable=decodeValue(values.value("TryExec"));
    if (!executable.isEmpty()) {
        const bool found=QDir::isAbsolutePath(executable)
            ? QFileInfo(executable).isFile() && QFileInfo(executable).isExecutable()
            : !executable.contains('/') && !QStandardPaths::findExecutable(executable).isEmpty();
        if (!found) { entry.reason="TryExec program is unavailable"; return entry; }
    }
    if (values.value("X-GNOME-Autostart-enabled")=="false" && paths.desktops.contains("GNOME")) {
        entry.reason="Disabled by GNOME desktop setting"; return entry;
    }
    entry.eligible=true; entry.reason="Enabled for this desktop";
    return entry;
}
QByteArray withHidden(const QByteArray& source,bool hidden) {
    const auto eol=source.contains("\r\n")?QByteArray("\r\n"):QByteArray("\n");
    auto editable=source;
    const bool bom=editable.startsWith(QByteArray::fromHex("efbbbf"));
    if (bom) editable.remove(0,3);
    auto lines=editable.split('\n');
    for (auto& line:lines) if (line.endsWith('\r')) line.chop(1);
    bool inGroup=false, inserted=false;
    QList<QByteArray> edited;
    for (const auto& line:lines) {
        const auto trimmed=line.trimmed();
        if (trimmed.startsWith('[')) {
            inGroup=trimmed=="[Desktop Entry]";
            edited.append(line);
            if (inGroup) { edited.append(hidden?"Hidden=true":"Hidden=false"); inserted=true; }
        } else if (!(inGroup && trimmed.left(trimmed.indexOf('=')).trimmed()=="Hidden" && trimmed.contains('='))) edited.append(line);
    }
    if (!inserted) return {};
    auto result=edited.join(eol);
    if (bom) result.prepend(QByteArray::fromHex("efbbbf"));
    return result;
}
bool safeTarget(const Paths& paths,const QString& id) {
    if (!validId(id) || !QDir::isAbsolutePath(paths.configHome)
        || QFileInfo(root(paths)).isSymLink()
        || QFileInfo(QDir(root(paths)).filePath(id)).isSymLink()) return false;
    const auto identity=[](const QString& path) {
        const auto canonical=QFileInfo(path).canonicalFilePath();
        return canonical.isEmpty()?QDir::cleanPath(QFileInfo(path).absoluteFilePath()):canonical;
    };
    for (const auto& directory:paths.configDirs)
        if (QDir::isAbsolutePath(directory) && identity(root(paths))==identity(QDir(directory).filePath("autostart"))) return false;
    return true;

}
}
Paths Paths::environment() {
    Paths paths;
    paths.configHome=qEnvironmentVariable("XDG_CONFIG_HOME");
    if (!QDir::isAbsolutePath(paths.configHome)) paths.configHome=QDir::home().filePath(".config");
    const auto configured=qEnvironmentVariable("XDG_CONFIG_DIRS");
    const auto candidates=(configured.isEmpty()?QString("/etc/xdg"):configured).split(':',Qt::SkipEmptyParts);
    for (const auto& path:candidates) if (QDir::isAbsolutePath(path) && !paths.configDirs.contains(path)) paths.configDirs.append(path);
    paths.desktops=qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(':',Qt::SkipEmptyParts);
    return paths;
}
QVector<Entry> discover(const Paths& paths) {
    QStringList directories{root(paths)};
    for (const auto& path:paths.configDirs) if (QDir::isAbsolutePath(path)) directories.append(QDir(path).filePath("autostart"));
    QMap<QString,QString> effective;
    QSet<QString> systemIds;
    for (int i=0;i<directories.size();++i) {
        for (const auto& id:QDir(directories[i]).entryList({"*.desktop"},QDir::Files,QDir::Name)) {
            if (!validId(id)) continue;
            if (i>0) systemIds.insert(id);
            if (!effective.contains(id)) effective.insert(id,QDir(directories[i]).filePath(id));
        }
    }
    QVector<Entry> entries;
    for (auto it=effective.cbegin();it!=effective.cend();++it)
        entries.append(parse(paths,it.key(),it.value(),QFileInfo(it.value()).absolutePath()==QFileInfo(root(paths)).absoluteFilePath(),systemIds.contains(it.key())));
    std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.name.compare(b.name,Qt::CaseInsensitive)<0;});
    return entries;
}
bool hasUndo(const Paths& paths,const QString& id) {
    return validId(id) && !QFileInfo(journalRoot(paths)).isSymLink()
        && !QFileInfo(QFileInfo(journalRoot(paths)).absolutePath()).isSymLink()
        && QFileInfo::exists(journalPath(paths,id)) && !QFileInfo(journalPath(paths,id)).isSymLink();
}
Change setEnabled(const Paths& paths,const QString& id,bool enabled) {
    if (!safeTarget(paths,id)) return {false,"Unsafe startup filename or symbolic user target"};
    const auto entries=discover(paths);
    const auto found=std::find_if(entries.begin(),entries.end(),[&](const Entry& entry){return entry.id==id;});
    if (found==entries.end() || !found->valid) return {false,"This entry is missing or invalid; no changes were made"};
    auto source=found->contents;
    if (enabled && found->command.isEmpty()) {
        source.clear();
        for (const auto& directory:paths.configDirs) {
            const auto path=QDir(directory).filePath("autostart/"+id);
            if (!QFileInfo::exists(path)) continue;
            const auto candidate=parse(paths,id,path,false,true);
            if (candidate.valid && !candidate.command.isEmpty()) source=candidate.contents;
            break; // The first system entry masks all lower-priority files.

        }
        if (source.isEmpty()) return {false,"No valid launch command is available to enable"};
    }
    const auto contents=withHidden(source,!enabled);
    if (contents.isEmpty()) return {false,"Could not update the Desktop Entry group"};
    const auto target=QDir(root(paths)).filePath(id);
    QByteArray before;
    const bool existed=QFileInfo::exists(target);
    if (existed && !readBounded(target,before)) return {false,"Could not back up the current user entry"};
    if (existed && before==contents) return {true,"No change needed"};
    if (!secureJournalDirectory(paths) || !QDir().mkpath(root(paths))) return {false,"Cannot prepare a private undo record"};
    const auto recordPath=journalPath(paths,id);
    if (QFileInfo(recordPath).isSymLink()) return {false,"Symbolic undo record refused"};
    QByteArray oldRecord;
    const bool hadRecord=QFileInfo::exists(recordPath);
    if (hadRecord && !readBounded(recordPath,oldRecord,JournalLimit)) return {false,"Existing undo record is unreadable"};
    const QJsonObject record{{"version",1},{"id",id},{"existed",existed},
        {"original",QString::fromLatin1(before.toBase64())},{"after_hash",hash(contents)},
        {"permissions",static_cast<int>(existed?QFileInfo(target).permissions():PrivateFile)}};
    if (!atomicWrite(recordPath,QJsonDocument(record).toJson())) return {false,"Could not save private undo record; entry unchanged"};
    const auto permissions=existed?QFileInfo(target).permissions():PrivateFile;
    if (!atomicWrite(target,contents,permissions)) {
        const bool rolledBack=hadRecord?atomicWrite(recordPath,oldRecord):QFile::remove(recordPath);
        return {false,rolledBack?"Could not write startup override; undo state restored":"Write failed; preserve the undo record for manual recovery"};
    }
    return {true,"User startup override updated. Undo last change is available; desktop restrictions still apply."};
}
Change undo(const Paths& paths,const QString& id) {
    if (!safeTarget(paths,id) || !hasUndo(paths,id) || QFileInfo(journalRoot(paths)).isSymLink())
        return {false,"No safe undo record is available"};
    QByteArray bytes;
    if (!readBounded(journalPath(paths,id),bytes,JournalLimit)) return {false,"Undo record is unreadable"};
    QJsonParseError error;
    const auto object=QJsonDocument::fromJson(bytes,&error).object();
    const auto encoded=object.value("original").toString().toLatin1();
    const auto original=QByteArray::fromBase64(encoded);
    if (error.error!=QJsonParseError::NoError || object.value("version").toInt()!=1
        || object.value("id").toString()!=id || !object.value("existed").isBool()
        || !object.value("original").isString() || !object.value("permissions").isDouble()
        || object.value("permissions").toInt()<0
        || (object.value("existed").toBool() && original.isEmpty())
        || original.size()>FileLimit || original.toBase64()!=encoded
        || object.value("after_hash").toString().size()!=64)
        return {false,"Invalid undo record; nothing restored"};
    const auto target=QDir(root(paths)).filePath(id);
    QByteArray current;
    if (!readBounded(target,current) || hash(current)!=object.value("after_hash").toString())
        return {false,"The startup entry changed outside Tux Steward; refusing to overwrite it"};
    const bool restored=object.value("existed").toBool()
        ? atomicWrite(target,original,static_cast<QFileDevice::Permissions>(object.value("permissions").toInt()))
        : QFile::remove(target);
    if (!restored) return {false,"Could not restore the previous user entry"};
    if (!QFile::remove(journalPath(paths,id))) return {true,"Previous entry restored; the old undo record could not be cleared"};
    return {true,"Previous user startup state restored"};
}
}
