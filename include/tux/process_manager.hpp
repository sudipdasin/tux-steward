// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QLabel;
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
    explicit ProcessManagerWidget(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void stopSelectedProcess(bool force);
    void refreshProcesses();
    void exportProcessReport();
    qint64 selectedPid() const;

    ProcessTableModel *model_ = nullptr;
    QSortFilterProxyModel *proxy_ = nullptr;
    QTableView *table_ = nullptr;
    QLineEdit *search_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *endTaskButton_ = nullptr;
    QPushButton *forceStopButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;
    QTimer *timer_ = nullptr;
};
