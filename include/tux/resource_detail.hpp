// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/resource_chart.hpp"
#include "tux/resource_metrics.hpp"
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>
class ResourceDetail final : public QTabWidget {
public:
    explicit ResourceDetail(QWidget* parent=nullptr):QTabWidget(parent) {
        setObjectName("resourceDetailTabs");
        auto* cpu=page("CPU");cpuInfo_=label(cpu,"cpuDetailSummary");
        cpuTotal_=chart(cpu,"Total CPU","%",100,"cpuDetailChart");
        auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);auto* cores=new QWidget;coreGrid_=new QGridLayout(cores);scroll->setWidget(cores);cpu->addWidget(scroll,2);
        auto* memory=page("RAM");memoryInfo_=label(memory,"memoryDetailSummary");memory_=chart(memory,"RAM in use","%",100,"ramDetailChart");swap_=chart(memory,"Swap in use","%",100,"swapDetailChart");
        auto* disk=page("Disks");disks_=new QComboBox;disks_->setObjectName("diskDeviceSelect");disks_->setAccessibleName("Select disk device");disk->addWidget(disks_);diskInfo_=label(disk,"diskDetailSummary");
        diskRead_=chart(disk,"Read","MiB/s",0,"diskReadChart");diskWrite_=chart(disk,"Write","MiB/s",0,"diskWriteChart");diskBusy_=chart(disk,"Active time","%",100,"diskActiveChart");
        auto* net=page("Network");networks_=new QComboBox;networks_->setObjectName("networkInterfaceSelect");networks_->setAccessibleName("Select network interface");net->addWidget(networks_);netInfo_=label(net,"networkDetailSummary");
        receive_=chart(net,"Receive","Mbit/s",0,"networkReceiveChart");send_=chart(net,"Send","Mbit/s",0,"networkSendChart");
        connect(disks_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{diskRead_->clear();diskWrite_->clear();diskBusy_->clear();diskBaseline_=false;diskInfo_->setText("Collecting baseline for selected device…");});
        connect(networks_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{receive_->clear();send_->clear();netBaseline_=false;netInfo_->setText("Collecting baseline for selected interface…");});
    }
    void resetBaseline() {clock_.invalidate();diskBaseline_=netBaseline_=false;}
    void sample() {
        const auto cpus=tux::cpuTicks(read("/proc/stat"));const auto mem=tux::memoryCounters(read("/proc/meminfo"));
        const auto disks=tux::diskCounters(read("/proc/diskstats"));const auto nets=tux::networkCounters(read("/proc/net/dev"));
        QStringList diskNames;for(const auto& name:QDir("/sys/block").entryList(QDir::Dirs|QDir::NoDotAndDotDot)) if(!name.startsWith("loop")&&!name.startsWith("ram")&&disks.contains(name))diskNames.append(name);
        updateChoices(disks_,diskNames);updateChoices(networks_,nets.keys());
        const bool baseline=clock_.isValid();const double seconds=baseline?clock_.nsecsElapsed()/1e9:0;
        for(auto i=cores_.begin();i!=cores_.end();) {if(!cpus.contains(i.key())) {delete i.value();i=cores_.erase(i);}else ++i;}
        int coreIndex=0;for(auto i=cpus.cbegin();i!=cpus.cend();++i) {
            if(i.key()=="cpu")continue;
            if(coreIndex>=256) break;
            if(!cores_.contains(i.key())) {auto* c=new ResourceHistoryChart(i.key(),"%",100);c->setObjectName("coreChart_"+i.key());coreGrid_->addWidget(c,coreIndex/2,coreIndex%2);cores_.insert(i.key(),c);}++coreIndex;
            if(baseline&&previousCpu_.contains(i.key())) {auto n=tux::cpuUsage(previousCpu_[i.key()],i.value());if(n)cores_[i.key()]->addSample(*n);}
        }
        auto usage=baseline&&previousCpu_.contains("cpu")&&cpus.contains("cpu")?tux::cpuUsage(previousCpu_["cpu"],cpus["cpu"]):std::optional<double>{};
        if(usage)cpuTotal_->addSample(*usage);
        cpuInfo_->setText(cpus.isEmpty()?"CPU counters unavailable":QString("%1 logical CPUs · Total %2 · Per-core history below (up to 256) · %3").arg(qMax(0,cpus.size()-1)).arg(usage?QString::number(*usage,'f',1)+"%":"collecting baseline").arg(read("/proc/loadavg").section(' ',0,2).trimmed()));
        const auto total=mem.value("MemTotal"),available=mem.value("MemAvailable"),swapTotal=mem.value("SwapTotal"),swapFree=mem.value("SwapFree");
        if(total&&mem.contains("MemAvailable")) {auto used=total-qMin(total,available);memory_->addSample(100.0*used/total);if(swapTotal)swap_->addSample(100.0*(swapTotal-qMin(swapTotal,swapFree))/swapTotal);
            memoryInfo_->setText(QString("RAM %1 / %2 GiB used · Available %3 GiB\nCache %4 GiB · Buffers %5 MiB · Swap %6 / %7 GiB").arg(used/1048576.0,0,'f',2).arg(total/1048576.0,0,'f',2).arg(available/1048576.0,0,'f',2).arg(mem.value("Cached")/1048576.0,0,'f',2).arg(mem.value("Buffers")/1024.0,0,'f',1).arg((swapTotal-qMin(swapTotal,swapFree))/1048576.0,0,'f',2).arg(swapTotal/1048576.0,0,'f',2));
        }else memoryInfo_->setText("Memory counters unavailable");
        const auto disk=disks_->currentText(),net=networks_->currentText();
        if(!disk.isEmpty()&&disks.contains(disk)) {
            const auto n=disks[disk];const bool ready=baseline&&diskBaseline_&&previousDisks_.contains(disk);
            auto r=ready?tux::counterRate(previousDisks_[disk].read,n.read,seconds):std::optional<double>{};auto w=ready?tux::counterRate(previousDisks_[disk].write,n.write,seconds):std::optional<double>{};auto busy=ready?tux::counterRate(previousDisks_[disk].busyMs,n.busyMs,seconds):std::optional<double>{};
            const auto rotational=read("/sys/block/"+disk+"/queue/rotational").trimmed();
            QString identity=QString("%1 · %2 · %3 GiB · %4").arg(disk,rotational=="1"?"HDD / rotational":rotational=="0"?"Non-rotational (SSD/virtual)":"type unavailable").arg(read("/sys/block/"+disk+"/size").trimmed().toULongLong()*512.0/1073741824.0,0,'f',1).arg(read("/sys/block/"+disk+"/device/model").trimmed());
            if(r&&w&&busy) {diskRead_->addSample(*r/2048.0);diskWrite_->addSample(*w/2048.0);diskBusy_->addSample(qMin(100.0,*busy/10.0));diskInfo_->setText(identity+QString("\nRead %1 MiB/s · Write %2 MiB/s · Active %3%").arg(*r/2048.0,0,'f',2).arg(*w/2048.0,0,'f',2).arg(qMin(100.0,*busy/10.0),0,'f',1));}
            else diskInfo_->setText(identity+"\nCollecting baseline / counters reset");diskBaseline_=true;
        }else {diskInfo_->setText("No disk counters available");diskBaseline_=false;}
        if(!net.isEmpty()&&nets.contains(net)) {
            const auto n=nets[net];const bool ready=baseline&&netBaseline_&&previousNets_.contains(net);
            auto r=ready?tux::counterRate(previousNets_[net].read,n.read,seconds):std::optional<double>{};auto w=ready?tux::counterRate(previousNets_[net].write,n.write,seconds):std::optional<double>{};
            QString identity=net+" · "+read("/sys/class/net/"+net+"/operstate").trimmed();
            if(r&&w) {receive_->addSample(*r*8/1e6);send_->addSample(*w*8/1e6);netInfo_->setText(identity+QString("\nReceive %1 Mbit/s · Send %2 Mbit/s\nReceived %3 MiB · Sent %4 MiB since interface reset").arg(*r*8/1e6,0,'f',3).arg(*w*8/1e6,0,'f',3).arg(n.read/1048576.0,0,'f',1).arg(n.write/1048576.0,0,'f',1));}
            else netInfo_->setText(identity+"\nCollecting baseline / counters reset");netBaseline_=true;
        }else {netInfo_->setText("No network counters available");netBaseline_=false;}
        previousCpu_=cpus;previousDisks_=disks;previousNets_=nets;clock_.restart();
    }
