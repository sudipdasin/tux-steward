// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/resource_detail.hpp"
#include <QApplication>
#include <QThread>
#include <iostream>
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    const auto cpu=tux::cpuTicks("cpu 10 0 10 80 0 0 0 0 8 0\ncpu0 5 0 5 40 0 0 0 0\ncpu1 5 0 5 40 0 0 0 0\ncpu2 bad 0 0 0\n");
    if(cpu.size()!=3 || cpu["cpu"].total!=100 || cpu["cpu"].idle!=80) return 1;
    const auto usage=tux::cpuUsage(cpu["cpu"],{200,150});if(!usage || *usage!=30) return 2;
    if(tux::cpuUsage({100,80},{90,70}) || tux::cpuUsage({100,80},{110,100}) || tux::counterRate(100,99,1) || tux::counterRate(1,2,0))return 3;
    auto rate=tux::counterRate(100,300,2);if(!rate||*rate!=100)return 4;
    auto disks=tux::diskCounters("8 0 sda 1 0 2048 0 2 0 4096 0 0 500 0\ninvalid\n");
    if(disks.size()!=1 || disks["sda"].read!=2048 || disks["sda"].write!=4096 || disks["sda"].busyMs!=500) return 5;
    auto nets=tux::networkCounters("Inter-| Receive\neth0: 100 0 0 0 0 0 0 0 200 0 0 0 0 0 0 0\nlo: broken\n");if(nets.size()!=1||nets["eth0"].write!=200)return 6;
    auto mem=tux::memoryCounters("MemTotal: 1024 kB\nMemAvailable: 512 kB\nSwapTotal: 0 kB\nBad: x kB\n");if(mem.size()!=3||mem["MemAvailable"]!=512)return 7;
    ResourceHistoryChart bounded("test","%",100);for(int i=0;i<100;++i)bounded.addSample(i);if(bounded.sampleCount()!=90)return 8;bounded.clear();if(bounded.sampleCount())return 9;
    ResourceDetail panel;panel.resize(1000,850);panel.show();panel.sample();app.processEvents();
    if(panel.count()!=4 || panel.tabText(0)!="CPU" || panel.tabText(1)!="RAM" || panel.tabText(2)!="Disks" || panel.tabText(3)!="Network")return 10;
    QThread::msleep(60);panel.sample();app.processEvents();
    if(static_cast<ResourceHistoryChart*>(panel.findChild<QWidget*>("cpuDetailChart"))->sampleCount()!=1 || panel.findChild<QLabel*>("cpuDetailSummary")->text().contains("collecting baseline"))return 11;
    panel.resetBaseline();panel.sample();if(static_cast<ResourceHistoryChart*>(panel.findChild<QWidget*>("cpuDetailChart"))->sampleCount()!=1)return 12;
    panel.setStyleSheet("QWidget{background:#101827;color:#e7eef9;} QTabBar::tab{padding:9px;background:#22334b;} QTabBar::tab:selected{background:#365775;}");
    for(int i=0;i<6;++i) {QThread::msleep(60);panel.sample();app.processEvents();}
    if(argc>1) for(int i=0;i<panel.count();++i) {panel.setCurrentIndex(i);app.processEvents();if(!panel.grab().save(QString::fromLocal8Bit(argv[1])+QString(".%1.png").arg(i)))return 13;}
    std::cout<<"CPU guest exclusion, per-core counters, resets, elapsed rates, disk/network/memory parsers, bounded graphs, baseline and dedicated resource pages passed\n";
}
