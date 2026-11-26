// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/history.hpp"
#include "syspulse/schedule.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
namespace{int checks=0;void check(bool b,const char* t){++checks;if(!b)throw std::runtime_error(t);}bool fails(const std::function<void()>& f){try{f();return false;}catch(const std::exception&){return true;}}QByteArray read(const QString& p){QFile f(p);check(f.open(QIODevice::ReadOnly),"read fixture");return f.readAll();}}
int main(int argc,char** argv){QCoreApplication app(argc,argv);try{
 QTemporaryDir dir;const auto data=dir.path()+"/data";const auto units=dir.path()+"/config/systemd/user";
 tux::Result r;r.action="Storage usage";r.code=0;r.seconds=.25;r.output="PRIVATE_HOST SECRET_TOKEN /home/private-user";r.command=QStringList{"secret-command"};
 syspulse::appendHistory(data,r);const auto rows=syspulse::readHistory(data);check(rows.size()==1,"history survives a new read");const auto text=QJsonDocument(syspulse::historyReport(rows)).toJson();check(!text.contains("PRIVATE_HOST")&&!text.contains("secret-command")&&!text.contains("private-user")&&!text.contains("output"),"persistent and exported history uses a strict summary allowlist");
 struct stat st{};::stat(QFile::encodeName(data+"/history.json").constData(),&st);check((st.st_mode&077)==0&&st.st_uid==::geteuid(),"summary history is owner-only");
 r.action="private invented task";syspulse::appendHistory(data,r);check(syspulse::readHistory(data)[1].toObject().value("action")=="Unrecognized task","unknown labels cannot leak through history");
 syspulse::schedule::Plan p;check(syspulse::schedule::calendar(p)=="*-*-* 10:00:00","daily calendar");syspulse::schedule::save(units,data,p);
 syspulse::schedule::Plan loaded;check(syspulse::schedule::load(data,loaded)&&loaded.preset=="balanced","schedule plan persists");
 const auto service=read(units+"/syspulse-diagnostics.service");check(service.contains("/usr/bin/syspulse-diagnostic --preset balanced")&&!service.contains("pkexec")&&!service.contains("sudo"),"scheduled diagnostics use a fixed normal-user executable");
 p.cadence="weekly";p.weekday=5;p.hour=7;p.minute=30;p.acOnly=true;syspulse::schedule::save(units,data,p);check(read(units+"/syspulse-diagnostics.timer").contains("Fri *-*-* 07:30:00"),"weekly calendar saved");check(read(units+"/syspulse-diagnostics.service").contains("ConditionACPower=true"),"explicit AC condition saved");
 p.cadence="hourly";check(syspulse::schedule::calendar(p)=="*-*-* *:30:00","hourly calendar");p.preset="balanced\nExecStart=/bin/sh";check(fails([&]{syspulse::schedule::save(units,data,p);}),"preset injection rejected before writing units");
 p.preset="storage";QFile changed(units+"/syspulse-diagnostics.timer");check(changed.open(QIODevice::Append),"external edit fixture");changed.write("# external\n");changed.close();check(fails([&]{syspulse::schedule::save(units,data,p);}),"external unit edits are preserved rather than overwritten");check(read(units+"/syspulse-diagnostics.timer").endsWith("# external\n"),"external contents retained");
 QFile::remove(data+"/history.json");QFile outside(dir.path()+"/outside");check(outside.open(QIODevice::WriteOnly),"outside history fixture");outside.write("untouched");outside.close();QFile::link(outside.fileName(),data+"/history.json");check(fails([&]{syspulse::appendHistory(data,r);}),"history symlink cannot redirect a write");check(read(outside.fileName())=="untouched","outside history target preserved");
 for(int i=0;i<205;++i)syspulse::appendHistory(dir.path()+"/bounded",r);check(syspulse::readHistory(dir.path()+"/bounded").size()==200,"persistent history is bounded to the latest 200 summaries");
 QFile::remove(data+"/history.json");check(::mkfifo(QFile::encodeName(data+"/history.json").constData(),0600)==0,"FIFO record fixture");check(fails([&]{syspulse::readHistory(data);}),"special history files are rejected without blocking");
 QFile scheduleFile(data+"/schedule.json");check(scheduleFile.open(QIODevice::ReadOnly),"read saved plan fixture");auto row=QJsonDocument::fromJson(scheduleFile.readAll()).object();scheduleFile.close();row["hour"]=2.5;check(scheduleFile.open(QIODevice::WriteOnly|QIODevice::Truncate),"write fractional plan fixture");scheduleFile.write(QJsonDocument(row).toJson());scheduleFile.close();check(fails([&]{syspulse::schedule::load(data,loaded);}),"fractional saved time fields are rejected");
 std::cout<<checks<<" persistent privacy, private storage, schedule validation and external-edit checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
