// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/history_dialog.hpp"
#include "tux/trusted_command.hpp"
#include "tux/report_dialog.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
namespace tux {
HistoryDialog::HistoryDialog(const QVector<Result>& results,bool taskRunning,QWidget* parent)
    : QDialog(parent),taskRunning_(taskRunning) {
    // Copy only the same bounded tail retained by the session. Nothing is persisted.
    snapshot_=results.mid(qMax(0,results.size()-20));
    setObjectName("taskHistory"); setWindowTitle("Session history"); resize(900,650);
    setStyleSheet("QDialog,QWidget{background:#101827;color:#e7eef9;font-size:14px;}"
        "QHeaderView::section{background:#192438;color:#e7eef9;padding:8px;border:0;}"
        "QTableWidget{background:#0b1220;gridline-color:#2a3c54;selection-background-color:#293c54;selection-color:#66dfbf;}"
        "QLineEdit,QPlainTextEdit{background:#0b1220;border:1px solid #2a3c54;padding:8px;}"
        "QPlainTextEdit{font-family:monospace;}"
        "QPushButton{background:#2b4359;border:0;border-radius:5px;padding:9px 14px;}"
        "QPushButton:hover{background:#365775;} QPushButton:disabled{color:#8794a7;background:#202c3e;}");
    auto* layout=new QVBoxLayout(this);
    auto* notice=new QLabel("Snapshot of the latest 20 completed tasks, newest first. Details may contain private machine information. History stays in memory and is discarded when the application exits.");
    notice->setWordWrap(true); layout->addWidget(notice);
    search_=new QLineEdit; search_->setObjectName("historySearch");
    search_->setPlaceholderText("Filter by tool or result…"); search_->setAccessibleName("Filter session history");
    search_->setClearButtonEnabled(true); layout->addWidget(search_);
    outcome_=new QComboBox; outcome_->setObjectName("historyOutcome"); outcome_->setAccessibleName("Filter task outcome");
    outcome_->addItems({"All outcomes","Needs attention","Completed","Cancelled","Timed out"}); layout->addWidget(outcome_);
    count_=new QLabel; count_->setObjectName("historyCount"); layout->addWidget(count_);
    table_=new QTableWidget(snapshot_.size(),3); table_->setObjectName("historyTable");
    table_->setHorizontalHeaderLabels({"Tool","Result","Seconds"});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->verticalHeader()->hide();
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    for (int row=0;row<snapshot_.size();++row) {
        const int index=snapshot_.size()-1-row; const auto& result=snapshot_[index];
        auto* title=new QTableWidgetItem(result.action); title->setData(Qt::UserRole,index); table_->setItem(row,0,title);
        table_->setItem(row,1,new QTableWidgetItem(result.cancelled?"Cancelled":result.code==0?"Completed":result.code==124?"Timed out":QString("Needs attention (%1)").arg(result.code)));
        table_->setItem(row,2,new QTableWidgetItem(QString::number(result.seconds,'f',2)));
    }
    layout->addWidget(table_,1);
    details_=new QPlainTextEdit; details_->setObjectName("historyDetails"); details_->setReadOnly(true);
    details_->setAccessibleName("Selected task details"); details_->setMinimumHeight(180); layout->addWidget(details_,1);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close);
    rerun_=buttons->addButton("Review and run again…",QDialogButtonBox::ActionRole); rerun_->setObjectName("historyRerun");
    report_=buttons->addButton("Review matching report…",QDialogButtonBox::ActionRole); report_->setObjectName("historyReport");
    connect(report_,&QPushButton::clicked,this,[this]{const auto results=visibleResults(); if(!results.isEmpty()) {ReportDialog dialog(results,this); dialog.exec();}});
    connect(outcome_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{filter();});
    buttons->button(QDialogButtonBox::Close)->setDefault(true); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(rerun_,&QPushButton::clicked,this,[this]{if(!taskRunning_ && !selectedTool().isEmpty()) accept();});
    connect(table_,&QTableWidget::itemSelectionChanged,this,[this]{select();});
    connect(search_,&QLineEdit::textChanged,this,[this]{filter();});
    if (table_->rowCount()) table_->selectRow(0);
    filter();
}
QVector<Result> HistoryDialog::visibleResults() const {
    QVector<Result> results;
    for(int row=table_->rowCount()-1;row>=0;--row) if(!table_->isRowHidden(row)) results.append(snapshot_[table_->item(row,0)->data(Qt::UserRole).toInt()]);
    return results;
}
QString HistoryDialog::toolFor(int index) const {
    if(index<0 || index>=snapshot_.size()) return {};
    const auto& result=snapshot_[index]; QString id;
    for(const auto& tool:tux::actions()) {
        auto command=tool.command; if(tool.admin) command.prepend("pkexec");
        bool matches=command==result.command;
        if(tool.admin) {
            const auto target=trustedSystemExecutable(tool.command.first());
            const auto authorization=trustedSystemExecutable("pkexec");
            if(!target.isEmpty()&&!authorization.isEmpty()) {
                auto resolved=tool.command;resolved[0]=target;resolved.prepend(authorization);
                matches=matches||resolved==result.command;
            }
        }
        if(tool.title!=result.action || !matches) continue;
        if(!id.isEmpty()) return {}; // Never guess between ambiguous catalog entries.
        id=tool.id;
    }
    return id;
}
QString HistoryDialog::selectedTool() const {
    const int row=table_->currentRow();
    if(row<0 || table_->isRowHidden(row) || !table_->item(row,0)) return {};
    return toolFor(table_->item(row,0)->data(Qt::UserRole).toInt());
}
void HistoryDialog::select() {
    const int row=table_->currentRow();
    const bool chosen=row>=0 && !table_->isRowHidden(row) && table_->item(row,0);
    rerun_->setEnabled(chosen && !taskRunning_ && !selectedTool().isEmpty());
    rerun_->setToolTip(taskRunning_?"Wait for the active task, then reopen history.":"Opens the normal command review. Nothing runs from selection or double-click.");
    if(!chosen) {
        details_->setPlainText(snapshot_.isEmpty()?"No completed tasks yet. Run a tool to begin this session's history.":"No matching tasks. Clear the filter to see this session's completed tasks."); return;
    }
    const auto& result=snapshot_[table_->item(row,0)->data(Qt::UserRole).toInt()];
    details_->setPlainText(QString("%1\nExit code: %2 · %3 seconds%4\n\nCommand (local details):\n%5\n\n%6")
        .arg(result.action).arg(result.code).arg(result.seconds,0,'f',2)
        .arg((result.cancelled?QString(" · Cancellation requested"):QString())+(result.truncated?" · Output truncated":"")).arg(preview(result.command),QString::fromUtf8(result.output)));
}
void HistoryDialog::filter() {
    int first=-1,shown=0;
    for(int row=0;row<table_->rowCount();++row) {
        const auto text=table_->item(row,0)->text()+" "+table_->item(row,1)->text();
        const auto& result=snapshot_[table_->item(row,0)->data(Qt::UserRole).toInt()];
        const int kind=outcome_->currentIndex();
        const bool matches=kind==0 || (kind==1 && (result.code!=0 || result.cancelled)) || (kind==2 && result.code==0 && !result.cancelled) || (kind==3 && result.cancelled) || (kind==4 && result.code==124 && !result.cancelled);
        const bool visible=matches && text.contains(search_->text().trimmed(),Qt::CaseInsensitive);
        if(visible) ++shown;
        table_->setRowHidden(row,!visible); if(visible && first<0) first=row;
    }
    count_->setText(QString("%1 of %2 tasks shown · Report includes only matching tasks after privacy review.").arg(shown).arg(snapshot_.size()));
    report_->setEnabled(shown>0);
    if(first>=0) table_->selectRow(first); else table_->clearSelection();
    select();
}
}
