// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_manager.hpp"
#include "tux/process_tree.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void treeFixtures()
{
    const auto parents = processTreeParents({{1,0,1},{2,1,2},{3,2,3},{4,99,4},{5,5,5}});
    check(parents == QVector<int>({-1,0,1,-1,-1}), "normal tree, orphan and self-parent");
    check(processTreeParents({{1,2,1},{2,1,1},{3,2,2}}) == QVector<int>({-1,-1,1}), "cycles become roots without losing descendants");
    check(processTreeParents({{1,0,9},{2,1,2}}) == QVector<int>({-1,-1}), "newer/reused parent rejected");
    check(processTreeParents({{1,0,1},{1,0,1},{2,1,2}}) == QVector<int>({-1,-1,-1}), "duplicate PID parent rejected");
    check(processTreeParents({}).isEmpty(), "empty snapshot");
    QVector<ProcessTreeIdentity> deep;
    for (int i=1; i<=2000; ++i) deep.append({i,i-1,static_cast<quint64>(i)});
    check(processTreeParents(deep).last()==1998, "deep chain resolves iteratively");
}
}
int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    try {
        treeFixtures();
        ProcessManagerWidget manager;
        auto *tabs=manager.findChild<QTabWidget *>("taskManagerTabs");
        auto *tree=manager.findChild<QTreeWidget *>("processTree");
        auto *pause=manager.findChild<QPushButton *>("processPauseButton");
        auto *speed=manager.findChild<QComboBox *>("processRefreshSpeed");
        auto *timer=manager.findChild<QTimer *>("processRefreshTimer");
        auto *refresh=manager.findChild<QPushButton *>("processRefreshButton");
        auto *search=manager.findChild<QLineEdit *>("processTreeSearch");
        check(tabs && tree && pause && speed && timer && refresh && search, "explorer controls exist");
        check(!timer->isActive(), "hidden process timer stopped");
        manager.show(); app.processEvents();
        check(timer->isActive() && timer->interval()==2000, "default refresh active");
        check(tree->topLevelItemCount()>0, "real snapshot populates tree");
        bool foundSelf=false;
        int count=0;
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            ++count;
            if ((*it)->data(0,Qt::UserRole).toLongLong()==getpid()) foundSelf=true;
        }
        check(foundSelf, "current process represented exactly as a snapshot node");
        auto *table=manager.findChild<QTableView *>("processTable");
        check(count==table->model()->rowCount(), "tree includes every snapshot row once");
        speed->setCurrentIndex(0);
        check(timer->interval()==1000, "fast speed selected");
        pause->click();
        check(!timer->isActive() && pause->text().contains("Resume"), "pause stops refresh");
        speed->setCurrentIndex(2);
        check(timer->interval()==5000 && !timer->isActive(), "speed change preserves pause");
        manager.hide(); manager.show(); app.processEvents();
        check(!timer->isActive(), "pause survives hide/show");
        refresh->click();
        check(!timer->isActive(), "manual refresh is one-shot while paused");
        search->setText("no-process-can-match-this-synthetic-query");
        for (QTreeWidgetItemIterator it(tree); *it; ++it) check((*it)->isHidden(), "tree filter hides nonmatches");
        search->setText(QString::number(getpid()));
        bool selfVisible=false;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0,Qt::UserRole).toLongLong()==getpid()) selfVisible=!(*it)->isHidden();
        check(selfVisible, "PID tree search");
        search->clear();
        pause->click();
        check(timer->isActive(), "resume restarts timer");
        manager.hide(); app.processEvents();
        check(!timer->isActive(), "hide stops process refresh");
        manager.show(); app.processEvents();
        pause->click(); // Freeze one snapshot for deterministic export checks.
        QTemporaryDir directory;
        auto *exportButton=manager.findChild<QPushButton *>("processExportButton");
        check(exportButton && directory.isValid(), "export controls");
        bool defaultPrivate=false;
        const auto exportOnce=[&](bool includePrivate, const QString &filename) {
            QTimer::singleShot(0,[&] {
                auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog) return;
                auto *option=dialog->findChild<QCheckBox *>("includePrivateProcessDetails");
                auto *buttons=dialog->findChild<QDialogButtonBox *>();
                if (!option || !buttons) { dialog->reject(); return; }
                defaultPrivate=option->isChecked();
                option->setChecked(includePrivate);
                QTimer::singleShot(0,[&] {
                    if (auto *save=qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                        save->selectFile(filename);
                        QMetaObject::invokeMethod(save,"accept",Qt::DirectConnection);
                    }
                });
                buttons->button(QDialogButtonBox::Save)->click();
            });
            exportButton->click();
            QFile file(filename);
            check(file.open(QIODevice::ReadOnly), "export creates report");
            return QJsonDocument::fromJson(file.readAll()).object();
        };
        const auto standard=exportOnce(false,directory.filePath("standard.json"));
        check(!defaultPrivate && !standard["includes_private_details"].toBool(), "private fields default off");
        check(!standard["processes"].toArray().isEmpty(), "standard report retains metrics");
        for (const auto &row:standard["processes"].toArray()) {
            check(!row.toObject().contains("user") && !row.toObject().contains("command_line"), "standard report omits private fields");
            check(row.toObject().contains("pid") && row.toObject().contains("cpu_percent"), "standard report retains process data");
        }
        const auto full=exportOnce(true,directory.filePath("detailed.json"));
        check(!defaultPrivate && full["includes_private_details"].toBool(), "private details require fresh opt-in");
        check(full["processes"].toArray().first().toObject().contains("user"), "detailed report includes opted-in user field");
        manager.hide();
        std::cout<<"Process explorer fixtures, real snapshot, filters, pause/speed lifecycle and both export modes passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<"FAILED: "<<error.what()<<'\n'; return 1;
    }
}
