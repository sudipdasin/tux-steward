// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_manager.hpp"

#include <QApplication>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QEventLoop>
#include <QTimer>
#include <QStringList>
#include <QTabWidget>
#include <QTableView>

#include <iostream>

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    ProcessManagerWidget manager;
    const auto *tabs = manager.findChild<QTabWidget *>(QStringLiteral("taskManagerTabs"));
    if (tabs == nullptr) {
        std::cerr << "Task Manager tab control was not created\n";
        return 1;
    }
    const QStringList expected{
        QStringLiteral("Processes"), QStringLiteral("Performance"), QStringLiteral("App history"),
        QStringLiteral("Users"), QStringLiteral("Details"), QStringLiteral("Network"),
        QStringLiteral("Disk"), QStringLiteral("Bluetooth"), QStringLiteral("Startup apps"),
        QStringLiteral("Services"), QStringLiteral("GPU")
    };
    const auto *processTable = manager.findChild<QTableView *>(QStringLiteral("processTable"));
    if (processTable == nullptr || processTable->model() == nullptr || processTable->model()->columnCount() != 9) {
        std::cerr << "Process list is missing the parent-process column\n";
        return 1;
    }
    for (const QString &name : expected) {
        bool found = false;
        for (const auto *group : manager.findChildren<QTabWidget *>())
            for (int i = 0; i < group->count(); ++i) found = found || group->tabText(i) == name;
        if (!found) {
            std::cerr << "Missing Task Manager tab: " << name.toStdString() << '\n';
            return 1;
        }
    }
    const auto *experimental=manager.findChild<QTabWidget *>("experimentalResourceTabs");
    if (!experimental || tabs->indexOf(const_cast<QTabWidget *>(experimental))<0 || experimental->count()!=2) return 20;
    if (experimental->isAncestorOf(manager.findChild<QWidget *>("serviceAction_start"))
        || experimental->isAncestorOf(manager.findChild<QWidget *>("startupDisable"))
        || !experimental->isAncestorOf(manager.findChild<QWidget *>("bluetoothScanButton"))) return 21;
    if(!manager.findChild<QListWidget*>("taskManagerNavigation") || !manager.findChild<QListWidget*>("performanceNavigation"))return 35;
    const QStringList charts{QStringLiteral("cpuUsageChart"), QStringLiteral("memoryUsageChart"),
                             QStringLiteral("diskIoChart"), QStringLiteral("networkIoChart")};
    for (const QString &name : charts) {
        if (manager.findChild<QWidget *>(name) == nullptr) {
            std::cerr << "Missing live performance chart: " << name.toStdString() << '\n';
            return 1;
        }
    }
    const QStringList bluetoothControls{
        QStringLiteral("bluetoothDeviceSelect"), QStringLiteral("bluetoothScanButton"),
        QStringLiteral("bluetoothPairButton"), QStringLiteral("bluetoothAction_connect"),
        QStringLiteral("bluetoothAction_disconnect"), QStringLiteral("bluetoothAction_trust"),
        QStringLiteral("bluetoothAction_remove"), QStringLiteral("bluetoothSettingsButton")
    };
    for (const QString &name : bluetoothControls) {
        if (manager.findChild<QWidget *>(name) == nullptr) {
            std::cerr << "Missing Bluetooth control: " << name.toStdString() << '\n';
            return 1;
        }
    }
    if (manager.findChild<QWidget *>(QStringLiteral("gpuSummary")) == nullptr) {
        std::cerr << "Missing GPU metrics view\n";
        return 1;
    }
    auto *resourceTimer = manager.findChild<QTimer *>(QStringLiteral("taskManagerResourceRefreshTimer"));
    if (resourceTimer == nullptr || resourceTimer->isActive()) {
        std::cerr << "Hidden resource views must not poll\n";
        return 1;
    }
    manager.show();
    application.processEvents();
    if (resourceTimer->isActive()) {
        std::cerr << "Processes tab must not run the resource timer\n";
        return 1;
    }
    auto *mutableTabs = manager.findChild<QTabWidget *>(QStringLiteral("taskManagerTabs"));
    int performanceIndex = -1;
    for (int i = 0; i < tabs->count(); ++i)
        if (tabs->tabText(i) == QStringLiteral("Performance")) performanceIndex = i;
    mutableTabs->setCurrentIndex(performanceIndex);
    application.processEvents();
    if (!resourceTimer->isActive() || resourceTimer->interval() != 2000) {
        std::cerr << "Visible Performance view must refresh every two seconds\n";
        return 1;
    }
    int ticks = 0;
    QObject::connect(resourceTimer, &QTimer::timeout, &manager, [&ticks]() { ++ticks; });
    QEventLoop loop;
    QTimer::singleShot(2400, &loop, &QEventLoop::quit);
    loop.exec();
    if (ticks == 0) {
        std::cerr << "Resource refresh timer did not fire\n";
        return 1;
    }
    if(argc>1 && !manager.grab().save(QString::fromLocal8Bit(argv[1]))) return 34;
    auto* pause=manager.findChild<QPushButton*>("performancePause");
    auto* speed=manager.findChild<QComboBox*>("performanceRefreshRate");
    auto* resourcePages=manager.findChild<QTabWidget*>("resourceDetailTabs");
    if(!pause || !speed || !resourcePages || resourcePages->count()!=5) return 30;
    pause->click();application.processEvents();if(resourceTimer->isActive())return 31;
    speed->setCurrentIndex(0);pause->click();application.processEvents();if(!resourceTimer->isActive()||resourceTimer->interval()!=1000)return 32;
    speed->setCurrentIndex(2);if(resourceTimer->interval()!=5000)return 33;
    speed->setCurrentIndex(1);
    manager.hide();
    application.processEvents();
    if (resourceTimer->isActive()) {
        std::cerr << "Hidden Task Manager continued polling\n";
        return 1;
    }
    manager.show();
    application.processEvents();
    if (!resourceTimer->isActive()) {
        std::cerr << "Resource timer did not resume after showing the window\n";
        return 1;
    }
    mutableTabs->setCurrentIndex(0);
    if (resourceTimer->isActive()) {
        std::cerr << "Resource timer did not stop on Processes tab\n";
        return 1;
    }
    manager.hide();
    return 0;
}

