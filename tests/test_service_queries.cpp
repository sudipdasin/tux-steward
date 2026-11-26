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
    if(mode=="hold-action") {
        QFile ready(qEnvironmentVariable("TUX_SERVICE_TEST_READY"));
        if(!ready.open(QIODevice::WriteOnly)||ready.write("ready")!=5)return 91;
        ready.close();QElapsedTimer deadline;deadline.start();
        while(!QFileInfo::exists(qEnvironmentVariable("TUX_SERVICE_TEST_RELEASE"))&&deadline.elapsed()<10000)QThread::msleep(5);
        if(!QFileInfo::exists(qEnvironmentVariable("TUX_SERVICE_TEST_RELEASE")))return 92;
    }
    if (mode=="slow") QThread::msleep(600);
    if (mode=="switch") QThread::msleep(user ? 40 : 800);
    if (mode=="timeout") QThread::msleep(10000);
    if (mode=="failure") { std::fputs("synthetic query failure\n",stderr); return 7; }
    if (mode=="flood") {
        const QByteArray bytes(4096,'x');
        for (int i=0;i<200;++i) std::fwrite(bytes.constData(),1,bytes.size(),stdout);
        return 0;
    }
    bool runtime=false;for(int i=1;i<argc;++i)if(QString::fromLocal8Bit(argv[i])=="list-units")runtime=true;
    if(runtime){std::fputs(mode=="same"?"shared-demo.service loaded active running Synthetic\n":user?"user-demo.service loaded active running Synthetic\n":"system-demo.service loaded active running Synthetic\n",stdout);return 0;}
    std::fputs(mode == "same" ? "shared-demo.service enabled\n"
        : user ? "user-demo.service enabled\n" : "system-demo.service enabled\n",stdout);
    return 0;
}
}
int main(int argc, char **argv) {
    if (QFileInfo(QString::fromLocal8Bit(argv[0])).fileName()=="systemctl") return fixture(argc,argv);
    if (QFileInfo(QString::fromLocal8Bit(argv[0])).fileName()=="pkexec") {
        QFile calls(qEnvironmentVariable("TUX_SERVICE_TEST_CALLS"));
        if (calls.open(QIODevice::WriteOnly | QIODevice::Append)) calls.write("unexpected-admin\n");
        return 93;
    }
    QApplication app(argc,argv);
    QTemporaryDir temp;
    const auto oldPath=qgetenv("PATH");
    try {
        require(temp.isValid(),"fixture directory");
        const auto program=temp.filePath("systemctl");
        require(QFile::link(QCoreApplication::applicationFilePath(),program),"fake service executable");
        const auto authorization=temp.filePath("pkexec");
        require(QFile::link(QCoreApplication::applicationFilePath(),authorization),"fake authorization executable");
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
            QObject owner; bool done=false; int completions=0;
            qputenv("TUX_SERVICE_TEST_MODE","hold-action");
            const auto ready=temp.filePath("action-ready");
            const auto release=temp.filePath("action-release");
            qputenv("TUX_SERVICE_TEST_READY",ready.toUtf8());qputenv("TUX_SERVICE_TEST_RELEASE",release.toUtf8());
            auto* action=AsyncCommand::runAction(&owner,program,{"--user"},1024,[&](AsyncCommandResult value){
                require(value.ok()&&!value.cancelled&&!value.timedOut,"reviewed action reports its actual process result");
                ++completions;done=true;
            });
            require(spin([&]{return QFileInfo::exists(ready);}),"reviewed action fixture reaches its release gate");
            action->cancel();QCoreApplication::processEvents();
            require(!done&&action->active(),"query cancellation cannot release a live reviewed action");
            QFile gate(release);require(gate.open(QIODevice::WriteOnly)&&gate.write("finish")==6,"release the synthetic action only after lifetime assertions");gate.close();
            require(spin([&]{return done;}),"reviewed action finishes without blocking event processing");
            settle(20);require(completions==1,"reviewed action completion delivered exactly once");
            qunsetenv("TUX_SERVICE_TEST_READY");qunsetenv("TUX_SERVICE_TEST_RELEASE");
        }
        {
            QObject owner; bool done=false;
            AsyncCommand::run(&owner,program,{"--echo","$(touch must-not-run); literal"},1000,1024,[&](AsyncCommandResult value) {
                require(value.output=="$(touch must-not-run); literal","literal arguments preserved"); done=true;
            });
            require(spin([&]{return done;}),"literal command completion");
        }
        qputenv("PATH",temp.path().toUtf8());
        {
            QFile calls(callsPath);
            require(calls.open(QIODevice::WriteOnly | QIODevice::Truncate),"reset command-plan execution marker");
            calls.close();
            require(QStandardPaths::findExecutable("systemctl")==program
                && QStandardPaths::findExecutable("pkexec")==authorization,"PATH fixtures shadow both administrative programs");
            const auto userCommand=SystemMonitorTabs::serviceActionCommand(true,"restart","user-demo.service");
            require(userCommand==QStringList{program,"--user","restart","--","user-demo.service"},"user-scope plan preserves scoped literal arguments and session PATH");
            const auto systemCommand=SystemMonitorTabs::serviceActionCommand(false,"restart","system-demo.service");
            const auto trustedControl=tux::trustedSystemExecutable("systemctl");
            const auto trustedAuthorization=tux::trustedSystemExecutable("pkexec");
            if(trustedControl.isEmpty()||trustedAuthorization.isEmpty())
                require(systemCommand.isEmpty(),"missing trusted privileged helpers rejects the system command plan");
            else
                require(systemCommand==QStringList{trustedAuthorization,trustedControl,"restart","--","system-demo.service"},"system plan uses only protected absolute programs");
            require(!systemCommand.contains(program)&&!systemCommand.contains(authorization),"untrusted PATH helpers never enter a privileged command plan");
            require(SystemMonitorTabs::serviceActionCommand(false,"daemon-reexec","system-demo.service").isEmpty()
                &&SystemMonitorTabs::serviceActionCommand(true,"start","invalid-unit").isEmpty(),"command planning rejects unsupported actions and targets");
            for(const auto& invalidUnit:QStringList{"/tmp/evil.service","bad\n.service","*.service","-option.service",QString(256,'a')+".service"})
                require(SystemMonitorTabs::serviceActionCommand(false,"enable",invalidUnit).isEmpty()
                    &&SystemMonitorTabs::serviceActionCommand(true,"enable",invalidUnit).isEmpty(),"service plans reject absolute paths, whitespace, globs, option prefixes and oversized unit names");
            require(SystemMonitorTabs::serviceActionCommand(true,"start","worker@demo\\x2djob.service")
                ==QStringList{program,"--user","start","--","worker@demo\\x2djob.service"},"literal systemd escaped template names remain supported");
            require(calls.open(QIODevice::ReadOnly)&&calls.readAll().isEmpty(),"command-plan inspection never invokes real or fixture administrative helpers");
        }
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
        int services=-1;for(int i=0;i<tabs.count();++i)if(tabs.tabText(i)=="Services")services=i;
        require(services>=0,"first-class Services tab exists");
        QFile calls(callsPath); calls.open(QIODevice::WriteOnly | QIODevice::Truncate); calls.close();
        QElapsedTimer navigation; navigation.start(); tabs.setCurrentIndex(services);
        require(navigation.elapsed()<250,"service tab navigation does not wait for systemctl");
        require(!start->isEnabled() && !selection->isEnabled(),"actions disabled while query is pending");
        require(spin([&]{return selection->count()>0;}),"service query populates choice list");
        require(report->toPlainText().contains("system-demo.service") && start->isEnabled(),"one result powers report and actions");
        require(calls.open(QIODevice::ReadOnly) && calls.readAll()=="system\nsystem\n","one file-state and one runtime query");
        calls.close();
        require(calls.open(QIODevice::WriteOnly | QIODevice::Truncate),"reset cancelled system review marker");calls.close();
        bool cancelledSystemReview=false;
        QTimer::singleShot(0,[&] {
            if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                cancelledSystemReview=box->text().contains("whole system");
                box->button(QMessageBox::Cancel)->click();
            }
        });
        start->click();
        require(cancelledSystemReview&&start->isEnabled(),"system-scope action is reviewable and cancellation preserves idle controls");
        require(calls.open(QIODevice::ReadOnly)&&calls.readAll().isEmpty(),"cancelled system review executes neither service nor authorization helpers");calls.close();
        auto* serviceTable=tabs.findChild<QTableWidget*>("serviceTable");auto* search=tabs.findChild<QLineEdit*>("serviceSearch");auto* restart=tabs.findChild<QPushButton*>("serviceAction_restart");
        require(serviceTable&&search&&restart&&restart->isEnabled(),"service table/search/restart controls");
        require(serviceTable->rowCount()==1&&serviceTable->item(0,3)->text()=="active"&&serviceTable->item(0,4)->text()=="running","runtime and startup states kept distinct");
        search->setText("no matching service");require(!restart->isEnabled()&&serviceTable->isRowHidden(0),"no-match disables mutations");search->clear();require(restart->isEnabled(),"matching service restores actions");
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
        require(calls.open(QIODevice::ReadOnly) && calls.readAll()=="user\nuser\nuser\n","one mutation plus file-state/runtime refresh"); calls.close();
        calls.open(QIODevice::WriteOnly|QIODevice::Truncate);calls.close();
        QTimer::singleShot(0,[]{if(auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->button(QMessageBox::Yes)->click();});
        restart->click();require(spin([&]{return restart->isEnabled();}),"restart completes and refreshes");
        require(calls.open(QIODevice::ReadOnly)&&calls.readAll()=="user\nuser\nuser\n","one restart plus two read-only queries");calls.close();
        if(argc>1)require(window.grab().save(QString::fromLocal8Bit(argv[1])),"service table screenshot");
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
        std::cout<<"Async query lifecycle, responsiveness, bounds, scope cancellation, trusted privileged planning and shared service results passed\n";
        return 0;
    } catch (const std::exception &error) {
        qputenv("PATH",oldPath);
        std::cerr<<"FAILED: "<<error.what()<<'\n'; return 1;
    }
}

