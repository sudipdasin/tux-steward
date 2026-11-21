// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/health.hpp"
#include <QDialog>
#include <QMap>
#include <mutex>
#include <optional>
#include <thread>
class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;
namespace tux {
class HealthDialog : public QDialog {
public:
    explicit HealthDialog(QWidget* parent=nullptr,int timeoutMs=30000);
    ~HealthDialog() override;
    QVector<Result> results() const {return results_;}
    void reject() override;
protected:
    void closeEvent(QCloseEvent*) override;
private:
    QTableWidget* tasks_;
    QComboBox* presets_;
    QPushButton* incomplete_;
    QPushButton* newPlan_;
    QStringList lastPlan_;
    QVector<Result> lastResults_;
    QMap<QString,Result> inspected_;
    QCheckBox* stopOnFailure_;
    QLabel* status_;
    QPlainTextEdit* preview_;
    QPlainTextEdit* output_;
    QProgressBar* progress_;
    QPushButton* start_;
    QPushButton* stop_;
    QPushButton* export_;
    QTimer* timer_;
    int timeoutMs_;
    bool running_=false, started_=false;
    std::atomic_bool cancel_{false};
    std::thread worker_;
    std::mutex mutex_;
    QString current_;
    QVector<Result> pending_,results_;
    std::optional<HealthRun> completed_;
    QStringList selected() const;
    void review();
    void choose(const QStringList& ids);
    void inspect();
    void prepareIncomplete();
    void start();
    void stop();
    void poll();
};
}
