// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/schedule.hpp"
#include "syspulse/private_store.hpp"
#include "tux/health.hpp"
#include <QJsonDocument>
#include <QFileInfo>
#include <QJsonObject>
#include <QStandardPaths>
namespace syspulse::schedule {
namespace {
void validate(const Plan& p) {
    if(!QStringList{"balanced","storage","logs","all"}.contains(p.preset)||!QStringList{"hourly","daily","weekly"}.contains(p.cadence)||p.hour<0||p.hour>23||p.minute<0||p.minute>59||p.weekday<1||p.weekday>7)throw storage::Error("Invalid diagnostic schedule.");
}
QJsonObject json(const Plan& p){return {{"schema",1},{"preset",p.preset},{"cadence",p.cadence},{"hour",p.hour},{"minute",p.minute},{"weekday",p.weekday},{"ac_only",p.acOnly}};}
Plan parse(const QByteArray& bytes) {
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(bytes,&error);
    const auto row=doc.object();
    auto integer=[&](const char* name){const auto value=row.value(name);return value.isDouble()&&value.toDouble()==value.toInt(-1);};
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||!integer("schema")||row.value("schema").toInt()!=1||!integer("hour")||!integer("minute")||!integer("weekday")||!row.value("ac_only").isBool())throw storage::Error("Saved diagnostic schedule is invalid.");
    Plan p{row.value("preset").toString(),row.value("cadence").toString(),row.value("hour").toInt(-1),row.value("minute").toInt(-1),row.value("weekday").toInt(-1),row.value("ac_only").toBool()};
    validate(p);return p;
}
}
QString configDirectory(){return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/systemd/user";}
QString dataDirectory(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/syspulse";}
QString calendar(const Plan& p){validate(p);const auto time=QString("%1:%2:00").arg(p.hour,2,10,QChar('0')).arg(p.minute,2,10,QChar('0'));if(p.cadence=="hourly")return QString("*-*-* *:%1:00").arg(p.minute,2,10,QChar('0'));if(p.cadence=="weekly")return QStringList{"Mon","Tue","Wed","Thu","Fri","Sat","Sun"}[p.weekday-1]+" *-*-* "+time;return "*-*-* "+time;}
QString service(const Plan& p){validate(p);return QString("[Unit]\nDescription=SysPulse read-only diagnostic summary\n%1\n[Service]\nType=oneshot\nExecStart=/usr/bin/syspulse-diagnostic --preset %2\nNoNewPrivileges=true\nNice=10\nIOSchedulingClass=idle\nTimeoutStartSec=200\n").arg(p.acOnly?"ConditionACPower=true":"",p.preset);}
QString timer(const Plan& p){return QString("[Unit]\nDescription=SysPulse diagnostic schedule\n\n[Timer]\nOnCalendar=%1\nPersistent=true\nAccuracySec=1min\nUnit=syspulse-diagnostics.service\n\n[Install]\nWantedBy=timers.target\n").arg(calendar(p));}
bool load(const QString& path,Plan& p){if(!QFileInfo::exists(path)&&!QFileInfo(path).isSymLink())return false;auto dir=storage::directory(path,false,true);const auto bytes=storage::read(dir.get(),"schedule.json",65536);if(bytes.isEmpty())return false;p=parse(bytes);return true;}
void save(const QString& config,const QString& data,const Plan& plan){
    validate(plan);auto dir=storage::directory(data,true,true);auto lock=storage::lock(dir.get());
    const auto oldRecord=storage::read(dir.get(),"schedule.json",65536);auto units=storage::directory(config,true);
    const auto oldService=storage::read(units.get(),"syspulse-diagnostics.service",65536);const auto oldTimer=storage::read(units.get(),"syspulse-diagnostics.timer",65536);
    if(oldRecord.isEmpty()){if(!oldService.isEmpty()||!oldTimer.isEmpty())throw storage::Error("Existing diagnostic units have no managed record; inspect them before replacing.");}
    else {const auto previous=parse(oldRecord);if(oldService!=service(previous).toUtf8()||oldTimer!=timer(previous).toUtf8())throw storage::Error("Diagnostic units were changed externally; existing files were retained.");}
    try {storage::write(units.get(),"syspulse-diagnostics.service",service(plan).toUtf8());storage::write(units.get(),"syspulse-diagnostics.timer",timer(plan).toUtf8());storage::write(dir.get(),"schedule.json",QJsonDocument(json(plan)).toJson(QJsonDocument::Compact));}
    catch(...){if(oldService.isEmpty())::unlinkat(units.get(),"syspulse-diagnostics.service",0);else storage::write(units.get(),"syspulse-diagnostics.service",oldService);if(oldTimer.isEmpty())::unlinkat(units.get(),"syspulse-diagnostics.timer",0);else storage::write(units.get(),"syspulse-diagnostics.timer",oldTimer);throw;}
}
}
