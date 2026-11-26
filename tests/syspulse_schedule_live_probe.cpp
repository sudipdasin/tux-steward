// SPDX-License-Identifier: GPL-3.0-or-later
// Manual integration probe: run only in an isolated VM with a real systemd
// user manager and the candidate package installed. Never part of ordinary CTest.
#include "syspulse/window.hpp"
#include "syspulse/schedule_page.hpp"
#include "syspulse/history.hpp"
#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace {
int checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
void wait(const std::function<bool()>& done){QElapsedTimer timer;timer.start();while(!done()&&timer.elapsed()<25000){qApp->processEvents();QThread::msleep(5);}check(done(),"real user-manager UI operation finished");}
void confirm(){QTimer::singleShot(30,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});}
QByteArray ctl(const QStringList& args){QProcess p;p.start("/usr/bin/systemctl",args);check(p.waitForStarted(3000)&&p.waitForFinished(20000)&&p.exitCode()==0,"actual systemctl integration command succeeded");return p.readAllStandardOutput();}
}
int main(int argc,char** argv){QApplication app(argc,argv);app.setApplicationName("SysPulse");try{
    check(::geteuid()!=0,"integration probe runs as the desktop user");
    const int before=syspulse::readHistory(syspulse::historyDirectory()).size();
    syspulse::Window window(false);window.show();window.navigate("schedules");
    auto* scheduler=static_cast<syspulse::SchedulePage*>(window.findChild<QWidget*>("diagnosticSchedule"));check(scheduler,"native diagnostic scheduler is embedded");
    window.findChild<QComboBox*>("schedulePreset")->setCurrentText("storage");window.findChild<QSpinBox*>("scheduleHour")->setValue((QTime::currentTime().hour()+1)%24);
    confirm();window.findChild<QPushButton*>("enableSchedule")->click();wait([&]{return !scheduler->isBusy();});
    std::cout<<window.findChild<QLabel*>("scheduleStatus")->text().toStdString()<<'\n';
    check(scheduler->automationSummary().contains("active / enabled"),"UI confirms actual enabled and active timer");
    check(ctl({"--user","show","syspulse-diagnostics.timer","--property=NextElapseUSecRealtime","--value"}).trimmed().size()>0,"real timer has a next scheduled run");
    ctl({"--user","start","syspulse-diagnostics.service"});
    const auto state=ctl({"--user","show","syspulse-diagnostics.service","--property=Result,ExecMainStatus","--no-pager"});
    check(state.contains("Result=success")&&state.contains("ExecMainStatus=0"),"installed diagnostic helper completed under the real user manager");
    window.navigate("reports");check(window.findChild<QTableWidget*>("sessionReports")->rowCount()>=before+3,"GUI loads both service-generated summaries and schedule outcome");
    window.navigate("schedules");if(argc>1)check(window.grab().save(QString::fromLocal8Bit(argv[1])+"-enabled.png"),"capture confirmed actual timer state");
    confirm();window.findChild<QPushButton*>("disableSchedule")->click();wait([&]{return !scheduler->isBusy();});
    check(scheduler->automationSummary().contains("inactive / disabled"),"UI confirms actual stopped and disabled timer");
    const auto after=syspulse::readHistory(syspulse::historyDirectory());check(after.size()>=before+4,"enable, helper run and disable leave persistent redacted outcomes");
    if(argc>1)check(window.grab().save(QString::fromLocal8Bit(argv[1])+"-disabled.png"),"capture confirmed disabled state");
    std::cout<<checks<<" real systemd user-timer enable/run/report/disable checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