private:
    static QString read(const QString& path) {QFile f(path);return f.open(QIODevice::ReadOnly)?QString::fromUtf8(f.read(1024*1024)):QString();}
    QVBoxLayout* page(const QString& name) {auto* p=new QWidget;auto* l=new QVBoxLayout(p);addTab(p,name);return l;}
    static QLabel* label(QVBoxLayout* l,const QString& name) {auto* w=new QLabel("Collecting first sample…");w->setObjectName(name);w->setWordWrap(true);l->addWidget(w);return w;}
    static ResourceHistoryChart* chart(QVBoxLayout* l,const QString& title,const QString& unit,double maximum,const QString& name) {auto* w=new ResourceHistoryChart(title,unit,maximum);w->setObjectName(name);l->addWidget(w,1);return w;}
    static void updateChoices(QComboBox* box,const QStringList& names) {QStringList old;for(int i=0;i<box->count();++i)old.append(box->itemText(i));if(old==names)return;auto selected=box->currentText();box->clear();box->addItems(names);if(names.contains(selected))box->setCurrentText(selected);}
    QLabel *cpuInfo_,*memoryInfo_,*diskInfo_,*netInfo_;QComboBox *disks_,*networks_;QGridLayout* coreGrid_;
    ResourceHistoryChart *cpuTotal_,*memory_,*swap_,*diskRead_,*diskWrite_,*diskBusy_,*receive_,*send_;
    QMap<QString,ResourceHistoryChart*> cores_;QMap<QString,tux::CpuTicks> previousCpu_;QMap<QString,tux::IoCounters> previousDisks_,previousNets_;
    QElapsedTimer clock_;bool diskBaseline_=false,netBaseline_=false;
};
