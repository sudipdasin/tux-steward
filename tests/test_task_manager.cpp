// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_manager.hpp"

#include <QApplication>
#include <QStringList>
#include <QTabWidget>

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
        QStringLiteral("Services")
    };
    for (const QString &name : expected) {
        bool found = false;
        for (int i = 0; i < tabs->count(); ++i) found = found || tabs->tabText(i) == name;
        if (!found) {
            std::cerr << "Missing Task Manager tab: " << name.toStdString() << '\n';
            return 1;
        }
    }
    return 0;
}
