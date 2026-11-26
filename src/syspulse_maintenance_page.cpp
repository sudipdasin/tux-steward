// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/maintenance_page.hpp"
#include "syspulse/maintenance.hpp"
#include <QCheckBox>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <mutex>
#include <optional>
#include <thread>
namespace syspulse {
namespace {
QLabel* label(const QString& value){auto* l=new QLabel(value);l->setWordWrap(true);l->setTextFormat(Qt::PlainText);return l;}
QString bytes(quint64 value){
    if(value<1024)return QString::number(value)+" B";
    double amount=static_cast<double>(value)/1024;QString unit="KiB";
    if(amount>=1024){amount/=1024;unit="MiB";}
    if(amount>=1024){amount/=1024;unit="GiB";}
    return QString::number(amount,'f',2)+" "+unit;
}
}
struct MaintenancePage::Impl {
    MaintenancePage* q;bool recovery=false,busy=false;QString root=maintenance::cacheRoot();
    QVector<QCheckBox*> categories;QSpinBox* age=nullptr;QLabel* status=nullptr;QTableWidget* table=nullptr;
    QPushButton *scan=nullptr,*apply=nullptr,*purge=nullptr,*stop=nullptr;QTimer* poll=nullptr;std::thread worker;
    std::optional<maintenance::Plan> plan;QVector<maintenance::Recovery> runs;
    struct Shared{std::mutex mutex;std::atomic_bool cancel{false};bool done=false;QString error;std::optional<maintenance::Plan> plan;QVector<maintenance::Recovery> runs;std::optional<maintenance::Outcome> outcome;double seconds=0;};
    std::shared_ptr<Shared> shared;
    Impl(MaintenancePage* page,bool mode):q(page),recovery(mode){
        q->setObjectName(mode?"nativeRecovery":"nativeCleanup");auto* layout=new QVBoxLayout(q);
        layout->addWidget(label(mode?"Restore staged thumbnail files or explicitly remove them permanently. Restoring never overwrites a regenerated file. Startup recovery remains available in Startup apps.":"Review old thumbnail cache files, then move selected files into private recovery storage. Original photos and documents are outside this workflow. Moving files does not free disk space; permanent removal is a separate Recovery action."));
        layout->addWidget(label("Cache scope: "+root+"/thumbnails"));
        auto* row=new QHBoxLayout;
        if(!mode){for(const auto& id:maintenance::categories()){auto* c=new QCheckBox(id);c->setProperty("category",id);c->setObjectName("cleanupCategory_"+id);categories.append(c);row->addWidget(c);}age=new QSpinBox;age->setObjectName("cleanupAgeDays");age->setRange(1,3650);age->setValue(30);age->setSuffix(" days old");row->addWidget(age);}
        scan=new QPushButton(mode?"Refresh recovery":"Analyze selected categories");scan->setObjectName(mode?"refreshRecovery":"analyzeCleanup");row->addWidget(scan);layout->addLayout(row);
        status=label(mode?"Not checked. Refresh to inspect saved recovery runs.":"Nothing selected. Analysis never changes files.");status->setObjectName(mode?"recoveryStatus":"cleanupStatus");layout->addWidget(status);
        table=new QTableWidget(0,4);table->setObjectName(mode?"recoveryRuns":"cleanupPlan");table->setHorizontalHeaderLabels(mode?QStringList{"Created (UTC)","Files","Logical size","State"}:QStringList{"Category","Thumbnail file","Logical size","Review"});table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);table->verticalHeader()->hide();table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);layout->addWidget(table,1);
        auto* actions=new QHBoxLayout;apply=new QPushButton(mode?"Review restore…":"Review move to recovery…");apply->setObjectName(mode?"restoreRecovery":"applyCleanup");apply->setProperty("primary",true);apply->setEnabled(false);actions->addWidget(apply);
        if(mode){purge=new QPushButton("Review permanent removal…");purge->setObjectName("purgeRecovery");purge->setEnabled(false);actions->addWidget(purge);}else{auto* open=new QPushButton("Open recovery");open->setObjectName("openCleanupRecovery");actions->addWidget(open);QObject::connect(open,&QPushButton::clicked,q,[this]{if(q->onOpenRecovery)q->onOpenRecovery();});}
        stop=new QPushButton("Stop analysis");stop->setObjectName("stopCleanupAnalysis");stop->setEnabled(false);stop->setVisible(!mode);actions->addWidget(stop);
        QObject::connect(stop,&QPushButton::clicked,q,[this]{if(busy&&!recovery&&pendingTitle.isEmpty()&&shared)shared->cancel.store(true);});
        actions->addStretch();layout->addLayout(actions);poll=new QTimer(q);poll->setInterval(40);
        QObject::connect(scan,&QPushButton::clicked,q,[this]{refresh();});QObject::connect(apply,&QPushButton::clicked,q,[this]{review(false);});
        if(purge)QObject::connect(purge,&QPushButton::clicked,q,[this]{review(true);});
        QObject::connect(table,&QTableWidget::itemSelectionChanged,q,[this]{controls();});
        for(auto* c:categories)QObject::connect(c,&QCheckBox::toggled,q,[this]{plan.reset();table->setRowCount(0);status->setText("Selection changed. Analyze again; no files were changed.");controls();});
        if(age)QObject::connect(age,qOverload<int>(&QSpinBox::valueChanged),q,[this]{plan.reset();table->setRowCount(0);status->setText("Age changed. Analyze again; no files were changed.");controls();});
        QObject::connect(poll,&QTimer::timeout,q,[this]{finish();});
    }
    ~Impl(){if(worker.joinable())worker.join();}
    bool admitted(){return !busy&&(!q->canExecute||q->canExecute());}
    void controls(){scan->setEnabled(!busy);stop->setEnabled(busy&&!recovery&&pendingTitle.isEmpty());for(auto* c:categories)c->setEnabled(!busy);if(age)age->setEnabled(!busy);const int row=table->currentRow();const bool selected=recovery&&row>=0&&row<runs.size()&&runs[row].problem.isEmpty();apply->setEnabled(!busy&&(recovery?selected:plan&& !plan->files.isEmpty()));if(purge)purge->setEnabled(!busy&&selected);}
    void start(std::function<void(Shared&)> fn,const QString& title={}){
        if(!admitted()){status->setText("Another operation is active. Wait before starting maintenance.");return;}
        pendingTitle=title;busy=true;controls();if(q->onBusyChanged)q->onBusyChanged(true);status->setText("Working…");shared=std::make_shared<Shared>();auto state=shared;
        worker=std::thread([state,fn=std::move(fn)]{QElapsedTimer timer;timer.start();try{fn(*state);}catch(const std::exception& e){state->error=e.what();}std::lock_guard<std::mutex> lock(state->mutex);state->seconds=timer.elapsed()/1000.0;state->done=true;});
        poll->start();
    }
    QString pendingTitle;
    void refresh(){
        if(!admitted()){status->setText("Another operation is active. Wait before refreshing maintenance.");return;}
        plan.reset();runs.clear();table->setRowCount(0);
        if(recovery)start([root=root](Shared& s){s.runs=maintenance::recoveries(root);});
        else {QStringList selected;for(auto* c:categories)if(c->isChecked())selected.append(c->property("category").toString());if(selected.isEmpty()){status->setText("Select at least one category. Nothing has been changed.");return;}const auto days=age->value();start([root=root,selected,days](Shared& s){s.plan=maintenance::scan(root,selected,days,&s.cancel);});}
    }
    void review(bool remove){
        if(!admitted())return;
        const int row=table->currentRow();if(recovery&&(row<0||row>=runs.size()||!runs[row].problem.isEmpty()))return;if(!recovery&&(!plan||plan->files.isEmpty()))return;
        const auto prompt=recovery?(remove?QString("Permanently remove %1 recovery file(s), %2? This cannot be undone. Original documents are outside this action.").arg(runs[row].files).arg(bytes(runs[row].bytes)):QString("Restore %1 file(s)? Existing cache files will never be overwritten; conflicts stay in recovery.").arg(runs[row].files)):QString("Move %1 reviewed thumbnail file(s), %2, to recovery? Disk space is not freed until a separate permanent-removal review.").arg(plan->files.size()).arg(bytes(plan->bytes));
        if(QMessageBox::question(q,"Review scoped maintenance",prompt,QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes||!admitted())return;
        if(recovery){const auto id=runs[row].id;start([root=root,id,remove](Shared& s){s.outcome=remove?maintenance::purge(root,id):maintenance::restore(root,id);s.runs=maintenance::recoveries(root);},remove?"Thumbnail cache permanent removal":"Thumbnail cache restore");}
        else {const auto snapshot=*plan;start([snapshot](Shared& s){s.outcome=maintenance::stage(snapshot);},"Thumbnail cache staging");}
    }
    void finish(){
        {std::lock_guard<std::mutex> lock(shared->mutex);if(!shared->done)return;}
        if(worker.joinable())worker.join();
        poll->stop();const auto state=shared;shared.reset();
        if(state->cancel.load()&&pendingTitle.isEmpty()){plan.reset();table->setRowCount(0);status->setText("Analysis cancelled; no files changed.");}
        else if(!state->error.isEmpty())status->setText("Operation failed: "+state->error);
        else if(state->outcome){status->setText(state->outcome->message);plan.reset();if(!recovery)table->setRowCount(0);}
        else if(state->plan){plan=state->plan;table->setRowCount(plan->files.size());for(int i=0;i<plan->files.size();++i){const auto& f=plan->files[i];table->setItem(i,0,new QTableWidgetItem(f.directory));table->setItem(i,1,new QTableWidgetItem(f.name));table->setItem(i,2,new QTableWidgetItem(bytes(f.size)));table->setItem(i,3,new QTableWidgetItem("Move only after explicit review"));}status->setText(QString("%1 eligible file(s), %2. %3").arg(plan->files.size()).arg(bytes(plan->bytes),plan->limited?"Bounded scan reached its limit; only these listed files can be moved.":"Scan complete; files unchanged."));}
        if(recovery&&state->error.isEmpty()){runs=state->runs;table->setRowCount(runs.size());for(int i=0;i<runs.size();++i){table->setItem(i,0,new QTableWidgetItem(runs[i].created));table->setItem(i,1,new QTableWidgetItem(QString::number(runs[i].files)));table->setItem(i,2,new QTableWidgetItem(bytes(runs[i].bytes)));table->setItem(i,3,new QTableWidgetItem(runs[i].problem.isEmpty()?"Restore or review permanent removal":runs[i].problem));}table->setCurrentCell(-1,-1);if(!state->outcome)status->setText(QString("%1 recovery run(s), up to 100 per refresh. Select a run to review an action.").arg(runs.size()));}
        if(!pendingTitle.isEmpty()&&q->onResult){tux::Result result;result.action=pendingTitle;result.code=state->error.isEmpty()?(state->outcome&&state->outcome->skipped?2:0):1;result.output=status->text().toUtf8();result.seconds=state->seconds;q->onResult(result);}pendingTitle.clear();busy=false;controls();if(q->onBusyChanged)q->onBusyChanged(false);
    }
};
MaintenancePage::MaintenancePage(bool recovery,QWidget* parent):QWidget(parent),d(std::make_unique<Impl>(this,recovery)){}
MaintenancePage::~MaintenancePage()=default;
bool MaintenancePage::isBusy()const{return d->busy;}
bool MaintenancePage::isCancellable()const{return d->busy&&!d->recovery&&d->pendingTitle.isEmpty();}
void MaintenancePage::requestStop(){if(isCancellable()&&d->shared)d->shared->cancel.store(true);}
void MaintenancePage::refresh(){d->refresh();}
}
