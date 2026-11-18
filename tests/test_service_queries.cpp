// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/async_command.hpp"
#include "tux/system_monitor_tabs.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QThread>
#include <QMainWindow>
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
        QMainWindow window;
        QTabWidget tabs(&window);
        window.setCentralWidget(&tabs);
        new SystemMonitorTabs(&tabs);
        window.resize(1000,700); window.show(); app.processEvents();
        auto *scope=tabs.findChild<QComboBox *>("serviceScopeFilter");
        auto *selection=tabs.findChild<QComboBox *>("serviceSelection");
        auto *report=tabs.findChild<QPlainTextEdit *>("serviceReport");
        auto *start=tabs.findChild<QPushButton *>("serviceAction_start");
        require(scope && selection && report && start,"service integration controls");
        auto *experimental=tabs.findChild<QTabWidget *>("experimentalResourceTabs");
        require(experimental!=nullptr,"experimental group exists");
        int services=tabs.indexOf(experimental);
        int servicePage=-1;
        for (int i=0;i<experimental->count();++i) if (experimental->tabText(i)=="Services") servicePage=i;
        require(servicePage>=0,"experimental services page exists"); experimental->setCurrentIndex(servicePage);
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
        scope->setCurrentIndex(2);
        require(spin([&]{return selection->count()==1 && selection->isEnabled();}),"user-scope mutation fixture");
        qputenv("TUX_SERVICE_TEST_MODE","slow");
        calls.open(QIODevice::WriteOnly|QIODevice::Truncate); calls.close();
        QTimer::singleShot(0,[] {
            if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click();
        });
        start->click();
        require(!start->isEnabled() && !selection->isEnabled(),"service mutation is single-flight");
        bool closeWarningSeen=false;
        QTimer::singleShot(0,[&] {
            if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                closeWarningSeen=box->windowTitle()=="Service action still running";
                box->button(QMessageBox::Ok)->click();
            }
        });
        require(!window.close() && closeWarningSeen && window.isVisible(),"window close refused while service action is running");
        start->click(); // Disabled controls cannot enqueue a second mutation.
        require(spin([&]{return start->isEnabled();}),"mutation completion refreshes list");
        require(calls.open(QIODevice::ReadOnly) && calls.readAll()=="user\nuser\n","one mutation plus one refresh, without duplicate action"); calls.close();
        require(QFile::remove(program),"remove only fake executable symlink");
        QFile invalid(program);
        require(invalid.open(QIODevice::WriteOnly),"failed-start executable fixture");
        invalid.write("#!/nonexistent-synthetic-service-interpreter\n"); invalid.close();
        require(invalid.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner),"fixture executable permissions");
        QTimer::singleShot(0,[] {
            if (auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->button(QMessageBox::Yes)->click();
        });
        start->click();
        auto *actionStatus=tabs.findChild<QLabel *>("serviceActionStatus");
        require(actionStatus && spin([&]{return actionStatus->text().contains("Could not");}),"failed service start leaves waiting state");
        require(spin([&]{return report->toPlainText().contains("Query failed");}),"failed-start refresh reported");
        require(QFile::remove(program) && QFile::link(QCoreApplication::applicationFilePath(),program),"restore fake executable");
        qputenv("TUX_SERVICE_TEST_MODE","same");
        scope->setCurrentIndex(0);
        require(spin([&]{return selection->count()==2 && selection->isEnabled();}),"scope refreshed after command failure");
        qputenv("TUX_SERVICE_TEST_MODE","flood");
        scope->setCurrentIndex(2);
        require(spin([&]{return report->toPlainText().contains("512 KiB");}),"oversized service output reported");
        require(selection->count()==0 && !start->isEnabled(),"truncated results cannot drive actions");
        require(window.close() && !window.isVisible(),"window closes after service action completion and failed-start recovery");
        heartbeat.stop();
        qputenv("PATH",oldPath);
        std::cout<<"Async query lifecycle, responsiveness, bounds, scope cancellation and shared service results passed\n";
        return 0;
    } catch (const std::exception &error) {
        qputenv("PATH",oldPath);
        std::cerr<<"FAILED: "<<error.what()<<'\n'; return 1;
    }
}

