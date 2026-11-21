// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/autostart.hpp"
#include "tux/system_monitor_tabs.hpp"
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool value,const char* text) { ++checks; if (!value) throw std::runtime_error(text); }
void write(const QString& path,const QByteArray& bytes) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()),"fixture directory creation");
    QFile file(path); require(file.open(QIODevice::WriteOnly|QIODevice::Truncate) && file.write(bytes)==bytes.size(),"fixture file write");
}
QByteArray read(const QString& path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
const QByteArray Desktop="[Desktop Entry]\nType=Application\nName=Demo\nExec=/bin/true\n";
tux::autostart::Entry find(const tux::autostart::Paths& paths,const QString& id) {
    for (const auto& entry:tux::autostart::discover(paths)) if (entry.id==id) return entry;
    throw std::runtime_error("fixture entry missing");
}
QString userFile(const tux::autostart::Paths& paths,const QString& id) { return QDir(paths.configHome).filePath("autostart/"+id); }
QString journal(const tux::autostart::Paths& paths,const QString& id) {
    const auto digest=QCryptographicHash::hash(id.toUtf8(),QCryptographicHash::Sha256).toHex();
    return QDir(paths.configHome).filePath("tux-steward/autostart-undo/"+QString::fromLatin1(digest)+".json");
}
struct Environment {
    QMap<QByteArray,QByteArray> previous;
    void set(const QByteArray& key,const QByteArray& value) { if (!previous.contains(key)) previous.insert(key,qgetenv(key)); qputenv(key,value); }
    ~Environment() { for (auto it=previous.cbegin();it!=previous.cend();++it) { if (it.value().isNull()) qunsetenv(it.key()); else qputenv(it.key(),it.value()); } }
};
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        QTemporaryDir directory; require(directory.isValid(),"temporary startup fixture root");
        tux::autostart::Paths paths{directory.filePath("user"),{directory.filePath("system-a"),directory.filePath("system-b")},{"XFCE"}};
        const auto sys=[&](const QString& id,int index=0) { return QDir(paths.configDirs[index]).filePath("autostart/"+id); };
        const auto source=Desktop+"# Hidden=true\n[Desktop Action Extra]\nName=Extra\nHidden=true\n";
        write(sys("demo.desktop"),source); write(sys("demo.desktop",1),Desktop+"Comment=lower precedence\n");
        auto entry=find(paths,"demo.desktop");
        require(entry.eligible && !entry.hidden && !entry.userOverride && entry.hasSystemDefault,"effective system priority and section-scoped Hidden parsing");
        require(tux::autostart::setEnabled(paths,"demo.desktop",false).ok,"disable system default via user override");
        require(find(paths,"demo.desktop").hidden && find(paths,"demo.desktop").userOverride,"user override takes precedence");
        require(read(sys("demo.desktop"))==source,"system file never modified");
        require(read(userFile(paths,"demo.desktop")).contains("[Desktop Action Extra]\nName=Extra\nHidden=true"),"unrelated desktop action retained");
        require(tux::autostart::hasUndo(paths,"demo.desktop"),"undo record created");
        const auto permissions=QFileInfo(journal(paths,"demo.desktop")).permissions();
        require(!(permissions & (QFileDevice::ReadGroup|QFileDevice::ReadOther|QFileDevice::WriteGroup|QFileDevice::WriteOther)),"undo contents private");
        require(tux::autostart::undo(paths,"demo.desktop").ok && !QFileInfo::exists(userFile(paths,"demo.desktop")),"undo removes only managed new override");
        require(read(sys("demo.desktop"))==source && !tux::autostart::hasUndo(paths,"demo.desktop"),"system retained and undo consumed");
        const QByteArray custom="[Desktop Entry]\r\nType=Application\r\nName=Custom\r\nExec=/bin/true\r\n# keep exact bytes\r\nHidden=false\r\n";
        write(userFile(paths,"custom.desktop"),custom);
        require(tux::autostart::setEnabled(paths,"custom.desktop",false).ok,"user entry disabled");
        require(tux::autostart::undo(paths,"custom.desktop").ok && read(userFile(paths,"custom.desktop"))==custom,"undo restores exact CRLF user contents");
        require(tux::autostart::setEnabled(paths,"custom.desktop",false).ok,"second user change");
        write(userFile(paths,"custom.desktop"),custom+"Comment=external edit\r\n");
        require(!tux::autostart::undo(paths,"custom.desktop").ok && read(userFile(paths,"custom.desktop")).contains("external edit"),"undo refuses external changes");
        write(userFile(paths,"demo.desktop"),"[Desktop Entry]\nHidden=true\n");
        require(tux::autostart::setEnabled(paths,"demo.desktop",true).ok && find(paths,"demo.desktop").eligible,"enabling minimal tombstone restores system launch fields");
        require(tux::autostart::undo(paths,"demo.desktop").ok && read(userFile(paths,"demo.desktop"))=="[Desktop Entry]\nHidden=true\n","undo restores original tombstone");
        write(sys("masked.desktop"),"[Desktop Entry]\nHidden=true\n"); write(sys("masked.desktop",1),Desktop);
        require(!tux::autostart::setEnabled(paths,"masked.desktop",true).ok,"higher system tombstone cannot expose a lower source");
        write(sys("desktop.desktop"),Desktop+"OnlyShowIn=GNOME;\n");
        require(!find(paths,"desktop.desktop").eligible && find(paths,"desktop.desktop").reason.contains("another desktop"),"OnlyShowIn eligibility");
        write(sys("not.desktop"),Desktop+"NotShowIn=XFCE;\n");
        require(!find(paths,"not.desktop").eligible,"NotShowIn eligibility");
        write(sys("try.desktop"),Desktop+"TryExec=nonexistent-synthetic-autostart-program\n");
        require(!find(paths,"try.desktop").eligible,"TryExec availability");
        write(sys("okay.desktop"),Desktop+"TryExec=/bin/true\nNoDisplay=true\n");
        require(find(paths,"okay.desktop").eligible,"valid TryExec and NoDisplay does not disable autostart");
        write(sys("duplicate.desktop"),Desktop+"Name=Duplicate\n");
        require(!find(paths,"duplicate.desktop").valid && !tux::autostart::setEnabled(paths,"duplicate.desktop",false).ok,"duplicate keys refused");
        write(sys("invalid.desktop"),"[Other]\nName=Not an app\nHidden=true\n");
        require(!find(paths,"invalid.desktop").valid,"missing desktop group refused");
        write(sys("large.desktop"),QByteArray(256*1024+1,'x'));
        require(!find(paths,"large.desktop").valid,"oversized desktop file bounded");
        require(!tux::autostart::setEnabled(paths,"../escape.desktop",false).ok,"path traversal refused");
        auto overlapping=paths; overlapping.configHome=paths.configDirs.front();
        require(!tux::autostart::setEnabled(overlapping,"demo.desktop",false).ok && read(sys("demo.desktop"))==source,"overlapping system and user directories cannot edit system defaults");
        const auto bom=QByteArray::fromHex("efbbbf")+Desktop;
        write(userFile(paths,"bom.desktop"),bom);
        require(tux::autostart::setEnabled(paths,"bom.desktop",false).ok,"UTF-8 BOM desktop entry edit");
        require(tux::autostart::undo(paths,"bom.desktop").ok && read(userFile(paths,"bom.desktop"))==bom,"BOM contents restored exactly");
        const auto outside=directory.filePath("outside.desktop"); write(outside,Desktop);
        require(QFile::link(outside,userFile(paths,"link.desktop")),"symbolic fixture creation");
        require(!tux::autostart::setEnabled(paths,"link.desktop",false).ok && read(outside)==Desktop,"symbolic user file never overwritten");
        auto linked=paths; linked.configHome=directory.filePath("linked"); QDir().mkpath(linked.configHome);
        require(QFile::link(QFileInfo(sys("demo.desktop")).absolutePath(),QDir(linked.configHome).filePath("autostart")),"symbolic folder fixture");
        require(!tux::autostart::setEnabled(linked,"demo.desktop",false).ok,"symbolic autostart folder refused");
        write(userFile(paths,"journal.desktop"),Desktop);
        require(tux::autostart::setEnabled(paths,"journal.desktop",false).ok,"journal fixture mutation");
        write(journal(paths,"journal.desktop"),"not json");
        require(!tux::autostart::undo(paths,"journal.desktop").ok,"corrupt undo record refused");
        auto blocked=paths; blocked.configHome=directory.filePath("blocked");
        write(QDir(blocked.configHome).filePath("tux-steward"),"blocking ordinary file");
        write(userFile(blocked,"blocked.desktop"),Desktop);
        require(!tux::autostart::setEnabled(blocked,"blocked.desktop",false).ok
            && read(userFile(blocked,"blocked.desktop"))==Desktop,"backup failure leaves startup entry unchanged");

        auto moved=paths; moved.configHome=directory.filePath("moved-journal");
        write(userFile(moved,"move.desktop"),Desktop);
        require(tux::autostart::setEnabled(moved,"move.desktop",false).ok,"undo relocation fixture");
        const auto savedDirectory=QDir(moved.configHome).filePath("tux-steward");
        const auto relocated=QDir(moved.configHome).filePath("relocated");
        require(QDir().rename(savedDirectory,relocated) && QFile::link(relocated,savedDirectory),"symbolic journal fixture");
        require(!tux::autostart::hasUndo(moved,"move.desktop") && !tux::autostart::undo(moved,"move.desktop").ok,"symbolic journal directory refused");

        Environment environment;
        environment.set("XDG_CONFIG_HOME",paths.configHome.toUtf8());
        environment.set("XDG_CONFIG_DIRS",paths.configDirs.join(':').toUtf8());
        environment.set("XDG_CURRENT_DESKTOP","XFCE");
        require(tux::autostart::Paths::environment().configHome==paths.configHome,"XDG custom user directory");
        environment.set("XDG_CONFIG_DIRS",(paths.configDirs[0]+":relative:"+paths.configDirs[1]).toUtf8());
        require(tux::autostart::Paths::environment().configDirs==paths.configDirs,"relative XDG paths ignored");
        environment.set("XDG_CONFIG_HOME","relative");
        require(tux::autostart::Paths::environment().configHome==QDir::home().filePath(".config"),"relative XDG home falls back");
        environment.set("XDG_CONFIG_HOME",paths.configHome.toUtf8());
        write(sys("ui-demo.desktop"),QByteArray(Desktop).replace("Name=Demo","Name=UI Demo"));
        QTabWidget tabs; new SystemMonitorTabs(&tabs); tabs.resize(1100,760); tabs.show(); app.processEvents();
        auto *experimental=tabs.findChild<QTabWidget *>("experimentalResourceTabs");
        require(experimental!=nullptr,"experimental group exists");
        int startup=-1; for (int index=0;index<tabs.count();++index) if (tabs.tabText(index)=="Startup apps") startup=index;
        require(startup>=0,"startup tab exists"); tabs.setCurrentIndex(startup); app.processEvents();
        if(argc>1)require(tabs.grab().save(QString::fromLocal8Bit(argv[1])),"startup screenshot");
        auto *table=tabs.findChild<QTableWidget *>("startupTable");
        auto *search=tabs.findChild<QLineEdit *>("startupSearch");
        auto *disable=tabs.findChild<QPushButton *>("startupDisable");
        auto *undo=tabs.findChild<QPushButton *>("startupUndo");
        auto *filter=tabs.findChild<QComboBox *>("startupScopeFilter");
        require(table && search && disable && undo && filter,"startup manager controls exist");
        const auto select=[&](const QString& id) {
            for (int row=0;row<table->rowCount();++row) if (table->item(row,0)->data(Qt::UserRole).toString()==id) { table->selectRow(row); return true; }
            return false;
        };
        require(select("ui-demo.desktop") && disable->isEnabled(),"startup row selection");
        bool defaultCancel=false;
        QTimer::singleShot(0,[&] { if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            defaultCancel=box->defaultButton()==box->button(QMessageBox::Cancel);
            box->done(QMessageBox::Cancel);
        }});
        disable->click();
        require(defaultCancel && !QFileInfo::exists(userFile(paths,"ui-demo.desktop")),"confirmation defaults to cancel without changes");
        QTimer::singleShot(0,[] { if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->done(QMessageBox::Yes); });
        disable->click();
        require(find(paths,"ui-demo.desktop").hidden && undo->isEnabled(),"GUI creates reversible user override");
        filter->setCurrentIndex(2);
        require(select("ui-demo.desktop"),"system filter retains user override of system app");
        QTimer::singleShot(0,[] { if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->done(QMessageBox::Yes); });
        undo->click();
        require(!QFileInfo::exists(userFile(paths,"ui-demo.desktop")) && read(sys("ui-demo.desktop")).contains("Name=UI Demo"),"GUI undo restores system default without editing it");
        if (argc>1) require(tabs.grab().save(QString::fromLocal8Bit(argv[1])),"startup UI render");
        search->setText("synthetic-no-match-query");
        for (int row=0;row<table->rowCount();++row) require(table->isRowHidden(row),"startup search hides nonmatches");
        require(!disable->isEnabled(),"hidden selection cannot be changed");
        tabs.hide();
        std::cout<<checks<<" startup backend and GUI checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<"FAILED: "<<error.what()<<'\n'; return 1; }
}
