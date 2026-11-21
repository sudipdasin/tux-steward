// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/health_dialog.hpp"
#include "tux/window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>
int main(int argc,char** argv) {
    const auto name=QFileInfo(QString::fromLocal8Bit(argv[0])).fileName();
    if(QStringList{"df","free","lsblk","systemctl","journalctl"}.contains(name)) {
        QFile marker(qEnvironmentVariable("TUX_HEALTH_MARKER")); if(marker.open(QIODevice::Append)) {marker.write(name.toUtf8()+"\n");marker.close();}
        std::cout<<"synthetic-private-health-output\n";
        if(name=="df" && qEnvironmentVariableIsSet("TUX_HEALTH_SLOW")) sleep(3);
        return name=="free" && qEnvironmentVariableIsSet("TUX_HEALTH_FAIL")?42:0;
    }
    QApplication app(argc,argv); QTemporaryDir temp; if(!temp.isValid()) return 1;
    for(const auto& helper:QStringList{"df","free","lsblk","systemctl","journalctl"}) if(!QFile::copy(app.applicationFilePath(),temp.filePath(helper))) return 2;
    const auto oldPath=qgetenv("PATH"); qputenv("PATH",temp.path().toUtf8());
    auto settle=[&](auto condition,int limit=5000) {QElapsedTimer timer;timer.start();while(!condition() && timer.elapsed()<limit) {app.processEvents();std::this_thread::sleep_for(std::chrono::milliseconds(10));}return condition();};
    tux::HealthDialog review(nullptr,1000); review.show(); app.processEvents();
    auto* tasks=review.findChild<QTableWidget*>("healthTasks"); auto* start=review.findChild<QPushButton*>("healthRun");
    if(tasks->rowCount()!=6 || !start->isEnabled() || review.findChild<QPushButton*>("healthStop")->isEnabled() || !review.results().isEmpty()) return 3;
    if(!review.findChild<QPlainTextEdit*>("healthCommands")->toPlainText().contains("'df' '-hT'") || review.findChild<QPlainTextEdit*>("healthCommands")->toPlainText().contains("journalctl")) return 4;
    for(int row=0;row<6;++row) tasks->item(row,0)->setCheckState(Qt::Unchecked);
    if(start->isEnabled() || !review.findChild<QLabel*>("healthStatus")->text().contains("at least one")) return 5;
    for(int row=0;row<4;++row) tasks->item(row,0)->setCheckState(Qt::Checked);
    review.reject(); if(!review.results().isEmpty()) return 6;
    tux::HealthDialog run(nullptr,1000); run.show(); run.findChild<QPushButton*>("healthRun")->click();
    if(!settle([&]{return run.findChild<QPushButton*>("healthExport")->isEnabled();})) return 7;
    if(run.results().size()!=4 || run.findChild<QProgressBar*>("healthProgress")->value()!=4 || !run.findChild<QLabel*>("healthStatus")->text().startsWith("Finished") || run.findChild<QPushButton*>("healthStop")->isEnabled()) return 8;
    bool reportReviewed=false;
    QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets()) if(widget->objectName()=="reportReview") {
        const auto summary=widget->findChild<QPlainTextEdit*>("reportPreview")->toPlainText();
        reportReviewed=summary.contains("summary") && !summary.contains("synthetic-private-health-output") && !widget->findChild<QCheckBox*>("reportDetails")->isChecked();
        qobject_cast<QDialog*>(widget)->reject();
    }});
    run.findChild<QPushButton*>("healthExport")->click(); if(!reportReviewed) return 9;
    if(argc>1 && !run.grab().save(QString::fromLocal8Bit(argv[1]))) return 10;
    run.reject(); qputenv("TUX_HEALTH_SLOW","1"); const auto marker=temp.filePath("active"); qputenv("TUX_HEALTH_MARKER",marker.toUtf8());
    tux::HealthDialog stopped(nullptr,1000); stopped.show(); stopped.findChild<QPushButton*>("healthRun")->click();
    if(!settle([&]{return QFile::exists(marker);})) return 11;
    stopped.reject(); if(!stopped.isVisible() || stopped.findChild<QPushButton*>("healthStop")->isEnabled()) return 12;
    if(!settle([&]{return stopped.findChild<QLabel*>("healthStatus")->text().startsWith("Stopped ·");}) || stopped.results().size()!=1 || !stopped.results().first().cancelled) return 13;
    stopped.reject();
    tux::HealthDialog timed(nullptr,100); timed.show(); timed.findChild<QPushButton*>("healthRun")->click();
    if(!settle([&]{return timed.findChild<QPushButton*>("healthExport")->isEnabled();}) || timed.results().size()!=1 || timed.results().first().code!=124 || !timed.findChild<QLabel*>("healthStatus")->text().startsWith("Stopped after failure")) return 14;
    timed.reject(); qunsetenv("TUX_HEALTH_SLOW"); qunsetenv("TUX_HEALTH_MARKER"); qputenv("TUX_HEALTH_FAIL","1");
    tux::HealthDialog continued(nullptr,1000); continued.findChild<QCheckBox*>("healthStopOnFailure")->setChecked(false); continued.findChild<QPushButton*>("healthRun")->click();
    if(!settle([&]{return continued.findChild<QPushButton*>("healthExport")->isEnabled();}) || continued.results().size()!=4 || continued.results()[1].code!=42) return 15;
    qunsetenv("TUX_HEALTH_FAIL"); QTemporaryDir empty; qputenv("PATH",empty.path().toUtf8()); tux::HealthDialog missing;
    if(missing.findChild<QPushButton*>("healthRun")->isEnabled() || !missing.findChild<QLabel*>("healthStatus")->text().contains("unavailable")) return 16;
    qputenv("PATH",temp.path().toUtf8());
    tux::Window window; window.show(); QTimer driver; int ticks=0; bool imported=false;
    QObject::connect(&driver,&QTimer::timeout,[&]{
        for(auto* widget:QApplication::topLevelWidgets()) if(widget->objectName()=="healthCheckDialog" && widget->isVisible()) {
            const auto state=widget->findChild<QLabel*>("healthStatus")->text();
            if(state.startsWith("Finished")) {imported=true; driver.stop(); qobject_cast<QDialog*>(widget)->reject();}
            else if(widget->findChild<QPushButton*>("healthRun")->isEnabled()) widget->findChild<QPushButton*>("healthRun")->click();
            else if(++ticks>500) {driver.stop(); qobject_cast<QDialog*>(widget)->reject();}
        }
    });
    driver.start(10); window.findChild<QPushButton*>("healthCheck")->click(); driver.stop();
    if(!imported || window.findChild<QPushButton*>("sessionHistory")->text()!="History (4)") return 17;
    window.close();
    qputenv("PATH",oldPath);
    std::cout<<"Health workspace: review without execution, selection, progress, private report review, cancellation/close guard, timeout, continue policy and missing dependency passed\n";
}
