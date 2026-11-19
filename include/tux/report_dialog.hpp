// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QDialog>
class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
namespace tux {
class ReportDialog : public QDialog {
public:
    explicit ReportDialog(const QVector<Result>& results, QWidget* parent=nullptr);
private:
    QVector<Result> snapshot_;
    QTableWidget* tasks_;
    QCheckBox* details_;
    QLabel* count_;
    QPlainTextEdit* preview_;
    QPushButton* save_;
    QByteArray bytes_;
    void refresh();
    void selectAll(bool selected);
    void save();
};
}
