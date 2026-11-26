// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/toolbox.hpp"
#include "tux/trusted_command.hpp"
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextCodec>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <mutex>
#include <optional>
#include <utility>

namespace syspulse {
namespace {
constexpr int HistoryLimit = 20;

QStringList reviewCommand(const tux::Action& tool,bool fixture) {
    auto command=tool.command;
    if(!tool.admin)return command;
    if(!fixture) {
        const auto target=tux::trustedSystemExecutable(command.first());
        const auto authorization=tux::trustedSystemExecutable("pkexec");
        if(!target.isEmpty()&&!authorization.isEmpty()) {
            command[0]=target;command.prepend(authorization);return command;
        }
    }
    command.prepend("pkexec");return command;
}

QJsonObject reportFor(const QVector<tux::Result>& results, bool details) {
    auto report = tux::sessionReport(results, details);
    report.insert("backend_version", report.value("version"));
    report.insert("application", "SysPulse");
    const auto version = QCoreApplication::applicationVersion();
    report.insert("version", version.isEmpty() ? QStringLiteral("development") : version);
    return report;
}

QString resultState(const tux::Result& result) {
    if (result.cancelled) return QStringLiteral("Cancelled");
    if (result.code == 0) return QStringLiteral("Completed");
    if (result.code == 124) return QStringLiteral("Timed out — inspect state");
    return QStringLiteral("Needs attention (%1)").arg(result.code);
}

QLabel* wrappedLabel(const QString& text, const QString& objectName,
                    QWidget* parent = nullptr) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}
}

struct ToolboxPage::SharedRun {
    std::mutex mutex;
    std::atomic_bool cancel{false};
    QByteArray pending;
    bool streamTruncated = false;
    std::optional<tux::Result> completed;
};

ToolboxPage::ToolboxPage(const QStringList& groups, bool experimental, QWidget* parent)
    : ToolboxPage(groups, experimental, Executor{}, parent) {}

