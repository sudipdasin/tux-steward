// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "tux/core.hpp"
#include <QWidget>
#include <functional>
#include <memory>
#include <thread>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QTextDecoder;

namespace syspulse {

// Fixed-catalog reviewed workflows. Construction and selection never execute a
// command; the production executor can only receive an existing catalog ID.
class ToolboxPage final : public QWidget {
public:
    using Executor = std::function<tux::Result(const QString&, tux::Output,
                                               const std::atomic_bool*)>;
    explicit ToolboxPage(const QStringList& groups, bool experimental = false,
                         QWidget* parent = nullptr);
    // Dependency injection is explicit and intended for deterministic VM tests.
    ToolboxPage(const QStringList& groups, bool experimental, Executor executor,
                QWidget* parent = nullptr);
    ~ToolboxPage() override;

    int toolCount() const;
    bool isBusy() const { return busy_; }
    bool isCancellable() const { return busy_ && cancellable_; }
    void requestStop();
    void setSearch(const QString& query);
    void setExecutionAllowed(bool allowed);
    QJsonObject summaryReport() const;
    std::function<void(bool)> onBusyChanged;
    std::function<bool()> canExecute;
    std::function<void(const tux::Result&)> onResult;

private:
    struct SharedRun;
    QVector<int> catalogIndices_;
    QVector<tux::Result> history_;
    Executor executor_;
    bool injectedExecutor_ = false;
    bool experimental_ = false;
    bool busy_ = false;
    bool cancellable_ = false;
    bool executionAllowed_ = true;
    int runningCatalogIndex_ = -1;
    std::shared_ptr<SharedRun> run_;
    std::thread worker_;
    std::unique_ptr<QTextDecoder> decoder_;
    QTimer* pollTimer_ = nullptr;
    QTableWidget* tools_ = nullptr;
    QTableWidget* historyTable_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLabel* count_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* description_ = nullptr;
    QLabel* access_ = nullptr;
    QLabel* status_ = nullptr;
    QPlainTextEdit* command_ = nullptr;
    QPlainTextEdit* output_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;

    int selectedCatalogIndex() const;
    void filter();
    void updateSelection();
    void reviewAndRun();
    void poll();
    void appendOutput(const QString& text);
    void exportReport();
};

}
