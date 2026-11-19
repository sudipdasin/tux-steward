// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/window.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    QApplication app(argc,argv); QTemporaryDir temp; if(!temp.isValid()) return 1;
    const auto oldPath=qgetenv("PATH");
    auto fixture=[&](const QString& name,const QByteArray& body) {
        QFile file(temp.filePath(name));
        if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate) || file.write("#!/bin/sh\n"+body)<0) return false;
        file.close(); return file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    };
    if(!fixture("df","printf 'fixture diagnostic running\\n'\nexec /bin/sleep 30\n") ||
       !fixture("xfconf-query","printf 'fixture mutation running\\n'\n/bin/sleep 0.3\nprintf 'fixture mutation completed\\n'\n")) return 2;
    qputenv("PATH",temp.path().toUtf8()+":"+oldPath);
    tux::Window window; window.show(); app.processEvents();
    auto* status=window.findChild<QLabel*>("taskStatus");
    auto* output=window.findChild<QPlainTextEdit*>("sessionOutput");
    auto* stop=window.findChild<QPushButton*>("stopDiagnostic");
    auto wait=[&](auto condition,int ms) { QElapsedTimer timer; timer.start(); while(!condition() && timer.elapsed()<ms) {app.processEvents();std::this_thread::sleep_for(std::chrono::milliseconds(10));} return condition(); };
    auto review=[&](const QString& id) {
        QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets()) if(auto* box=qobject_cast<QMessageBox*>(widget)) box->done(QMessageBox::Ok);});
        window.findChild<QPushButton*>("tool_"+id)->click();
    };
    if(!stop || stop->isEnabled()) return 3;
    review("storage");
    if(!wait([&]{return output->toPlainText().contains("fixture diagnostic running");},3000) || !stop->isEnabled()) return 4;
    stop->click();
    if(!wait([&]{return status->text().startsWith("Cancelled");},3000) || stop->isEnabled() || window.findChild<QPushButton*>("sessionHistory")->text()!="History (1)") return 5;
    if(argc>1 && !window.grab().save(QString::fromLocal8Bit(argv[1]))) return 6;
    if(!fixture("df","printf 'fixture diagnostic completed\\n'\n")) return 7;
    review("storage");
    if(!wait([&]{return status->text().startsWith("Completed");},3000) || !output->toPlainText().contains("fixture diagnostic completed")) return 8;
    review("compositor-on");
    if(!wait([&]{return output->toPlainText().contains("fixture mutation running");},3000) || stop->isEnabled()) return 9;
    if(!QMetaObject::invokeMethod(stop,"clicked",Qt::DirectConnection,Q_ARG(bool,false))) return 11;
    if(!wait([&]{return status->text().startsWith("Completed");},3000) || !output->toPlainText().contains("fixture mutation completed")) return 10;
    window.close(); qputenv("PATH",oldPath);
    std::cout<<"Read-only stop, cancelled UI/history, next-task token reset and protected mutation controls passed with isolated helpers\n";
}
