// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/health.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv); const auto name=QFileInfo(app.applicationFilePath()).fileName();
    if(QStringList{"df","free","lsblk","systemctl","journalctl"}.contains(name)) {
        QFile marker(qEnvironmentVariable("TUX_HEALTH_MARKER")); if(!marker.fileName().isEmpty() && marker.open(QIODevice::Append)) {marker.write(name.toUtf8()+"\n"); marker.close();}
        std::cout<<"synthetic diagnostic data\n";
        if(name=="free" && qEnvironmentVariableIsSet("TUX_HEALTH_SLOW")) sleep(3);
        return name=="free" && qEnvironmentVariableIsSet("TUX_HEALTH_FAIL")?42:0;
    }
    QTemporaryDir temp; if(!temp.isValid()) return 1;
    for(const auto& helper:QStringList{"df","free","lsblk","systemctl","journalctl"}) if(!QFile::copy(app.applicationFilePath(),temp.filePath(helper))) return 2;
    const auto oldPath=qgetenv("PATH"); qputenv("PATH",temp.path().toUtf8());
    const auto marker=temp.filePath("executed"); qputenv("TUX_HEALTH_MARKER",marker.toUtf8());
    for(const auto& id:tux::healthActions()) if(tux::action(id).changes || tux::action(id).admin) return 3;
    for(const auto& invalid:QVector<QStringList>{{},{"storage","apt-clean"},{"storage","storage"},{"made-up"}}) {
        bool refused=false; try{tux::runHealthCheck(invalid);}catch(const std::invalid_argument&){refused=true;}
        if(!refused || QFile::exists(marker)) return 4;
    }
    const QStringList ids={"storage","memory","devices","failed"}; int started=0,finished=0;
    auto run=tux::runHealthCheck(ids,true,1000,nullptr,[&](const QString&,const tux::Result* result){if(result) ++finished; else ++started;});
    if(run.results.size()!=4 || run.total!=4 || run.stopped || run.stoppedOnFailure || started!=4 || finished!=4 || run.results[0].action!=tux::action("storage").title) return 5;
    qputenv("TUX_HEALTH_FAIL","1"); run=tux::runHealthCheck(ids);
    if(run.results.size()!=2 || !run.stoppedOnFailure || run.results.last().code!=42) return 6;
    run=tux::runHealthCheck(ids,false); if(run.results.size()!=4 || run.stoppedOnFailure) return 7;
    qunsetenv("TUX_HEALTH_FAIL"); std::atomic_bool cancel{true};
    run=tux::runHealthCheck(ids,true,1000,&cancel); if(!run.results.isEmpty() || !run.stopped) return 8;
    cancel.store(false); qputenv("TUX_HEALTH_SLOW","1");
    std::thread stopper;
    run=tux::runHealthCheck(ids,true,1000,&cancel,[&](const QString& id,const tux::Result* result){if(id=="memory" && !result) stopper=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(100));cancel.store(true);});});
    if(stopper.joinable()) stopper.join();
    if(run.results.size()!=2 || !run.stopped || !run.results.last().cancelled || run.results.last().code!=130) return 9;
    // Isolate the deliberately slow helper: a timeout test must not assume an
    // unrelated Qt helper can start and complete within 100 ms on every VM.
    cancel.store(false); run=tux::runHealthCheck({"memory"},true,100,&cancel);
    if(run.results.size()!=1 || run.total!=1 || !run.stoppedOnFailure || run.results.last().code!=124) return 10;
    qunsetenv("TUX_HEALTH_SLOW"); QFile::remove(temp.filePath("lsblk"));
    run=tux::runHealthCheck({"devices"}); if(run.results.size()!=1 || run.results.first().code!=127 || !run.stoppedOnFailure) return 11;
    qputenv("PATH",oldPath); qunsetenv("TUX_HEALTH_MARKER");
    std::cout<<"Health plans: full validation before execution, read-only allowlist, ordered results, failure policy, live cancellation, timeout and missing dependency passed\n";
}
