// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/async_command.hpp"
#include "tux/system_monitor_tabs.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
bool spin(const std::function<bool()> &done, int deadlineMs = 5000) {
    QElapsedTimer clock; clock.start();
    while (!done() && clock.elapsed() < deadlineMs) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return done();
}
void settle(int ms) {
    QElapsedTimer clock; clock.start();
    spin([&] { return clock.elapsed() >= ms; }, ms + 100);
}
int fixture(int argc, char **argv) {
    if (argc > 2 && QString::fromLocal8Bit(argv[1]) == "--echo") {
        std::fputs(argv[2], stdout); return 0;
    }
    bool user = false;
    for (int i=1;i<argc;++i) if (QString::fromLocal8Bit(argv[i]) == "--user") user=true;
    QFile calls(qEnvironmentVariable("TUX_SERVICE_TEST_CALLS"));
    if (calls.open(QIODevice::WriteOnly | QIODevice::Append)) calls.write(user ? "user\n" : "system\n");
    const auto mode=qgetenv("TUX_SERVICE_TEST_MODE");
    if (mode=="slow") QThread::msleep(600);
    if (mode=="switch") QThread::msleep(user ? 40 : 800);
    if (mode=="timeout") QThread::msleep(10000);
    if (mode=="failure") { std::fputs("synthetic query failure\n",stderr); return 7; }
    if (mode=="flood") {
        const QByteArray bytes(4096,'x');
        for (int i=0;i<200;++i) std::fwrite(bytes.constData(),1,bytes.size(),stdout);
        return 0;
    }
    std::fputs(mode == "same" ? "shared-demo.service enabled\n"
        : user ? "user-demo.service enabled\n" : "system-demo.service enabled\n",stdout);
    return 0;
}
}
int main(int argc, char **argv) {
    if (QFileInfo(QString::fromLocal8Bit(argv[0])).fileName()=="systemctl") return fixture(argc,argv);
    QApplication app(argc,argv);
    QTemporaryDir temp;
    const auto oldPath=qgetenv("PATH");
    try {
        require(temp.isValid(),"fixture directory");
        const auto program=temp.filePath("systemctl");
        require(QFile::link(QCoreApplication::applicationFilePath(),program),"fake service executable");
        const auto callsPath=temp.filePath("calls.txt");
        qputenv("TUX_SERVICE_TEST_CALLS",callsPath.toUtf8());
        auto query=[&](const QString &mode,int timeout=1500,int limit=65536) {
            qputenv("TUX_SERVICE_TEST_MODE",mode.toUtf8());
            QObject owner; bool done=false; AsyncCommandResult result; int completions=0;
            AsyncCommand::run(&owner,program,{},timeout,limit,[&](AsyncCommandResult value) {
                result=std::move(value); done=true; ++completions;
            });
            require(spin([&]{return done;}),"query completion deadline");
            settle(20);
            require(completions==1,"completion delivered exactly once");
            return result;
        };
        int beats=0; QTimer heartbeat; heartbeat.setInterval(20);
        QObject::connect(&heartbeat,&QTimer::timeout,&app,[&]{++beats;}); heartbeat.start();
        auto result=query("slow");
        require(result.ok() && beats>=5,"slow query keeps Qt event loop responsive");
        result=query("failure"); require(result.exitCode==7 && result.output.contains("synthetic"),"query failure preserved");
        result=query("flood",1500,1024); require(result.truncated && result.output.size()==1024 && !result.ok(),"output bounded during capture");
        result=query("timeout",100); require(result.timedOut && result.exitCode==124,"query timeout");
        {
            QObject owner; bool done=false;
            AsyncCommand::run(&owner,temp.filePath("missing"),{},1000,1024,[&](AsyncCommandResult value) {
                require(value.exitCode==127 && !value.ok(),"failed-start status"); done=true;
            });
            require(spin([&]{return done;}),"failed-start completion");
        }
        {
            QObject owner; int callbacks=0; qputenv("TUX_SERVICE_TEST_MODE","slow");
            auto *running=AsyncCommand::run(&owner,program,{},1500,1024,[&](AsyncCommandResult value) {
                require(value.cancelled && value.exitCode==125,"cancelled status"); ++callbacks;
            });
            running->cancel(); settle(40); require(callbacks==1,"cancel completion once");
        }
        {
            int callbacks=0; auto *owner=new QObject;
            AsyncCommand::run(owner,program,{},1500,1024,[&](AsyncCommandResult){++callbacks;});
            delete owner; settle(40); require(callbacks==0,"no completion after owner destruction");
        }
        {
            QObject owner; bool done=false;
            AsyncCommand::run(&owner,program,{"--echo","$(touch must-not-run); literal"},1000,1024,[&](AsyncCommandResult value) {
                require(value.output=="$(touch must-not-run); literal","literal arguments preserved"); done=true;
            });
            require(spin([&]{return done;}),"literal command completion");
        }
        qputenv("PATH",temp.path().toUtf8());
        qputenv("TUX_SERVICE_TEST_MODE","slow");
        QTabWidget tabs;
        new SystemMonitorTabs(&tabs);
        tabs.resize(1000,700); tabs.show(); app.processEvents();
        auto *scope=tabs.findChild<QComboBox *>("serviceScopeFilter");
        auto *selection=tabs.findChild<QComboBox *>("serviceSelection");
        auto *report=tabs.findChild<QPlainTextEdit *>("serviceReport");
        auto *start=tabs.findChild<QPushButton *>("serviceAction_start");
        require(scope && selection && report && start,"service integration controls");
        int services=-1;
        for (int i=0;i<tabs.count();++i) if (tabs.tabText(i)=="Services") services=i;
        require(services>=0,"services tab exists");
        QFile calls(callsPath); calls.open(QIODevice::WriteOnly | QIODevice::Truncate); calls.close();
        QElapsedTimer navigation; navigation.start(); tabs.setCurrentIndex(services);
        require(navigation.elapsed()<250,"service tab navigation does not wait for systemctl");
        require(!start->isEnabled() && !selection->isEnabled(),"actions disabled while query is pending");
        require(spin([&]{return selection->count()>0;}),"service query populates choice list");
        require(report->toPlainText().contains("system-demo.service") && start->isEnabled(),"one result powers report and actions");
        require(calls.open(QIODevice::ReadOnly) && calls.readAll()=="system\n","no duplicate service query");
        calls.close();
        // A superseded system-scope response must not overwrite the newer user list.
        qputenv("TUX_SERVICE_TEST_MODE","switch");
        scope->setCurrentIndex(0);
        scope->setCurrentIndex(2);
        require(spin([&]{return selection->isEnabled();}),"replacement query completes");
        settle(900);
        require(selection->count()==1 && selection->currentData().toMap().value("user").toBool()
            && report->toPlainText().contains("user-demo.service")
            && !report->toPlainText().contains("system-demo.service"),"stale scope results ignored");
        // Cancellation on navigation stops applying pending data; re-entering retries.
        qputenv("TUX_SERVICE_TEST_MODE","slow");
        scope->setCurrentIndex(1);
        tabs.setCurrentIndex(0); settle(700);
        require(selection->count()==0 && !start->isEnabled(),"leaving services cancels pending query");
        tabs.setCurrentIndex(services);
        require(spin([&]{return selection->count()==1;}),"re-entering services refreshes cancelled query");
        // Equal unit names in different scopes are represented by complete maps,
        // and existing selection is restored without comparing a map to a string.
        qputenv("TUX_SERVICE_TEST_MODE","same");
        scope->setCurrentIndex(0);
        require(spin([&]{return selection->count()==2 && selection->isEnabled();}),"all-scope query");
        for (int i=0;i<selection->count();++i)
            if (selection->itemData(i).toMap().value("user").toBool()) selection->setCurrentIndex(i);
        const auto selectedScope=selection->currentData().toMap();
        QMetaObject::invokeMethod(scope,"currentIndexChanged",Qt::DirectConnection,Q_ARG(int,0));
        require(spin([&]{return selection->count()==2 && selection->isEnabled();}),"same-scope refresh");
        require(selection->currentData().toMap()==selectedScope,"selection preserves unit and scope identity");
        qputenv("TUX_SERVICE_TEST_MODE","flood");
        scope->setCurrentIndex(2);
        require(spin([&]{return report->toPlainText().contains("512 KiB");}),"oversized service output reported");
        require(selection->count()==0 && !start->isEnabled(),"truncated results cannot drive actions");
        tabs.hide(); heartbeat.stop();
        qputenv("PATH",oldPath);
        std::cout<<"Async query lifecycle, responsiveness, bounds, scope cancellation and shared service results passed\n";
        return 0;
    } catch (const std::exception &error) {
        qputenv("PATH",oldPath);
        std::cerr<<"FAILED: "<<error.what()<<'\n'; return 1;
    }
}
