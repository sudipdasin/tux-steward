// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/core.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QTemporaryDir>
#include <csignal>
#include <iostream>
#include <unistd.h>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(QFileInfo(app.applicationFilePath()).fileName()=="xfconf-query") { std::cout<<"fixture mutation completed\n"; return 0; }
    if(argc>1 && QString(argv[1])=="--child") { std::cout<<"ready "<<getpid()<<'\n'<<std::flush; sleep(30); return 0; }
    if(argc>2 && QString(argv[1])=="--touch") { QFile file(QString::fromLocal8Bit(argv[2])); return file.open(QIODevice::WriteOnly)?0:1; }
    const auto self=app.applicationFilePath(); QTemporaryDir temp;
    if(!temp.isValid()) return 1;
    std::atomic_bool cancel{true}; const auto marker=temp.filePath("not-started");
    auto pre=tux::runCommand({self,"--touch",marker},3000,{},&cancel);
    if(!pre.cancelled || pre.code!=130 || QFile::exists(marker)) return 2;
    QProcess sibling; sibling.start(self,{"--child"});
    if(!sibling.waitForStarted(3000) || !sibling.waitForReadyRead(3000)) return 3;
    cancel.store(false);
    auto result=tux::runCommand({self,"--child"},10000,[&](const QByteArray& chunk){if(chunk.contains("ready")) cancel.store(true);},&cancel);
    const auto pid=QString::fromUtf8(result.output).section('\n',0,0).section(' ',1,1).toLongLong();
    const bool siblingAlive=sibling.state()==QProcess::Running && !sibling.waitForFinished(10);
    sibling.kill(); sibling.waitForFinished(3000);
    if(!result.cancelled || result.code!=130 || result.seconds>3 || !result.output.contains("Cancellation requested") || !siblingAlive) return 4;
    if(pid<=0 || kill(static_cast<pid_t>(pid),0)==0) return 5;
    if(!result.json().value("cancelled").toBool() || !tux::sessionReport({result}).value("results").toArray().first().toObject().value("cancelled").toBool()) return 6;
    cancel.store(false);
    const auto timed=tux::runCommand({self,"--child"},80,{},&cancel);
    if(timed.code!=124 || timed.cancelled) return 7;
    const auto normal=tux::runCommand({self,"--touch",marker},1000,{},&cancel);
    if(normal.code!=0 || normal.cancelled || !QFile::exists(marker)) return 8;
    const auto oldPath=qgetenv("PATH"); const auto helper=temp.filePath("xfconf-query");
    if(!QFile::copy(self,helper)) return 9;
    qputenv("PATH",temp.path().toUtf8()); cancel.store(true);
    const auto mutation=tux::execute("compositor-on",true,3000,{},&cancel);
    qputenv("PATH",oldPath);
    if(mutation.code!=0 || mutation.cancelled || !mutation.output.contains("fixture mutation completed")) return 10;
    std::cout<<"Pre-start and live cancellation, owned-child cleanup, unrelated child retention, timeout distinction, token reset and mutation gate passed\n";
}