ToolboxPage::ToolboxPage(const QStringList& groups, bool experimental,
                         Executor executor, QWidget* parent)
    : QWidget(parent), executor_(std::move(executor)),
      injectedExecutor_(static_cast<bool>(executor_)), experimental_(experimental) {
    setObjectName("toolboxPage");
    setStyleSheet(
        "QFrame#toolboxInspector{background:white;border:1px solid #D5DAE3;border-radius:10px;}"
        "QLabel#toolboxTitle{font-size:18px;font-weight:600;color:#202124;}"
        "QLabel#toolboxCount,QLabel#toolboxAccess{color:#667085;}"
        "QPushButton#toolboxRun{background:#C8102E;color:white;border:0;border-radius:6px;padding:9px 16px;}"
        "QPushButton#toolboxRun:disabled{background:#E7E9EE;color:#747C8A;}"
        "QPlainTextEdit#toolboxCommand{background:#F7F8FA;color:#202124;border:1px solid #D5DAE3;border-radius:5px;}"
        "QPlainTextEdit#toolboxOutput{background:white;color:#202124;border:1px solid #D5DAE3;border-radius:5px;}"
    );
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto* introduction = wrappedLabel(
        experimental_ ? QStringLiteral("Experimental changes · Review each operation before applying it. These tools change settings or remove cached data.")
                      : QStringLiteral("Inspect your system with reviewed, read-only tools. Select a tool to see its scope and exact command."),
        "toolboxIntroduction");
    layout->addWidget(introduction);

    auto* toolbar = new QHBoxLayout;
    search_ = new QLineEdit;
    search_->setObjectName("toolboxSearch");
    search_->setAccessibleName("Filter system tools");
    search_->setPlaceholderText("Filter tools by name or purpose");
    search_->setClearButtonEnabled(true);
    toolbar->addWidget(search_, 1);
    count_ = wrappedLabel({}, "toolboxCount");
    toolbar->addWidget(count_);
    layout->addLayout(toolbar);

    auto* vertical = new QSplitter(Qt::Vertical);
    vertical->setObjectName("toolboxVerticalSplit");
    auto* workspace = new QSplitter(Qt::Horizontal);
    workspace->setObjectName("toolboxWorkspaceSplit");
    tools_ = new QTableWidget(0, 3);
    tools_->setObjectName("toolboxTools");
    tools_->setAccessibleName("Available system tools");
    tools_->setHorizontalHeaderLabels({"Tool", "Group", "Access"});
    tools_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tools_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tools_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tools_->verticalHeader()->hide();
    tools_->verticalHeader()->setDefaultSectionSize(42);
    tools_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tools_->setSelectionMode(QAbstractItemView::SingleSelection);
    tools_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tools_->setAlternatingRowColors(true);
    tools_->setWordWrap(false);
    workspace->addWidget(tools_);

    auto* inspector = new QFrame;
    inspector->setObjectName("toolboxInspector");
    inspector->setMinimumWidth(260);
    auto* detailLayout = new QVBoxLayout(inspector);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailLayout->setSpacing(12);
    detailLayout->addWidget(wrappedLabel("Tool details", "toolboxInspectorHeading"));
    title_ = wrappedLabel("Select a tool", "toolboxTitle");
    detailLayout->addWidget(title_);
    description_ = wrappedLabel("Select an entry to inspect its purpose and command.", "toolboxDescription");
    detailLayout->addWidget(description_);
    access_ = wrappedLabel({}, "toolboxAccess");
    detailLayout->addWidget(access_);
    detailLayout->addWidget(wrappedLabel("Command preview", "toolboxCommandHeading"));
    command_ = new QPlainTextEdit;
    command_->setObjectName("toolboxCommand");
    command_->setAccessibleName("Exact command preview, never evaluated by a shell");
    command_->setReadOnly(true);
    command_->setMaximumHeight(105);
    command_->setMinimumHeight(66);
    detailLayout->addWidget(command_);
    runButton_ = new QPushButton("Review diagnostic…");
    runButton_->setObjectName("toolboxRun");
    runButton_->setAccessibleName("Review selected tool before running");
    runButton_->setEnabled(false);
    detailLayout->addWidget(runButton_);
    detailLayout->addStretch();
    workspace->addWidget(inspector);
    workspace->setStretchFactor(0, 3);
    workspace->setStretchFactor(1, 2);
    workspace->setSizes({580, 340});
    vertical->addWidget(workspace);

    auto* activity = new QWidget;
    auto* activityLayout = new QVBoxLayout(activity);
    activityLayout->setContentsMargins(0, 0, 0, 0);
    auto* activityToolbar = new QHBoxLayout;
    status_ = wrappedLabel("Ready · No operation has run.", "toolboxStatus");
    activityToolbar->addWidget(status_, 1);
    stopButton_ = new QPushButton("Stop diagnostic");
    stopButton_->setObjectName("toolboxStop");
    stopButton_->setEnabled(false);
    activityToolbar->addWidget(stopButton_);
    exportButton_ = new QPushButton("Export report…");
    exportButton_->setObjectName("toolboxExport");
    exportButton_->setEnabled(false);
    activityToolbar->addWidget(exportButton_);
    activityLayout->addLayout(activityToolbar);
    auto* activityTabs = new QTabWidget;
    activityTabs->setObjectName("toolboxActivityTabs");
    output_ = new QPlainTextEdit;
    output_->setObjectName("toolboxOutput");
    output_->setAccessibleName("Local diagnostic output");
    output_->setReadOnly(true);
    output_->setPlaceholderText("Diagnostic output appears here after you review and run a tool. Nothing is uploaded.");
    activityTabs->addTab(output_, "Output");
    historyTable_ = new QTableWidget(0, 3);
    historyTable_->setObjectName("toolboxHistory");
    historyTable_->setAccessibleName("Recent tool outcomes, newest first");
    historyTable_->setHorizontalHeaderLabels({"Task", "Outcome", "Seconds"});
    historyTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    historyTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    historyTable_->verticalHeader()->hide();
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activityTabs->addTab(historyTable_, "Recent activity");
    activityLayout->addWidget(activityTabs, 1);
    vertical->addWidget(activity);
    vertical->setStretchFactor(0, 3);
    vertical->setStretchFactor(1, 2);
    vertical->setSizes({360, 190});
    layout->addWidget(vertical, 1);

    const auto& catalog = tux::actions();
    for (int index = 0; index < catalog.size(); ++index) {
        const auto& tool = catalog[index];
        const bool included = experimental_ ? tool.changes
                                           : !tool.changes && (groups.isEmpty() || groups.contains(tool.group));
        if (!included) continue;
        catalogIndices_.append(index);
        const int row = tools_->rowCount();
        tools_->insertRow(row);
        auto* name = new QTableWidgetItem(tool.title);
        name->setData(Qt::UserRole, index);
        name->setToolTip(tool.description);
        tools_->setItem(row, 0, name);
        tools_->setItem(row, 1, new QTableWidgetItem(tool.group));
        tools_->setItem(row, 2, new QTableWidgetItem(tool.admin ? "Administrator" : tool.changes ? "Changes settings" : "Read only"));
    }
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(tools_, &QTableWidget::itemSelectionChanged, this, [this] { updateSelection(); });
    connect(runButton_, &QPushButton::clicked, this, [this] { reviewAndRun(); });
    connect(stopButton_, &QPushButton::clicked, this, [this] { requestStop(); });
    connect(exportButton_, &QPushButton::clicked, this, [this] { exportReport(); });
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(50);
    connect(pollTimer_, &QTimer::timeout, this, [this] { poll(); });
    filter();
}

