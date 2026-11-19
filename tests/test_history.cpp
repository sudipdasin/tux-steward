// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/history_dialog.hpp"
#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <iostream>
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    tux::HistoryDialog empty({},false);
    if(empty.findChild<QTableWidget*>("historyTable")->rowCount()!=0 ||
       empty.findChild<QPushButton*>("historyRerun")->isEnabled() ||
       !empty.findChild<QPlainTextEdit*>("historyDetails")->toPlainText().contains("No completed")) return 1;
    tux::Result storage; storage.action=tux::action("storage").title; storage.command=tux::action("storage").command;
    storage.output="Synthetic local details, not uploaded."; storage.code=0; storage.seconds=0.25;
    tux::Result failed=storage; failed.action=tux::action("memory").title; failed.command=tux::action("memory").command;
    failed.code=124; failed.truncated=true; failed.output="Synthetic timeout tail";
    QVector<tux::Result> input={storage,failed};
    tux::HistoryDialog history(input,false); history.show(); app.processEvents();
    input[1].output="external change";
    auto* table=history.findChild<QTableWidget*>("historyTable");
    auto* details=history.findChild<QPlainTextEdit*>("historyDetails");
    auto* search=history.findChild<QLineEdit*>("historySearch");
    auto* rerun=history.findChild<QPushButton*>("historyRerun");
    if(table->rowCount()!=2 || table->item(0,0)->text()!=failed.action || history.selectedTool()!="memory" ||
       !rerun->isEnabled() || !details->toPlainText().contains("Synthetic timeout tail") ||
       !details->toPlainText().contains("Output truncated") || details->toPlainText().contains("external change")) return 2;
    search->setText("STORAGE"); app.processEvents();
    if(!table->isRowHidden(0) || table->isRowHidden(1) || history.selectedTool()!="storage") return 3;
    search->setText("no such task"); app.processEvents();
    if(rerun->isEnabled() || !history.selectedTool().isEmpty() || !details->toPlainText().contains("No matching")) return 4;
    search->clear(); table->selectRow(1); app.processEvents();
    // Selection and double-click do not accept the dialog or run any process.
    QMetaObject::invokeMethod(table,"cellDoubleClicked",Qt::DirectConnection,Q_ARG(int,1),Q_ARG(int,0));
    if(history.result()!=0 || history.selectedTool()!="storage") return 5;
    if(argc>1 && !history.grab().save(QString::fromLocal8Bit(argv[1]))) return 6;
    rerun->click(); if(history.result()!=QDialog::Accepted || history.selectedTool()!="storage") return 7;
    tux::HistoryDialog busy({storage},true);
    if(busy.findChild<QPushButton*>("historyRerun")->isEnabled()) return 8;
    tux::Result altered=storage; altered.command.append("unexpected");
    tux::HistoryDialog unknown({altered},false);
    if(unknown.findChild<QPushButton*>("historyRerun")->isEnabled() || !unknown.selectedTool().isEmpty()) return 9;
    tux::Result elevated; elevated.action=tux::action("apt-clean").title; elevated.command=tux::action("apt-clean").command;
    elevated.command.prepend("pkexec");
    tux::HistoryDialog admin({elevated},false);
    if(admin.selectedTool()!="apt-clean") return 10;
    QVector<tux::Result> many(25,storage); tux::HistoryDialog bounded(many,false);
    if(bounded.findChild<QTableWidget*>("historyTable")->rowCount()!=20) return 11;
    std::cout<<"Session history: snapshot/tail bounds, ordering, filter, local details, busy/unknown refusal and review request passed\n";
}
