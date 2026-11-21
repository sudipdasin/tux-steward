// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/health_dialog.hpp"
#include "tux/report_dialog.hpp"
#include <QCheckBox>
#include <QCloseEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
namespace tux {
HealthDialog::HealthDialog(QWidget* parent,int timeoutMs):QDialog(parent),timeoutMs_(timeoutMs) {
    setObjectName("healthCheckDialog"); setWindowTitle("Health Check · Tux Steward"); resize(950,780);
    setStyleSheet("QDialog,QWidget{background:#101827;color:#e7eef9;font-size:14px;}"
        "QHeaderView::section{background:#192438;color:#e7eef9;padding:6px;border:0;}"
        "QTableWidget{background:#0b1220;gridline-color:#2a3c54;selection-background-color:#293c54;}"
        "QPlainTextEdit{background:#0b1220;border:1px solid #2a3c54;padding:8px;font-family:monospace;}"
        "QPushButton{background:#2b4359;border:0;border-radius:5px;padding:9px;}"
        "QPushButton:hover{background:#365775;} QPushButton:disabled{color:#8794a7;background:#202c3e;}"
        "QCheckBox::indicator:unchecked{border:1px solid #9aacc5;background:#0b1220;width:16px;height:16px;}");
    auto* layout=new QVBoxLayout(this);
    auto* heading=new QLabel("Review a read-only health check"); heading->setStyleSheet("font-size:22px;font-weight:bold;color:#66dfbf;"); layout->addWidget(heading);
    auto* notice=new QLabel("Choose diagnostics and review their exact commands below. Nothing runs until you select Run selected checks. Results may contain private machine information. Completion is not a guarantee that the system is healthy."); notice->setWordWrap(true); layout->addWidget(notice);
    const auto ids=healthActions(); tasks_=new QTableWidget(ids.size(),2); tasks_->setObjectName("healthTasks");
    tasks_->setAccessibleName("Health check diagnostics"); tasks_->setHorizontalHeaderLabels({"Include diagnostic","Availability"});
    tasks_->setEditTriggers(QAbstractItemView::NoEditTriggers); tasks_->verticalHeader()->hide();
    tasks_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch); tasks_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    for(int row=0;row<ids.size();++row) {
        const auto& tool=action(ids[row]); auto* item=new QTableWidgetItem(tool.title); item->setData(Qt::UserRole,tool.id);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable); item->setCheckState(row<4?Qt::Checked:Qt::Unchecked); tasks_->setItem(row,0,item);
        const auto problem=unavailable(tool); tasks_->setItem(row,1,new QTableWidgetItem(problem.isEmpty()?"Ready · Read only":problem));
    }
    layout->addWidget(tasks_,2);
    stopOnFailure_=new QCheckBox("Stop after a failed diagnostic (remaining checks will not run)"); stopOnFailure_->setObjectName("healthStopOnFailure"); stopOnFailure_->setChecked(true); layout->addWidget(stopOnFailure_);
    preview_=new QPlainTextEdit; preview_->setObjectName("healthCommands"); preview_->setAccessibleName("Commands reviewed before health check"); preview_->setReadOnly(true); layout->addWidget(preview_,1);
    status_=new QLabel; status_->setObjectName("healthStatus"); status_->setWordWrap(true); layout->addWidget(status_);
    progress_=new QProgressBar; progress_->setObjectName("healthProgress"); progress_->setRange(0,4); progress_->setValue(0); layout->addWidget(progress_);
    output_=new QPlainTextEdit; output_->setObjectName("healthOutput"); output_->setReadOnly(true); output_->setAccessibleName("Completed diagnostic output"); layout->addWidget(output_,2);
    auto* buttons=new QHBoxLayout;
    start_=new QPushButton("Run selected checks"); start_->setObjectName("healthRun"); buttons->addWidget(start_);
    stop_=new QPushButton("Stop check"); stop_->setObjectName("healthStop"); stop_->setEnabled(false); buttons->addWidget(stop_);
    export_=new QPushButton("Review report…"); export_->setObjectName("healthExport"); export_->setEnabled(false); buttons->addWidget(export_);
    auto* close=new QPushButton("Close"); close->setObjectName("healthClose"); close->setDefault(true); buttons->addWidget(close); layout->addLayout(buttons);
    connect(close,&QPushButton::clicked,this,&HealthDialog::reject);
    connect(tasks_,&QTableWidget::itemChanged,this,[this]{review();});
    connect(start_,&QPushButton::clicked,this,[this]{start();}); connect(stop_,&QPushButton::clicked,this,[this]{stop();});
    connect(export_,&QPushButton::clicked,this,[this]{if(!running_ && !results_.isEmpty()) {ReportDialog dialog(results_,this); dialog.exec();}});
    timer_=new QTimer(this); timer_->setInterval(40); connect(timer_,&QTimer::timeout,this,[this]{poll();}); timer_->start(); review();
}
HealthDialog::~HealthDialog() {cancel_.store(true); if(worker_.joinable()) worker_.join();}
QStringList HealthDialog::selected() const {
    QStringList ids; for(int row=0;row<tasks_->rowCount();++row) if(tasks_->item(row,0)->checkState()==Qt::Checked) ids.append(tasks_->item(row,0)->data(Qt::UserRole).toString()); return ids;
}
void HealthDialog::review() {
    if(started_) return;
    const auto ids=selected(); QStringList commands; bool ready=!ids.isEmpty();
    for(const auto& id:ids) {const auto& tool=action(id); commands.append(tool.title+"\n"+preview(tool.command)); ready=ready && unavailable(tool).isEmpty();}
    preview_->setPlainText(commands.join("\n\n")); start_->setEnabled(ready);
    status_->setText(ids.isEmpty()?"Select at least one diagnostic.":ready?QString("%1 checks selected · Review commands, then run. Each has a time limit.").arg(ids.size()):"A selected diagnostic is unavailable. Install its dependency yourself or deselect it.");
    progress_->setRange(0,qMax(1,ids.size())); progress_->setValue(0);
}
void HealthDialog::start() {
    if(started_ || !start_->isEnabled()) return;
    const auto ids=selected(); for(const auto& id:ids) if(!unavailable(action(id)).isEmpty()) {review(); return;}
    const bool stopOnFailure=stopOnFailure_->isChecked(); started_=running_=true; cancel_.store(false);
    tasks_->setEnabled(false); stopOnFailure_->setEnabled(false); start_->setEnabled(false); stop_->setEnabled(true);
    status_->setText("Starting selected diagnostics…");
    worker_=std::thread([this,ids,stopOnFailure]{
        HealthRun run;
        try {run=runHealthCheck(ids,stopOnFailure,timeoutMs_,&cancel_,[this](const QString& id,const Result* result){
            std::lock_guard<std::mutex> lock(mutex_); current_=action(id).title; if(result) pending_.append(*result);
        });}
        catch(const std::exception& error) {run.total=ids.size(); run.stoppedOnFailure=true; Result result; result.action="Health check"; result.code=-1; result.output=error.what(); run.results.append(result); std::lock_guard<std::mutex> lock(mutex_); pending_.append(result);}
        std::lock_guard<std::mutex> lock(mutex_); completed_=std::move(run);
    });
}
void HealthDialog::stop() {if(running_) {cancel_.store(true); stop_->setEnabled(false); status_->setText("Stopping · Waiting for the active diagnostic; remaining checks will not run.");}}
void HealthDialog::poll() {
    QVector<Result> pending; QString current; std::optional<HealthRun> done;
    {std::lock_guard<std::mutex> lock(mutex_); pending.swap(pending_); current=current_; if(completed_) {done=std::move(completed_); completed_.reset();}}
    for(const auto& result:pending) {
        results_.append(result); output_->appendPlainText(QString("%1 · Exit %2%3\n%4\n").arg(result.action).arg(result.code).arg(result.cancelled?" · Cancelled":"").arg(QString::fromUtf8(result.output)));
        auto cursor=output_->textCursor(); const auto extra=output_->document()->characterCount()-1-OutputLimit;
        if(extra>0) {cursor.movePosition(QTextCursor::Start); cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor,extra); cursor.removeSelectedText();}
        progress_->setValue(results_.size());
    }
    if(done) {
        if(worker_.joinable()) worker_.join(); running_=false; stop_->setEnabled(false); export_->setEnabled(!results_.isEmpty());
        status_->setText(QString("%1 · %2 of %3 checks recorded. Inspect results; no system changes were made. Reopen Health Check for another run.")
            .arg(done->stopped?"Stopped":done->stoppedOnFailure?"Stopped after failure":"Finished").arg(done->results.size()).arg(done->total));
    } else if(running_ && !cancel_.load() && !current.isEmpty()) status_->setText("Running · "+current+" · You can stop this read-only check.");
}
void HealthDialog::reject() {if(running_) {stop(); return;} QDialog::reject();}
void HealthDialog::closeEvent(QCloseEvent* event) {if(running_) {stop(); event->ignore();} else event->accept();}
}