ToolboxPage::~ToolboxPage() {
    pollTimer_->stop();
    if (run_ && cancellable_) run_->cancel.store(true, std::memory_order_relaxed);
    // The worker owns only its shared run record and never this QWidget. Main
    // window close guards normally wait for a recorded completion; the fallback
    // remains safe even when a parent is forcibly destroyed during a query.
    if (worker_.joinable()) worker_.detach();
}

int ToolboxPage::toolCount() const { return catalogIndices_.size(); }

void ToolboxPage::setSearch(const QString& query) { search_->setText(query); }

void ToolboxPage::setExecutionAllowed(bool allowed) {
    if (executionAllowed_ == allowed) return;
    executionAllowed_ = allowed;
    updateSelection();
}

int ToolboxPage::selectedCatalogIndex() const {
    const int row = tools_->currentRow();
    if (row < 0 || tools_->isRowHidden(row) || !tools_->item(row, 0)) return -1;
    return tools_->item(row, 0)->data(Qt::UserRole).toInt();
}

void ToolboxPage::filter() {
    int visible = 0;
    int first = -1;
    for (int row = 0; row < tools_->rowCount(); ++row) {
        const auto index = tools_->item(row, 0)->data(Qt::UserRole).toInt();
        const bool matches = tux::matches(tux::actions()[index], search_->text());
        tools_->setRowHidden(row, !matches);
        if (matches) {
            ++visible;
            if (first < 0) first = row;
        }
    }
    count_->setText(visible ? QStringLiteral("%1 of %2 tools").arg(visible).arg(toolCount())
                           : QStringLiteral("No matching tools"));
    if (selectedCatalogIndex() < 0) {
        tools_->clearSelection();
        if (first >= 0) tools_->setCurrentCell(first, 0);
        else tools_->setCurrentCell(-1, -1);
    }
    updateSelection();
}

void ToolboxPage::updateSelection() {
    const int index = selectedCatalogIndex();
    if (index < 0) {
        title_->setText("No tool selected");
        description_->setText("Choose another filter or select a tool from the list.");
        command_->clear();
        access_->clear();
        runButton_->setEnabled(false);
        return;
    }
    const auto& tool = tux::actions()[index];
    title_->setText(tool.title);
    description_->setText(tool.description);
    const auto command = reviewCommand(tool,injectedExecutor_);
    command_->setPlainText(tux::preview(command));
    const auto problem = injectedExecutor_ ? QString{} : tux::unavailable(tool);
    access_->setText(!problem.isEmpty() ? problem : tool.admin ? (tool.changes ? "Administrator approval required · Experimental" : "Read only · Administrator approval required")
                                 : tool.changes ? "Changes settings · Experimental" : "Read only · No administrator approval");
    runButton_->setText(tool.changes ? "Review change…" : "Review diagnostic…");
    runButton_->setToolTip(!executionAllowed_ ? "Another SysPulse task is running. Wait for it to finish."
                          : problem.isEmpty() ? "Review scope and command before starting" : problem);
    runButton_->setEnabled(!busy_ && executionAllowed_ && problem.isEmpty());
}

