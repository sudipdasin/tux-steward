// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include "syspulse/performance.hpp"
#include "syspulse/maintenance_page.hpp"
#include "syspulse/history.hpp"
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <iostream>
#include <stdexcept>
namespace {
int checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void wait(const std::function<bool()>& done){QElapsedTimer timer;timer.start();while(!done()&&timer.elapsed()<8000){qApp->processEvents();QThread::msleep(3);}check(done(),"asynchronous UI operation finished within deadline");}
bool reviewed=false;
void answer(QMessageBox::StandardButton value){reviewed=false;QTimer::singleShot(20,[value]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box){reviewed=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(value)->click();}});}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setApplicationName("SysPulse");QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QTemporaryDir profile;const auto cache=profile.path()+"/cache",data=profile.path()+"/data",config=profile.path()+"/config";
    qputenv("XDG_CACHE_HOME",cache.toUtf8());qputenv("XDG_DATA_HOME",data.toUtf8());qputenv("XDG_CONFIG_HOME",config.toUtf8());
    syspulse::applyPrecisionTheme();
    try{
        check(QDir().mkpath(cache+"/thumbnails/normal"),"create private thumbnail fixture");
        const auto path=cache+"/thumbnails/normal/"+QString(32,'a')+".png";QFile file(path);check(file.open(QIODevice::WriteOnly),"create old cache fixture");file.write("thumbnail-only");file.close();
        timespec old[2]{};old[0].tv_sec=old[1].tv_sec=::time(nullptr)-90*86400;check(::utimensat(AT_FDCWD,QFile::encodeName(path).constData(),old,0)==0,"age cache fixture");
        {
            syspulse::Window window(false);window.show();window.navigate("storage");
            auto* cleanup=static_cast<syspulse::MaintenancePage*>(window.findChild<QWidget*>("nativeCleanup"));
            // No Q_OBJECT is needed by these pages; use the named QWidget cast.
            check(cleanup!=nullptr,"cleanup page is embedded");
            for(const auto* category:cleanup->findChildren<QCheckBox*>())check(!category->isChecked(),"cleanup categories require explicit selection");
            auto* plan=window.findChild<QTableWidget*>("cleanupPlan");auto* apply=window.findChild<QPushButton*>("applyCleanup");
            check(plan->rowCount()==0&&!apply->isEnabled()&&QFileInfo::exists(path),"opening cleanup leaves files unchanged and no plan active");
            window.findChild<QCheckBox*>("cleanupCategory_normal")->setChecked(true);
            window.findChild<QPushButton*>("analyzeCleanup")->click();
            check(cleanup->isBusy()&&!window.findChild<QPushButton*>("analyzeSystem")->isEnabled(),"central admission blocks competing analysis while cleanup scans");
            wait([&]{return !cleanup->isBusy();});check(plan->rowCount()==1&&apply->isEnabled()&&QFileInfo::exists(path),"analysis produces a real read-only plan");
            answer(QMessageBox::Cancel);apply->click();check(reviewed&&QFileInfo::exists(path),"default-cancel review preserves thumbnails");
            if(argc>1)check(window.grab().save(QString::fromLocal8Bit(argv[1])+"-cleanup.png"),"capture reviewed cleanup plan");
            answer(QMessageBox::Yes);apply->click();wait([&]{return !cleanup->isBusy();});check(reviewed&&!QFileInfo::exists(path),"confirmed plan moves only reviewed thumbnails");
            auto* recovery=static_cast<syspulse::MaintenancePage*>(window.findChild<QWidget*>("nativeRecovery"));window.navigate("recovery");recovery->refresh();wait([&]{return !recovery->isBusy();});
            auto* runs=window.findChild<QTableWidget*>("recoveryRuns");check(runs->rowCount()==1,"staged run is available for recovery");runs->setCurrentCell(0,0);
            answer(QMessageBox::Cancel);window.findChild<QPushButton*>("purgeRecovery")->click();check(reviewed&&runs->rowCount()==1,"permanent removal requires fresh explicit confirmation");
            if(argc>1)check(window.grab().save(QString::fromLocal8Bit(argv[1])+"-recovery.png"),"capture actual staged recovery");
            answer(QMessageBox::Yes);window.findChild<QPushButton*>("restoreRecovery")->click();wait([&]{return !recovery->isBusy();});check(QFileInfo::exists(path)&&runs->rowCount()==0,"confirmed restore returns staged file and clears completed run");
            window.navigate("storage");window.findChild<QPushButton*>("analyzeCleanup")->click();wait([&]{return !cleanup->isBusy();});answer(QMessageBox::Yes);apply->click();wait([&]{return !cleanup->isBusy();});
            window.navigate("recovery");recovery->refresh();wait([&]{return !recovery->isBusy();});runs->setCurrentCell(0,0);answer(QMessageBox::Yes);window.findChild<QPushButton*>("purgeRecovery")->click();wait([&]{return !recovery->isBusy();});check(!QFileInfo::exists(path)&&runs->rowCount()==0,"separately confirmed purge removes staged thumbnail data");
            window.navigate("schedules");bool previewed=false;QTimer::singleShot(20,[&]{auto* modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());previewed=modal&&modal->windowTitle().contains("Read-only diagnostic");if(modal)modal->reject();});window.findChild<QPushButton*>("previewSchedule")->click();check(previewed&&!QFileInfo::exists(config+"/systemd/user/syspulse-diagnostics.timer"),"schedule preview writes no units");
            answer(QMessageBox::Cancel);window.findChild<QPushButton*>("enableSchedule")->click();check(reviewed&&!QFileInfo::exists(data+"/syspulse/schedule.json"),"cancelled schedule review does not save or activate automation");
            window.navigate("reports");auto* reports=window.findChild<QTableWidget*>("sessionReports");check(reports->rowCount()==4,"only completed mutations create persistent summaries");
            const auto exported=QJsonDocument(syspulse::historyReport(syspulse::readHistory(syspulse::historyDirectory()))).toJson();check(!exported.contains(profile.path().toUtf8())&&!exported.contains("thumbnail-only")&&!exported.contains("output"),"saved summary export excludes private paths and command output");
            if(argc>1)check(window.grab().save(QString::fromLocal8Bit(argv[1])+"-reports.png"),"capture actual persistent reports");
        }
        syspulse::Window reopened(false);reopened.navigate("reports");check(reopened.findChild<QTableWidget*>("sessionReports")->rowCount()==4,"report summaries survive closing and reopening the application");
        check(reopened.findChild<QLabel*>("statusFooter")->text().contains("not checked"),"startup does not assert automation is off without checking runtime state");
        std::cout<<checks<<" cleanup/recovery/review/persistence/scheduling UI checks passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
