// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_manager.hpp"

#include <QApplication>
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
        for (int i = 0; i < tabs->count(); ++i) found = found || tabs->tabText(i) == name;
        if (!found) {
            std::cerr << "Missing Task Manager tab: " << name.toStdString() << '\n';
            return 1;
        }
    }
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
    return 0;
}
