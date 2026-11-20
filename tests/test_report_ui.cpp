// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/report_dialog.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <sys/stat.h>
int main(int argc,char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    tux::Result first; first.action=tux::action("storage").title; first.code=0; first.seconds=0.2;
    first.command=QStringList{"fixture","private-one"}; first.output="private-output-one";
    tux::Result second=first; second.action=tux::action("memory").title; second.cancelled=true; second.code=130;
    second.command=QStringList{"fixture","private-two"}; second.output="private-output-two";
    QVector<tux::Result> input={first,second};
    tux::ReportDialog report(input); report.show(); app.processEvents();
    input[1].output="external-change";
    auto* tasks=report.findChild<QTableWidget*>("reportTasks");
    auto* details=report.findChild<QCheckBox*>("reportDetails");
    auto* preview=report.findChild<QPlainTextEdit*>("reportPreview");
    auto* save=report.findChild<QPushButton*>("reportSave");
    auto records=[&]{return QJsonDocument::fromJson(preview->toPlainText().toUtf8()).object().value("results").toArray();};
    if(tasks->rowCount()!=2 || tasks->item(0,0)->text()!=second.action || tasks->item(0,1)->text()!="Cancelled" || details->isChecked() || !save->isEnabled()) return 1;
    if(records().size()!=2 || records().first().toObject().value("action").toString()!=first.action || preview->toPlainText().contains("private-")) return 2;
    report.findChild<QPushButton*>("reportNone")->click();
    if(save->isEnabled() || records().size()!=0 || !report.findChild<QLabel*>("reportCount")->text().startsWith("0 of 2")) return 3;
    tasks->item(0,0)->setCheckState(Qt::Checked);
    if(records().size()!=1 || !records().first().toObject().value("cancelled").toBool() || preview->toPlainText().contains(first.action)) return 4;
    details->setChecked(true);
    if(!preview->toPlainText().contains("private-output-two") || preview->toPlainText().contains("private-output-one") || preview->toPlainText().contains("external-change")) return 5;
    details->setChecked(false);
    const auto reviewed=preview->toPlainText().toUtf8();
    if(reviewed.contains("private-") || reviewed.contains("command") || reviewed.contains("output")) return 6;
    if(argc>1 && !report.grab().save(QString::fromLocal8Bit(argv[1]))) return 7;
    QTemporaryDir temp; if(!temp.isValid()) return 8;
    const auto path=temp.filePath("selected.json");
    // Cancelling the file picker leaves review open and creates no report.
    QTimer::singleShot(0,[&]{for(auto* w:QApplication::topLevelWidgets()) if(auto* f=qobject_cast<QFileDialog*>(w)) f->reject();});
    save->click();
    if(report.result()!=0 || QFile::exists(path)) return 9;
    QTimer::singleShot(0,[&]{for(auto* w:QApplication::topLevelWidgets()) if(auto* f=qobject_cast<QFileDialog*>(w)) {f->selectFile(path);QMetaObject::invokeMethod(f,"accept",Qt::DirectConnection);}});
    save->click(); QFile file(path);
    if(report.result()!=QDialog::Accepted || !file.open(QIODevice::ReadOnly) || file.readAll()!=reviewed) return 10;
    struct stat metadata{};
    if(stat(path.toLocal8Bit().constData(),&metadata)!=0 || (metadata.st_mode&0777)!=0600) return 15;
    file.close();
    // An existing permissive report is replaced privately, even with a broad umask.
    QFile::setPermissions(path,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ReadGroup|QFileDevice::ReadOther);
    tux::ReportDialog overwrite({first});
    const auto previousMask=umask(0);
    QTimer::singleShot(0,[&]{for(auto* w:QApplication::topLevelWidgets()) if(auto* f=qobject_cast<QFileDialog*>(w)) {
        f->setOption(QFileDialog::DontConfirmOverwrite); f->selectFile(path); QMetaObject::invokeMethod(f,"accept",Qt::DirectConnection);
    }});
    overwrite.findChild<QPushButton*>("reportSave")->click(); umask(previousMask);
    if(overwrite.result()!=QDialog::Accepted || stat(path.toLocal8Bit().constData(),&metadata)!=0 || (metadata.st_mode&0777)!=0600) return 16;
    tux::ReportDialog fresh({first,second});
    if(fresh.findChild<QCheckBox*>("reportDetails")->isChecked() || fresh.findChild<QTableWidget*>("reportTasks")->item(1,0)->checkState()!=Qt::Checked) return 11;
    fresh.findChild<QPushButton*>("reportNone")->click(); fresh.findChild<QPushButton*>("reportAll")->click();
    if(!fresh.findChild<QPushButton*>("reportSave")->isEnabled() || fresh.findChild<QTableWidget*>("reportTasks")->item(0,0)->checkState()!=Qt::Checked) return 12;
    QVector<tux::Result> many(25,first); many[0].action="excluded oldest";
    tux::ReportDialog bounded(many);
    if(bounded.findChild<QTableWidget*>("reportTasks")->rowCount()!=20 || bounded.findChild<QPlainTextEdit*>("reportPreview")->toPlainText().contains("excluded oldest")) return 13;
    tux::ReportDialog empty({});
    if(empty.findChild<QPushButton*>("reportSave")->isEnabled() || empty.findChild<QPushButton*>("reportAll")->isEnabled() || !empty.findChild<QLabel*>("reportCount")->text().contains("No completed")) return 14;
    std::cout<<"Report selection: exact subset, chronological ordering, summary allowlist, per-export consent, snapshot bounds, empty state, picker cancellation and byte-exact saved preview passed\n";
}

