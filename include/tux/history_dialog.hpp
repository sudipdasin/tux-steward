// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QDialog>
class QTableWidget;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;
class QComboBox;
class QLabel;
namespace tux {
class HistoryDialog : public QDialog {
public:
    explicit HistoryDialog(const QVector<Result>& results, bool taskRunning, QWidget* parent=nullptr);
    QString selectedTool() const;
    QVector<Result> visibleResults() const;
private:
    QVector<Result> snapshot_;
    bool taskRunning_;
    QTableWidget* table_;
    QPlainTextEdit* details_;
    QPushButton* rerun_;
    QLineEdit* search_;
    QComboBox* outcome_;
    QLabel* count_;
    QPushButton* report_;
    QString toolFor(int index) const;
    void select();
    void filter();
};
}
