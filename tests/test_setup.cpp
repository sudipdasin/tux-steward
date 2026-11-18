// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/setup.hpp"
#include "tux/version.hpp"
#include "tux/setup_wizard.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QProcess>
#include <QStandardPaths>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
void require(bool condition,const char *message) { if(!condition)throw std::runtime_error(message); }
void put(const QString &path,const QByteArray &bytes) { QFile file(path);require(file.open(QIODevice::WriteOnly),"fixture write open");require(file.write(bytes)==bytes.size(),"fixture write"); }
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    try {
        require(argc==6,"setup test arguments");
        QTemporaryDir temp;require(temp.isValid(),"temporary configuration");
        const QString source=temp.path()+"/archive/usr",home=temp.path()+"/user data $cash %item \\notes";
        QDir().mkpath(source+"/bin");QDir().mkpath(source+"/share/doc/tux-steward");
        const QStringList names={"bin/tux-steward","bin/tux-task-manager","bin/tux-setup","share/doc/tux-steward/LICENSE","share/doc/tux-steward/PRIVACY.md"};
        for(int i=0;i<names.size();++i)require(QFile::copy(QString::fromLocal8Bit(argv[i+1]),source+"/"+names[i]),"fixture archive copy");
        require(!tux::setup::install(source,home+"=invalid").ok,"unsupported launcher path refused");
        require(!tux::setup::install(source+"/missing",home).ok,"incomplete archive refused");
        tux::SetupWizard wizard(source,home);wizard.show();app.processEvents();
        wizard.next();require(wizard.currentId()==1,"review page");
        wizard.next();require(wizard.currentId()==1 && !QFileInfo::exists(tux::setup::installRoot(home)),"no install without consent");
        wizard.findChild<QCheckBox *>("setupConsent")->setChecked(true);wizard.next();app.processEvents();
        require(wizard.property("operationSucceeded").toBool() && wizard.currentId()==2,"wizard installed archive");
        const auto root=tux::setup::installRoot(home);
        for(int i=0;i<3;++i) {
            QProcess child;child.start(root+"/"+names[i],{"--version"});require(child.waitForFinished(5000) && child.exitCode()==0,"installed program starts");
            require(child.readAllStandardOutput().contains(tux::Version),"installed version");
        }
        require(!tux::setup::install(source,home).ok,"existing installation never overwritten");
        const QString unrelated=root+"/my-notes.txt";put(unrelated,"keep me");
        const QString altered=root+"/share/doc/tux-steward/PRIVACY.md";
        QFile original(altered);require(original.open(QIODevice::ReadOnly),"read saved original");const auto originalBytes=original.readAll();original.close();
        put(altered,"user edits");require(!tux::setup::remove(home).ok && QFileInfo::exists(root+"/bin/tux-steward"),"modified files prevent all removal");
        put(altered,originalBytes);
        const QString menu=home+"/applications/tux-steward-user.desktop";
        QFile entry(menu);require(entry.open(QIODevice::ReadOnly),"menu entry exists");const auto menuBytes=entry.readAll();entry.close();
        require(menuBytes.contains("%%item") && menuBytes.contains("\\$cash"),"desktop arguments escape percent and dollar");
        const QString validator=QStandardPaths::findExecutable("desktop-file-validate");
        require(!validator.isEmpty(),"desktop file validator installed on VM");
        for (const auto &entryName:QStringList{"tux-steward-user.desktop","tux-task-manager-user.desktop","tux-setup-user.desktop"}) {
            QProcess check;check.start(validator,{home+"/applications/"+entryName});
            require(check.waitForFinished(5000) && check.exitCode()==0,"generated desktop shortcut validates");
        }
        put(menu,"edited shortcut");require(!tux::setup::remove(home).ok,"modified menu retained");put(menu,menuBytes);
        require(QFile::remove(altered) && QFile::link(source+"/share/doc/tux-steward/PRIVACY.md",altered),"symlink fixture");
        require(!tux::setup::remove(home).ok,"symlink removal refused");QFile::remove(altered);put(altered,originalBytes);
        tux::SetupWizard removal(source,home);removal.findChild<QRadioButton *>("setupRemove")->setChecked(true);removal.show();removal.next();
        removal.findChild<QCheckBox *>("setupConsent")->setChecked(true);removal.next();app.processEvents();
        require(removal.property("operationSucceeded").toBool(),"removal wizard succeeds");
        require(!QFileInfo::exists(root+"/bin/tux-steward") && !QFileInfo::exists(menu),"owned programs and menus removed");
        require(QFileInfo::exists(unrelated),"unrelated user files retained");
        require(!tux::setup::remove(home).ok,"missing install record refused");
        const QString linkHome=temp.path()+"/linked-home";require(QFile::link(home,linkHome),"symlink home fixture");
        require(!tux::setup::install(source,linkHome).ok,"symlink install root refused");
        std::cout<<"GUI consent/install/remove, native copied executables, collisions, tamper/symlink refusal and unrelated-file retention passed\n";
        return 0;
    }catch(const std::exception &error){std::cerr<<"FAILED: "<<error.what()<<'\n';return 1;}
}
