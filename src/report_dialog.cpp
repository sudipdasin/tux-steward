// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/report_dialog.hpp"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
namespace tux {
ReportDialog::ReportDialog(const QVector<Result>& results,QWidget* parent)
    : QDialog(parent),snapshot_(results.mid(qMax(0,results.size()-20))) {
    setObjectName("reportReview"); setWindowTitle("Review session report"); resize(850,700);
    auto* layout=new QVBoxLayout(this);
    auto* notice=new QLabel("Choose completed tasks to include. Summary omits command lines and raw diagnostics. Saved files are owner-only. Review the exact preview before sharing; nothing is uploaded.");
    notice->setWordWrap(true); layout->addWidget(notice);
    auto* selection=new QHBoxLayout;
    count_=new QLabel; count_->setObjectName("reportCount"); selection->addWidget(count_,1);
    auto* all=new QPushButton("Select all"); all->setObjectName("reportAll");
    auto* none=new QPushButton("Select none"); none->setObjectName("reportNone");
    all->setEnabled(!snapshot_.isEmpty()); none->setEnabled(!snapshot_.isEmpty());
    selection->addWidget(all); selection->addWidget(none); layout->addLayout(selection);
    tasks_=new QTableWidget(snapshot_.size(),2); tasks_->setObjectName("reportTasks");
    tasks_->setAccessibleName("Completed tasks to include in report");
    tasks_->setHorizontalHeaderLabels({"Include task","Result"}); tasks_->verticalHeader()->hide();
    tasks_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tasks_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    tasks_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    for(int row=0;row<snapshot_.size();++row) {
        const int index=snapshot_.size()-1-row; const auto& result=snapshot_[index];
        auto* item=new QTableWidgetItem(result.action); item->setData(Qt::UserRole,index);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable); item->setCheckState(Qt::Checked);
        tasks_->setItem(row,0,item);
        tasks_->setItem(row,1,new QTableWidgetItem(result.cancelled?"Cancelled":result.code==0?"Completed":QString("Needs attention (%1)").arg(result.code)));
    }
    layout->addWidget(tasks_,1);
    details_=new QCheckBox("Include commands and diagnostic output (may contain private data)");
    details_->setObjectName("reportDetails");
    details_->setStyleSheet("QCheckBox::indicator{width:16px;height:16px;} QCheckBox::indicator:unchecked{border:1px solid #9aacc5;background:#0b1220;}");
    layout->addWidget(details_);
    preview_=new QPlainTextEdit; preview_->setObjectName("reportPreview"); preview_->setReadOnly(true);
    preview_->setAccessibleName("Exact JSON report preview"); layout->addWidget(preview_,2);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);
    save_=buttons->button(QDialogButtonBox::Save); save_->setObjectName("reportSave");
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,this,[this]{save();});
    connect(tasks_,&QTableWidget::itemChanged,this,[this]{refresh();});
    connect(details_,&QCheckBox::toggled,this,[this]{refresh();});
    connect(all,&QPushButton::clicked,this,[this]{selectAll(true);});
    connect(none,&QPushButton::clicked,this,[this]{selectAll(false);});
    refresh();
}
void ReportDialog::selectAll(bool selected) {
    { const QSignalBlocker block(tasks_);
      for(int row=0;row<tasks_->rowCount();++row) tasks_->item(row,0)->setCheckState(selected?Qt::Checked:Qt::Unchecked); }
    refresh();
}
void ReportDialog::refresh() {
    // Keep chronological report ordering regardless of newest-first display order.
    QVector<Result> selected;
    for(int index=0;index<snapshot_.size();++index) {
        const int row=snapshot_.size()-1-index;
        if(tasks_->item(row,0)->checkState()==Qt::Checked) selected.push_back(snapshot_[index]);
    }
    count_->setText(snapshot_.isEmpty()?"No completed tasks to export.":QString("%1 of %2 tasks selected").arg(selected.size()).arg(snapshot_.size()));
    save_->setEnabled(!selected.isEmpty());
    bytes_=QJsonDocument(sessionReport(selected,details_->isChecked())).toJson();
    preview_->setPlainText(QString::fromUtf8(bytes_));
}
void ReportDialog::save() {
    if(!save_->isEnabled()) return;
    // Capture the reviewed bytes before entering the nested file picker event loop.
    const auto reviewed=bytes_;
    const auto path=QFileDialog::getSaveFileName(this,"Export reviewed report","tux-steward-report.json","JSON reports (*.json)");
    if(path.isEmpty()) return;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly) ||
       !file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner) ||
       file.write(reviewed)!=reviewed.size() || !file.commit()) {
        QMessageBox::critical(this,"Export failed","The report could not be saved. "+file.errorString()); return;
    }
    accept();
}
}

