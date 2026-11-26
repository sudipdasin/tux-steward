// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/maintenance.hpp"
#include "syspulse/private_store.hpp"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <dirent.h>
#include <sys/syscall.h>
#include <ctime>
#include <limits>

namespace syspulse::maintenance {
namespace {
using storage::Fd; using storage::Error;
constexpr int FileLimit=2000, EntryLimit=20000;
bool leaf(const QString& name) { static const QRegularExpression re("\\A[0-9a-fA-F]{32}\\.png\\z"); return re.match(name).hasMatch(); }
bool runId(const QString& id) { return QUuid(id).toString(QUuid::WithoutBraces)==id && !QUuid(id).isNull(); }
bool relative(const QString& path) {
    if(QStringList{"normal","large","x-large","xx-large"}.contains(path))return true;
    static const QRegularExpression fail("\\Afail/([A-Za-z0-9._-]{1,80})\\z"); const auto m=fail.match(path);
    return m.hasMatch()&&m.captured(1)!="."&&m.captured(1)!="..";
}
QStringList names(int fd) {
    DIR* dir=::fdopendir(::dup(fd)); if(!dir)throw Error("Cannot enumerate cache directory.");
    QStringList result;
    while(auto* item=::readdir(dir)) { const QByteArray name(item->d_name); if(name!="."&&name!="..")result.append(QFile::decodeName(name)); if(result.size()>EntryLimit)break; }
    ::closedir(dir); return result;
}
Fd thumbnails(int root) { return storage::childDirectory(root,"thumbnails",false); }
Fd parent(int thumbnailsFd,const QString& relativePath,bool create=false) {
    if(!relative(relativePath))throw Error("Unsupported thumbnail category.");
    Fd fd(::dup(thumbnailsFd));
    for(const auto& part:relativePath.split('/'))fd=storage::childDirectory(fd.get(),QFile::encodeName(part),create);
    return fd;
}
File record(const QString& dir,const QString& name,const struct stat& st,quint64 parentInode) {
    return {dir,name,static_cast<quint64>(st.st_dev),static_cast<quint64>(st.st_ino),static_cast<quint64>(st.st_size),parentInode,
        st.st_mtim.tv_sec,st.st_mtim.tv_nsec,st.st_ctim.tv_sec,st.st_ctim.tv_nsec};
}
bool same(const File& file,const struct stat& st,bool ctime=false) {
    return S_ISREG(st.st_mode)&&st.st_uid==::geteuid()&&st.st_nlink==1&&!(st.st_mode&022)&&st.st_size>=0
        &&file.device==static_cast<quint64>(st.st_dev)&&file.inode==static_cast<quint64>(st.st_ino)&&file.size==static_cast<quint64>(st.st_size)
        &&file.modified==st.st_mtim.tv_sec&&file.modifiedNanos==st.st_mtim.tv_nsec
        &&(!ctime||(file.changed==st.st_ctim.tv_sec&&file.changedNanos==st.st_ctim.tv_nsec));
}
Fd recoveryRoot(int root,bool create) { auto app=storage::childDirectory(root,"syspulse",create,true); return storage::childDirectory(app.get(),"recovery",create,true); }
QJsonObject json(const File& f,int index) {
    return {{"directory",f.directory},{"name",f.name},{"payload",QString::number(index)},
        {"device",QString::number(f.device)},{"inode",QString::number(f.inode)},{"size",QString::number(f.size)},
        {"modified",QString::number(f.modified)},{"modified_ns",QString::number(f.modifiedNanos)}};
}
QVector<File> manifest(int fd,QString* created=nullptr) {
    QJsonParseError error; const auto doc=QJsonDocument::fromJson(storage::read(fd,"manifest.json"),&error);
    const auto object=doc.object(); const auto array=object.value("files").toArray();
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||object.value("schema").toInt()!=1||array.size()>FileLimit||!object.value("files").isArray())throw Error("Invalid recovery manifest.");
    QVector<File> files; QSet<QString> seen;
    for(int i=0;i<array.size();++i) {
        const auto row=array[i].toObject(); File f; f.directory=row.value("directory").toString(); f.name=row.value("name").toString();
        if(!relative(f.directory)||!leaf(f.name)||row.value("payload").toString()!=QString::number(i)||seen.contains(f.directory+'/'+f.name))throw Error("Invalid recovery path.");
        seen.insert(f.directory+'/'+f.name);
        bool ok=true;
        auto number=[&](const char* key){bool valid=false; auto n=row.value(key).toString().toULongLong(&valid);ok=ok&&valid;return n;};
        f.device=number("device");f.inode=number("inode");f.size=number("size");f.modified=static_cast<qint64>(number("modified"));f.modifiedNanos=static_cast<qint64>(number("modified_ns"));
        if(!ok||f.modified<0||f.modifiedNanos<0||f.modifiedNanos>=1000000000)throw Error("Invalid recovery identity.");
        files.append(f);
    }
    if(created)*created=object.value("created").toString();
    return files;
}
int move(int from,const QByteArray& name,int to,const QByteArray& target) {
    return static_cast<int>(::syscall(SYS_renameat2,from,name.constData(),to,target.constData(),1 /* RENAME_NOREPLACE */));
}
void finishEmptyRun(int recovery,const QByteArray& id,int run) {
    const auto entries=names(run); if(entries.size()!=1||entries[0]!="manifest.json")return;
    ::unlinkat(run,"manifest.json",0);::fsync(run);::unlinkat(recovery,id.constData(),AT_REMOVEDIR);::fsync(recovery);
}
}
QString cacheRoot() { return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)); }
QStringList categories() { return {"normal","large","x-large","xx-large","fail"}; }
Plan scan(const QString& root,const QStringList& selected,int ageDays,const std::atomic_bool* cancel) {
    if(selected.isEmpty()||ageDays<1||ageDays>3650)throw Error("Select a thumbnail category and an age from 1 to 3650 days.");
    QSet<QString> used;for(const auto& id:selected){if(!categories().contains(id)||used.contains(id))throw Error("Invalid or duplicate category.");used.insert(id);}
    auto base=storage::directory(root); const auto identity=storage::info(base.get());
    Plan plan;plan.root=root;plan.categories=selected;plan.ageDays=ageDays;plan.device=identity.st_dev;plan.inode=identity.st_ino;plan.created=::time(nullptr);
    struct stat thumbnailInfo{};
    if(::fstatat(base.get(),"thumbnails",&thumbnailInfo,AT_SYMLINK_NOFOLLOW)&&errno==ENOENT)return plan;
    auto thumbs=thumbnails(base.get());
    QStringList directories;
    for(const auto& id:selected) {
        if(id!="fail"){directories.append(id);continue;}
        try { auto fail=storage::childDirectory(thumbs.get(),"fail",false); for(const auto& name:names(fail.get()))if(relative("fail/"+name))directories.append("fail/"+name); }catch(const Error&){}
    }
    int inspected=0;QElapsedTimer timer;timer.start();
    for(const auto& directory:directories) {
        Fd dir;try{dir=parent(thumbs.get(),directory);}catch(const Error&){continue;}
        const auto pinfo=storage::info(dir.get());if(static_cast<quint64>(pinfo.st_dev)!=plan.device)continue;
        for(const auto& name:names(dir.get())) {
            if((cancel&&cancel->load())||plan.files.size()>=FileLimit||++inspected>EntryLimit||timer.elapsed()>5000){plan.limited=true;return plan;}
            if(!leaf(name))continue;
            struct stat st{};const auto encoded=QFile::encodeName(name);
            if(::fstatat(dir.get(),encoded.constData(),&st,AT_SYMLINK_NOFOLLOW)||!S_ISREG(st.st_mode)||st.st_uid!=::geteuid()||st.st_nlink!=1||(st.st_mode&022)||st.st_size<0||st.st_mtim.tv_sec<0||st.st_mtim.tv_sec>plan.created-static_cast<qint64>(ageDays)*86400||static_cast<quint64>(st.st_dev)!=plan.device)continue;
            if(std::numeric_limits<quint64>::max()-plan.bytes<static_cast<quint64>(st.st_size))throw Error("Cache size overflow.");
            plan.files.append(record(directory,name,st,pinfo.st_ino));plan.bytes+=st.st_size;
        }
    }
    return plan;
}
Outcome stage(const Plan& plan,const std::function<void()>& beforeMove) {
    if(plan.files.isEmpty()||plan.files.size()>FileLimit||::time(nullptr)-plan.created>3600||plan.created>::time(nullptr)+5)throw Error("Scan again before applying a cache plan.");
    auto base=storage::directory(plan.root);const auto identity=storage::info(base.get());
    if(static_cast<quint64>(identity.st_dev)!=plan.device||static_cast<quint64>(identity.st_ino)!=plan.inode)throw Error("Cache root changed; scan again.");
    QSet<QString> seen;
    for(const auto& item:plan.files){const auto key=item.directory+'/'+item.name;if(!relative(item.directory)||!leaf(item.name)||!plan.categories.contains(item.directory.section('/',0,0))||seen.contains(key))throw Error("The reviewed plan has an invalid or duplicate path/category.");seen.insert(key);}
    auto thumbs=thumbnails(base.get());auto recovery=recoveryRoot(base.get(),true);auto lock=storage::lock(recovery.get());
    Outcome result;result.run=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto id=result.run.toLatin1();
    if(::mkdirat(recovery.get(),id.constData(),0700))throw Error("Cannot create recovery run.");
    auto run=storage::childDirectory(recovery.get(),id,false,true);QJsonArray rows;
    for(int i=0;i<plan.files.size();++i)rows.append(json(plan.files[i],i));
    // Write-ahead identity/path map exists before any file is moved.
    storage::write(run.get(),"manifest.json",QJsonDocument(QJsonObject{{"schema",1},{"created",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"files",rows}}).toJson(QJsonDocument::Compact));
    for(int i=0;i<plan.files.size();++i) {
        const auto& file=plan.files[i];
        try {
            auto dir=parent(thumbs.get(),file.directory);if(static_cast<quint64>(storage::info(dir.get()).st_ino)!=file.parentInode){++result.skipped;continue;}
            const auto name=QFile::encodeName(file.name);Fd opened(::openat(dir.get(),name.constData(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));
            if(!opened||!same(file,storage::info(opened.get()),true)){++result.skipped;continue;}
            if(beforeMove)beforeMove();
            const auto payload=QByteArray::number(i);
            if(move(dir.get(),name,run.get(),payload)){++result.skipped;continue;}
            struct stat moved{};
            if(::fstatat(run.get(),payload.constData(),&moved,AT_SYMLINK_NOFOLLOW)||!same(file,moved)) {
                move(run.get(),payload,dir.get(),name);++result.skipped;continue;
            }
            ::fsync(dir.get());++result.changed;result.bytes+=file.size;
        }catch(const Error&){++result.skipped;}
    }
    ::fsync(run.get());::fsync(recovery.get());finishEmptyRun(recovery.get(),id,run.get());
    result.message=QString("Moved %1 file(s) to recovery; skipped %2 changed/unavailable file(s). Disk space is not freed until you permanently remove the recovery files.").arg(result.changed).arg(result.skipped);return result;
}
QVector<Recovery> recoveries(const QString& root) {
    auto base=storage::directory(root);struct stat st{};
    if(::fstatat(base.get(),"syspulse",&st,AT_SYMLINK_NOFOLLOW)&&errno==ENOENT)return {};
    auto app=storage::childDirectory(base.get(),"syspulse",false,true);
    if(::fstatat(app.get(),"recovery",&st,AT_SYMLINK_NOFOLLOW)&&errno==ENOENT)return {};
    auto recovery=storage::childDirectory(app.get(),"recovery",false,true);
    QVector<Recovery> result;
    for(const auto& id:names(recovery.get())) {
        if(!runId(id))continue;
        if(result.size()>=100)break;
        Recovery row;row.id=id;
        try { auto run=storage::childDirectory(recovery.get(),id.toLatin1(),false,true);const auto files=manifest(run.get(),&row.created);
            for(int i=0;i<files.size();++i){struct stat st{};const auto name=QByteArray::number(i);if(::fstatat(run.get(),name.constData(),&st,AT_SYMLINK_NOFOLLOW)){if(errno==ENOENT)continue;throw Error("Recovery file unavailable.");}if(!same(files[i],st))throw Error("Recovery data changed; retained for manual inspection.");++row.files;row.bytes+=files[i].size;}
        }catch(const std::exception& e){row.problem=e.what();}
        if(row.files||!row.problem.isEmpty())result.append(row);
    }
    return result;
}
static Outcome processRecovery(const QString& root,const QString& id,bool remove) {
    if(!runId(id))throw Error("Invalid recovery run.");
    auto base=storage::directory(root);auto recovery=recoveryRoot(base.get(),false);auto lock=storage::lock(recovery.get());auto run=storage::childDirectory(recovery.get(),id.toLatin1(),false,true);const auto files=manifest(run.get());
    Fd thumbs;if(!remove)thumbs=storage::childDirectory(base.get(),"thumbnails",true);
    Outcome result;result.run=id;
    for(int i=0;i<files.size();++i) {
        const auto payload=QByteArray::number(i);struct stat st{};
        if(::fstatat(run.get(),payload.constData(),&st,AT_SYMLINK_NOFOLLOW)){if(errno!=ENOENT)++result.skipped;continue;}
        if(!same(files[i],st)){++result.skipped;continue;}
        if(remove) {if(::unlinkat(run.get(),payload.constData(),0)){++result.skipped;continue;}}
        else {try{auto dir=parent(thumbs.get(),files[i].directory,true);if(move(run.get(),payload,dir.get(),QFile::encodeName(files[i].name))){++result.skipped;continue;}::fsync(dir.get());}catch(const Error&){++result.skipped;continue;}}
        ++result.changed;result.bytes+=files[i].size;
    }
    ::fsync(run.get());finishEmptyRun(recovery.get(),id.toLatin1(),run.get());
    result.message=QString("%1 %2 file(s); skipped %3 changed or conflicting file(s).").arg(remove?"Permanently removed":"Restored").arg(result.changed).arg(result.skipped);return result;
}
Outcome restore(const QString& root,const QString& id){return processRecovery(root,id,false);}
Outcome purge(const QString& root,const QString& id){return processRecovery(root,id,true);}
}