void ToolboxPage::reviewAndRun() {
    if (busy_ || !executionAllowed_ || (canExecute && !canExecute())) return;
    const int index = selectedCatalogIndex();
    if (index < 0) return;
    const auto tool = tux::actions()[index];
    const auto problem = injectedExecutor_ ? QString{} : tux::unavailable(tool);
    if (!problem.isEmpty()) {
        QMessageBox::information(this, "Tool unavailable", problem);
        return;
    }
    const auto command = reviewCommand(tool,injectedExecutor_);
    QMessageBox review(QMessageBox::Question, "Review " + tool.title,
                       tool.description, QMessageBox::Ok | QMessageBox::Cancel, this);
    review.setObjectName("toolboxReview");
    review.setTextFormat(Qt::PlainText);
    const QString scope = tool.changes ? "This operation changes your system. Review irreversible effects before continuing."
                                      : "This diagnostic reads system state and displays its output locally.";
    review.setInformativeText(scope + "\n\n" +
        (tool.admin ? "Administrator approval will be requested.\n\n" : QString{}) +
        "Exact command:\n" + tux::preview(command));
    review.setDefaultButton(QMessageBox::Cancel);
    review.setEscapeButton(QMessageBox::Cancel);
    review.button(QMessageBox::Ok)->setText(tool.changes ? "Run reviewed change" : "Run diagnostic");
    if (review.exec() != QMessageBox::Ok) return;
    // The modal review runs a nested event loop. The shell can revoke admission
    // while it is open, so recheck immediately before creating the worker.
    if (busy_ || !executionAllowed_ || (canExecute && !canExecute())) return;

    // No command is accepted from the text preview or any editable UI field.
    busy_ = true;
    cancellable_ = !tool.changes && !tool.admin;
    runningCatalogIndex_ = index;
    run_ = std::make_shared<SharedRun>();
    decoder_.reset(QTextCodec::codecForName("UTF-8")->makeDecoder());
    output_->clear();
    status_->setText("Running · " + tool.title);
    stopButton_->setEnabled(cancellable_);
    exportButton_->setEnabled(false);
    updateSelection();
    if (onBusyChanged) onBusyChanged(true);
    const auto shared = run_;
    const auto injected = executor_;
    try {
    worker_ = std::thread([shared, tool, injected] {
        const auto output = [shared](const QByteArray& bytes) {
            const std::lock_guard<std::mutex> lock(shared->mutex);
            shared->pending.append(bytes);
            if (shared->pending.size() > tux::OutputLimit) {
                shared->pending = shared->pending.right(tux::OutputLimit);
                shared->streamTruncated = true;
            }
        };
        tux::Result result;
        try {
            result = injected ? injected(tool.id, output, &shared->cancel)
                              : tux::execute(tool.id, true, 120000, output, &shared->cancel);
        } catch (const std::exception& error) {
            result.code = -1;
            result.output = QByteArray("Unable to complete tool: ") + error.what();
            output(result.output);
        } catch (...) {
            result.code = -1;
            result.output = "Unable to complete tool: unknown error.";
            output(result.output);
        }
        result.action = tool.title;
        if (result.output.size() > tux::OutputLimit) {
            result.output = result.output.right(tux::OutputLimit);
            result.truncated = true;
        }
        const std::lock_guard<std::mutex> lock(shared->mutex);
        shared->completed = std::move(result);
    });
    } catch (const std::exception& error) {
        tux::Result result;
        result.action = tool.title;
        result.code = -1;
        result.output = QByteArray("Cannot start task worker: ") + error.what();
        const std::lock_guard<std::mutex> lock(shared->mutex);
        shared->completed = std::move(result);
    }
    pollTimer_->start();
}

void ToolboxPage::requestStop() {
    if (!isCancellable() || !run_) return;
    run_->cancel.store(true, std::memory_order_relaxed);
    stopButton_->setEnabled(false);
    status_->setText("Stopping diagnostic…");
}

void ToolboxPage::appendOutput(const QString& text) {
    output_->moveCursor(QTextCursor::End);
    output_->insertPlainText(text);
    if (output_->document()->characterCount() > tux::OutputLimit) {
        auto cursor = output_->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                            output_->document()->characterCount() - tux::OutputLimit);
        cursor.removeSelectedText();
    }
    output_->moveCursor(QTextCursor::End);
}

