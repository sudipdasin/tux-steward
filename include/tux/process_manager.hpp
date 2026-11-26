// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QLabel;
class QComboBox;
class QTreeWidget;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QShowEvent;
class QHideEvent;
class QTableView;
class QTimer;
class ProcessTableModel;

class ProcessManagerWidget final : public QWidget {
public:
    explicit ProcessManagerWidget(QWidget *parent = nullptr, bool includeSystemTabs = true);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void stopSelectedProcess(bool force);
    void showProcessDetails();
    void refreshProcesses();
    void exportProcessReport();
    void refreshProcessTree();
    void filterProcessTree();
    qint64 selectedPid() const;

    ProcessTableModel *model_ = nullptr;
    QSortFilterProxyModel *proxy_ = nullptr;
    QTableView *table_ = nullptr;
    QLineEdit *search_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *endTaskButton_ = nullptr;
    QPushButton *forceStopButton_ = nullptr;
    QPushButton *detailsButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;
    QTimer *timer_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QLineEdit *treeSearch_ = nullptr;
    QComboBox *refreshSpeed_ = nullptr;
    QPushButton *pauseButton_ = nullptr;
    bool paused_ = false;
};
