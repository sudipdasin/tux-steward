// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/health_dialog.hpp"
#include "tux/report_dialog.hpp"
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QSignalBlocker>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
namespace tux {
HealthDialog::HealthDialog(QWidget* parent,int timeoutMs):QDialog(parent),timeoutMs_(timeoutMs) {
    setObjectName("healthCheckDialog"); setWindowTitle("Health Check · Tux Steward"); resize(950,780);
    setStyleSheet("QDialog,QWidget{background:#101827;color:#e7eef9;font-size:14px;}"
        "QHeaderView::section{background:#192438;color:#e7eef9;padding:6px;border:0;}"
        "QTableView::indicator:unchecked{border:1px solid #9aacc5;background:#0b1220;width:14px;height:14px;}"
        "QTableWidget{background:#0b1220;gridline-color:#2a3c54;selection-background-color:#293c54;}"
        "QPlainTextEdit{background:#0b1220;border:1px solid #2a3c54;padding:8px;font-family:monospace;}"
        "QPushButton{background:#2b4359;border:0;border-radius:5px;padding:9px;}"
        "QPushButton:hover{background:#365775;} QPushButton:disabled{color:#8794a7;background:#202c3e;}"
        "QCheckBox::indicator:unchecked{border:1px solid #9aacc5;background:#0b1220;width:16px;height:16px;}");
    auto* layout=new QVBoxLayout(this);
    auto* heading=new QLabel("Health Check · Review, inspect, recover"); heading->setStyleSheet("font-size:22px;font-weight:bold;color:#66dfbf;"); layout->addWidget(heading);
    auto* notice=new QLabel("Choose diagnostics and review their exact commands below. Nothing runs until you select Run selected checks. Results may contain private machine information. Completion is not a guarantee that the system is healthy."); notice->setWordWrap(true); layout->addWidget(notice);
    presets_=new QComboBox; presets_->setObjectName("healthPreset"); presets_->setAccessibleName("Health check preset");
    presets_->addItem("Choose a preset…",""); presets_->addItem("Balanced · Storage, memory and services","balanced");
    presets_->addItem("Storage · Space and devices","storage"); presets_->addItem("Logs · Services, kernel and system warnings","logs"); presets_->addItem("All six diagnostics","all");
    layout->addWidget(presets_);
    const auto ids=healthActions(); tasks_=new QTableWidget(ids.size(),2); tasks_->setObjectName("healthTasks");
    tasks_->setAccessibleName("Health check diagnostics"); tasks_->setHorizontalHeaderLabels({"Include diagnostic","Status"});
    tasks_->setSelectionBehavior(QAbstractItemView::SelectRows); tasks_->setSelectionMode(QAbstractItemView::SingleSelection);
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
    auto* inspectHint=new QLabel("Select a diagnostic row to inspect its latest result. Prior runs remain in Review report and session history."); inspectHint->setWordWrap(true); layout->addWidget(inspectHint);
    output_=new QPlainTextEdit; output_->setObjectName("healthOutput"); output_->setReadOnly(true); output_->setAccessibleName("Completed diagnostic output"); layout->addWidget(output_,2);
    auto* buttons=new QHBoxLayout;
    start_=new QPushButton("Run selected checks"); start_->setObjectName("healthRun"); buttons->addWidget(start_);
    stop_=new QPushButton("Stop check"); stop_->setObjectName("healthStop"); stop_->setEnabled(false); buttons->addWidget(stop_);
    export_=new QPushButton("Review report…"); export_->setObjectName("healthExport"); export_->setEnabled(false); buttons->addWidget(export_);
    incomplete_=new QPushButton("Review incomplete checks…"); incomplete_->setObjectName("healthIncomplete"); incomplete_->setEnabled(false); buttons->addWidget(incomplete_);
    layout->addLayout(buttons); buttons=new QHBoxLayout;
    newPlan_=new QPushButton("Prepare new plan…"); newPlan_->setObjectName("healthNewPlan"); newPlan_->setEnabled(false); buttons->addWidget(newPlan_);
    connect(newPlan_,&QPushButton::clicked,this,[this]{
        if(running_ || !started_) return;
        started_=false; inspected_.clear(); incomplete_->setEnabled(false); newPlan_->setEnabled(false);
        presets_->setEnabled(true); stopOnFailure_->setEnabled(true);
        {const QSignalBlocker block(tasks_); for(int row=0;row<tasks_->rowCount();++row) tasks_->item(row,0)->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);}
        review(); status_->setText("New plan ready for review. Nothing has restarted. Prior results remain in Review report.");
    });
    auto* close=new QPushButton("Close"); close->setObjectName("healthClose"); close->setDefault(true); buttons->addWidget(close); layout->addLayout(buttons);
    connect(close,&QPushButton::clicked,this,&HealthDialog::reject);
    connect(tasks_,&QTableWidget::itemChanged,this,[this]{if(!started_) {const QSignalBlocker block(presets_); presets_->setCurrentIndex(0);} review();});
    connect(tasks_,&QTableWidget::itemSelectionChanged,this,[this]{inspect();});
    connect(presets_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){if(!started_ && index>0) choose(healthPreset(presets_->itemData(index).toString()));});
    connect(incomplete_,&QPushButton::clicked,this,[this]{prepareIncomplete();});
    connect(start_,&QPushButton::clicked,this,[this]{start();}); connect(stop_,&QPushButton::clicked,this,[this]{stop();});
    connect(export_,&QPushButton::clicked,this,[this]{if(!running_ && !results_.isEmpty()) {ReportDialog dialog(results_,this); dialog.exec();}});
    timer_=new QTimer(this); timer_->setInterval(40); connect(timer_,&QTimer::timeout,this,[this]{poll();}); timer_->start(); review(); tasks_->selectRow(0);
}
HealthDialog::~HealthDialog() {cancel_.store(true); if(worker_.joinable()) worker_.join();}
QStringList HealthDialog::selected() const {
    QStringList ids; for(int row=0;row<tasks_->rowCount();++row) if(tasks_->item(row,0)->checkState()==Qt::Checked) ids.append(tasks_->item(row,0)->data(Qt::UserRole).toString()); return ids;
}
void HealthDialog::choose(const QStringList& ids) {
    if(started_) return;
    {const QSignalBlocker block(tasks_); for(int row=0;row<tasks_->rowCount();++row)
        tasks_->item(row,0)->setCheckState(ids.contains(tasks_->item(row,0)->data(Qt::UserRole).toString())?Qt::Checked:Qt::Unchecked);}
    review();
}
void HealthDialog::inspect() {
    const int row=tasks_->currentRow(); if(row<0) {output_->setPlainText("Select a diagnostic to inspect its local result."); return;}
    const auto id=tasks_->item(row,0)->data(Qt::UserRole).toString();
    if(!inspected_.contains(id)) {output_->setPlainText(action(id).title+"\n"+tasks_->item(row,1)->text()+"\nNo result recorded for this diagnostic in the current run."); return;}
    const auto& result=inspected_[id];
    output_->setPlainText(QString("%1 · Exit %2 · %3 seconds%4%5\n\n%6")
        .arg(result.action).arg(result.code).arg(result.seconds,0,'f',2)
        .arg(result.cancelled?" · Cancelled":"").arg(result.truncated?" · Output truncated":"").arg(QString::fromUtf8(result.output)));
}
void HealthDialog::prepareIncomplete() {
    if(running_ || !incomplete_->isEnabled()) return;
    const auto ids=incompleteHealthActions(lastPlan_,lastResults_); if(ids.isEmpty()) return;
    started_=false; newPlan_->setEnabled(false); inspected_.clear(); presets_->setEnabled(true); stopOnFailure_->setEnabled(true); incomplete_->setEnabled(false);
    {const QSignalBlocker block(tasks_); for(int row=0;row<tasks_->rowCount();++row)
        tasks_->item(row,0)->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);}
    {const QSignalBlocker block(presets_); presets_->setCurrentIndex(0);}
    choose(ids); inspect();
    status_->setText("Incomplete checks selected for review. Nothing has restarted. Review commands, then select Run selected checks. Prior results remain in Review report.");
}
void HealthDialog::review() {
    if(started_) return;
    const auto ids=selected(); QStringList commands; bool ready=!ids.isEmpty();
    for(const auto& id:ids) {const auto& tool=action(id); commands.append(tool.title+"\n"+preview(tool.command)); ready=ready && unavailable(tool).isEmpty();}
    {const QSignalBlocker block(tasks_); for(int row=0;row<tasks_->rowCount();++row) {
        const auto id=tasks_->item(row,0)->data(Qt::UserRole).toString(); const auto problem=unavailable(action(id));
        tasks_->item(row,1)->setText(problem.isEmpty()?(ids.contains(id)?"Ready · Read only":"Not selected"):problem);
    }}
    preview_->setPlainText(commands.join("\n\n")); start_->setEnabled(ready);
    status_->setText(ids.isEmpty()?"Select at least one diagnostic.":ready?QString("%1 checks selected · Review commands, then run. Each has a time limit.").arg(ids.size()):"A selected diagnostic is unavailable. Install its dependency yourself or deselect it.");
    progress_->setRange(0,qMax(1,ids.size())); progress_->setValue(0); inspect();
}
void HealthDialog::start() {
    if(started_ || !start_->isEnabled()) return;
    const auto ids=selected(); for(const auto& id:ids) if(!unavailable(action(id)).isEmpty()) {review(); return;}
    const bool stopOnFailure=stopOnFailure_->isChecked(); started_=running_=true; cancel_.store(false);
    newPlan_->setEnabled(false); lastPlan_=ids; lastResults_.clear(); inspected_.clear(); incomplete_->setEnabled(false); presets_->setEnabled(false); export_->setEnabled(false);
    {const QSignalBlocker block(tasks_); for(int row=0;row<tasks_->rowCount();++row) {
        const auto id=tasks_->item(row,0)->data(Qt::UserRole).toString();
        tasks_->item(row,0)->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        tasks_->item(row,1)->setText(ids.contains(id)?"Pending":"Not selected");
    }}
    {std::lock_guard<std::mutex> lock(mutex_); current_.clear();}
    stopOnFailure_->setEnabled(false); start_->setEnabled(false); stop_->setEnabled(true); inspect();
    status_->setText("Starting selected diagnostics…");
    worker_=std::thread([this,ids,stopOnFailure]{
        HealthRun run;
        try {run=runHealthCheck(ids,stopOnFailure,timeoutMs_,&cancel_,[this](const QString& id,const Result* result){
            std::lock_guard<std::mutex> lock(mutex_); current_=id; if(result) pending_.append(*result);
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
        results_.append(result); if(results_.size()>20) results_.removeFirst();
        lastResults_.append(result);
        for(int row=0;row<tasks_->rowCount();++row) {
            const auto id=tasks_->item(row,0)->data(Qt::UserRole).toString(); const auto& tool=action(id);
            if(tool.title!=result.action || tool.command!=result.command) continue;
            inspected_.insert(id,result); tasks_->item(row,1)->setText(result.cancelled?"Cancelled":result.code==0?"Completed":result.code==124?"Timed out":QString("Needs attention (%1)").arg(result.code));
        }
        progress_->setValue(lastResults_.size());
    }
    if(running_ && !current.isEmpty() && !inspected_.contains(current)) {
        for(int row=0;row<tasks_->rowCount();++row) if(tasks_->item(row,0)->data(Qt::UserRole).toString()==current) tasks_->item(row,1)->setText("Running");
    }
    if(!pending.isEmpty()) inspect();
    if(done) {
        if(worker_.joinable()) worker_.join(); running_=false; newPlan_->setEnabled(true); stop_->setEnabled(false); export_->setEnabled(!results_.isEmpty());
        for(int row=0;row<tasks_->rowCount();++row) {const auto id=tasks_->item(row,0)->data(Qt::UserRole).toString();
            if(lastPlan_.contains(id) && !inspected_.contains(id)) tasks_->item(row,1)->setText("Not run");}
        try {incomplete_->setEnabled(!incompleteHealthActions(lastPlan_,lastResults_).isEmpty());}
        catch(const std::exception&) {incomplete_->setEnabled(false);}
        inspect();
        status_->setText(QString("%1 · %2 of %3 checks recorded. Select a row to inspect its result. Review incomplete checks to prepare another run.")
            .arg(done->stopped?"Stopped":done->stoppedOnFailure?"Stopped after failure":"Finished").arg(done->results.size()).arg(done->total));
    } else if(running_ && !cancel_.load() && !current.isEmpty()) status_->setText("Running · "+action(current).title+" · You can stop this read-only check.");
}
void HealthDialog::reject() {if(running_) {stop(); return;} QDialog::reject();}
void HealthDialog::closeEvent(QCloseEvent* event) {if(running_) {stop(); event->ignore();} else event->accept();}
}