void ToolboxPage::poll() {
    if (!run_) return;
    QByteArray bytes;
    bool streamTruncated = false;
    std::optional<tux::Result> completed;
    {
        const std::lock_guard<std::mutex> lock(run_->mutex);
        bytes.swap(run_->pending);
        streamTruncated = run_->streamTruncated;
        run_->streamTruncated = false;
        completed.swap(run_->completed);
    }
    if (streamTruncated) {
        decoder_.reset(QTextCodec::codecForName("UTF-8")->makeDecoder());
        output_->clear();
        appendOutput("[Earlier live output was truncated to keep this view bounded.]\n");
    }
    if (!bytes.isEmpty()) appendOutput(decoder_->toUnicode(bytes));
    if (!completed) return;
    pollTimer_->stop();
    if (worker_.joinable()) worker_.join();
    const auto result = std::move(*completed);
    if (output_->toPlainText().isEmpty() && !result.output.isEmpty())
        appendOutput(QString::fromUtf8(result.output));
    history_.append(result);
    if (history_.size() > HistoryLimit) history_.remove(0, history_.size() - HistoryLimit);
    historyTable_->setRowCount(history_.size());
    for (int row = 0; row < history_.size(); ++row) {
        const auto& entry = history_[history_.size() - 1 - row];
        historyTable_->setItem(row, 0, new QTableWidgetItem(entry.action));
        historyTable_->setItem(row, 1, new QTableWidgetItem(resultState(entry)));
        historyTable_->setItem(row, 2, new QTableWidgetItem(QString::number(entry.seconds, 'f', 2)));
    }
    status_->setText(resultState(result) + " · " + result.action +
                     (result.truncated ? " · Output truncated" : QString{}));
    busy_ = false;
    cancellable_ = false;
    runningCatalogIndex_ = -1;
    run_.reset();
    stopButton_->setEnabled(false);
    exportButton_->setEnabled(!history_.isEmpty());
    updateSelection();
    if (onResult) onResult(result);
    if (onBusyChanged) onBusyChanged(false);
}

QJsonObject ToolboxPage::summaryReport() const { return reportFor(history_, false); }

void ToolboxPage::exportReport() {
    if (history_.isEmpty() || busy_) return;
    // Snapshot and consent are scoped to this one dialog; details are off again
    // on the next export, even when a detailed preview was previously cancelled.
    const auto snapshot = history_;
    QDialog review(this);
    review.setObjectName("toolboxReportReview");
    review.setWindowTitle("Review SysPulse report");
    review.resize(800, 600);
    auto* layout = new QVBoxLayout(&review);
    layout->addWidget(wrappedLabel(
        "Summary excludes command lines and diagnostic output. Review the exact JSON before saving. Nothing is uploaded.",
        "toolboxReportNotice"));
    auto* details = new QCheckBox("Include commands and diagnostic output (may contain private data)");
    details->setObjectName("toolboxReportDetails");
    details->setChecked(false);
    layout->addWidget(details);
    auto* preview = new QPlainTextEdit;
    preview->setObjectName("toolboxReportPreview");
    preview->setAccessibleName("Exact report export preview");
    preview->setReadOnly(true);
    layout->addWidget(preview, 1);
    const auto refresh = [preview, details, snapshot] {
        preview->setPlainText(QString::fromUtf8(QJsonDocument(reportFor(snapshot, details->isChecked())).toJson()));
    };
    connect(details, &QCheckBox::toggled, &review, [refresh] { refresh(); });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setObjectName("toolboxReportSave");
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &review, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &review, [this, &review, preview] {
        const auto reviewed = preview->toPlainText().toUtf8();
        const auto path = QFileDialog::getSaveFileName(&review, "Save reviewed SysPulse report",
                                                       "syspulse-report.json", "JSON reports (*.json)");
        if (path.isEmpty()) return;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) ||
            !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
            file.write(reviewed) != reviewed.size() || !file.commit()) {
            QMessageBox::critical(this, "Report export failed", "The report was not saved. " + file.errorString());
            return;
        }
        review.accept();
    });
    refresh();
    review.exec();
}

}
