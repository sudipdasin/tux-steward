// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/toolbox.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool waitUntil(QApplication& app, const std::function<bool()>& ready, int timeout = 10000) {
    QElapsedTimer clock;
    clock.start();
    while (!ready() && clock.elapsed() < timeout) {
        app.processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    app.processEvents();
    return ready();
}

void answerReview(QMessageBox::StandardButton answer, bool* defaultCancel = nullptr) {
    QTimer::singleShot(0, [answer, defaultCancel] {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() != "toolboxReview") continue;
            auto* box = qobject_cast<QMessageBox*>(widget);
            if (!box) continue;
            if (defaultCancel)
                *defaultCancel = box->defaultButton() == box->button(QMessageBox::Cancel);
            box->done(answer);
            return;
        }
    });
}

int visibleRows(QTableWidget* table) {
    int count = 0;
    for (int row = 0; row < table->rowCount(); ++row)
        if (!table->isRowHidden(row)) ++count;
    return count;
}

void invokeRun(syspulse::ToolboxPage& page) {
    auto* button = page.findChild<QPushButton*>("toolboxRun");
    require(button && button->isEnabled(), "review action must be available");
    button->click();
}
}

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setApplicationName("SysPulse");
    app.setApplicationVersion("3.0.0-alpha.1");
    try {
        std::atomic<int> executions{0};
        auto successful = [&executions](const QString& id, tux::Output output,
                                       const std::atomic_bool*) {
            ++executions;
            require(id == "storage", "injected executor receives only selected catalog ID");
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            output("Synthetic private detail: /fixture/private-user/machine-name\n");
            tux::Result result;
            result.code = 0;
            result.command = QStringList{"df", "-hT"};
            result.output = "Synthetic private detail: /fixture/private-user/machine-name\n";
            result.seconds = 0.08;
            return result;
        };
        syspulse::ToolboxPage stable({}, false, successful);
        syspulse::ToolboxPage changes({}, true, successful);
        int readOnly = 0;
        int mutableTools = 0;
        for (const auto& action : tux::actions()) action.changes ? ++mutableTools : ++readOnly;
        require(stable.toolCount() == readOnly, "stable catalog includes every read-only action");
        require(changes.toolCount() == mutableTools, "experimental catalog includes every mutable action");
        require(stable.toolCount() + changes.toolCount() == tux::actions().size(), "catalog partition must not drop tools");
        auto* stableTable = stable.findChild<QTableWidget*>("toolboxTools");
        auto* changeTable = changes.findChild<QTableWidget*>("toolboxTools");
        require(stableTable && changeTable, "tool tables exist");
        for (int row = 0; row < stableTable->rowCount(); ++row)
            require(!tux::actions()[stableTable->item(row, 0)->data(Qt::UserRole).toInt()].changes,
                    "stable page cannot expose settings-changing tools");
        for (int row = 0; row < changeTable->rowCount(); ++row)
            require(tux::actions()[changeTable->item(row, 0)->data(Qt::UserRole).toInt()].changes,
                    "experimental page contains only settings-changing tools");
        require(executions == 0 && !stable.isBusy(), "construction must never run commands");

        syspulse::ToolboxPage storage({"Storage", "Maintenance"}, false, successful);
        int storageExpected = 0;
        for (const auto& action : tux::actions())
            if (!action.changes && (action.group == "Storage" || action.group == "Maintenance")) ++storageExpected;
        require(storage.toolCount() == storageExpected, "group filtering preserves requested tools only");

        stable.setSearch("Vulkan");
        int vulkanMatches = 0;
        for (const auto& action : tux::actions())
            if (!action.changes && tux::matches(action, "Vulkan")) ++vulkanMatches;
        require(visibleRows(stableTable) == vulkanMatches, "search filters through catalog semantics");
        stable.setSearch("no-such-fixture-tool-7321");
        require(visibleRows(stableTable) == 0, "no-results search hides every row");
        require(!stable.findChild<QPushButton*>("toolboxRun")->isEnabled(), "no-results state disables run");
        stable.setSearch("mounted filesystems free space");
        require(visibleRows(stableTable) == 1, "search selects exact known fixture tool");
        require(stable.findChild<QPlainTextEdit*>("toolboxCommand")->toPlainText().contains("'df'"), "inspector shows quoted preview");
        require(stable.findChild<QPlainTextEdit*>("toolboxCommand")->isReadOnly(), "command preview is not executable input");
        require(executions == 0, "selection and search cannot execute tools");

        stable.setExecutionAllowed(false);
        require(!stable.findChild<QPushButton*>("toolboxRun")->isEnabled(), "global admission guard disables idle run");
        stable.setSearch("mounted filesystems free space");
        require(!stable.findChild<QPushButton*>("toolboxRun")->isEnabled(), "filtering cannot bypass global guard");
        stable.setExecutionAllowed(true);
        require(stable.findChild<QPushButton*>("toolboxRun")->isEnabled(), "global guard can restore idle run");

        QTimer::singleShot(0, [&stable] {
            stable.setExecutionAllowed(false);
            for (auto* widget : QApplication::topLevelWidgets())
                if (widget->objectName() == "toolboxReview")
                    qobject_cast<QMessageBox*>(widget)->done(QMessageBox::Ok);
        });
        invokeRun(stable);
        require(executions == 0 && !stable.isBusy(), "admission revoked during modal review prevents execution");
        stable.setExecutionAllowed(true);

        bool liveAdmission=true;
        stable.canExecute=[&liveAdmission]{return liveAdmission;};
        QTimer::singleShot(0,[&liveAdmission]{
            liveAdmission=false;
            for(auto* widget:QApplication::topLevelWidgets())
                if(widget->objectName()=="toolboxReview")qobject_cast<QMessageBox*>(widget)->done(QMessageBox::Ok);
        });
        invokeRun(stable);
        require(executions==0&&!stable.isBusy(),"live cross-module guard is rechecked after modal review despite cached admission");
        stable.canExecute={};

        bool defaultCancel = false;
        answerReview(QMessageBox::Cancel, &defaultCancel);
        invokeRun(stable);
        require(defaultCancel && executions == 0 && !stable.isBusy(), "default-cancel review must preserve idle state");

        int callbacks = 0;
        stable.onResult = [&callbacks](const tux::Result& result) {
            require(result.action == "Storage usage", "result callback identifies catalog action");
            ++callbacks;
        };
        answerReview(QMessageBox::Ok);
        invokeRun(stable);
        require(stable.isBusy(), "accepted diagnostic runs asynchronously");
        bool eventLoopResponsive = false;
        QTimer::singleShot(0, [&eventLoopResponsive] { eventLoopResponsive = true; });
        require(waitUntil(app, [&] { return !stable.isBusy(); }), "diagnostic completion arrives");
        require(eventLoopResponsive && callbacks == 1 && executions == 1, "GUI remains responsive and completes once");
        require(stable.findChild<QLabel*>("toolboxStatus")->text().startsWith("Completed"), "successful result has completed status");
        require(stable.findChild<QPlainTextEdit*>("toolboxOutput")->toPlainText().contains("private-user"), "raw output remains locally visible");
        const auto summary = stable.summaryReport();
        const auto summaryText = QString::fromUtf8(QJsonDocument(summary).toJson());
        require(summary.value("application").toString() == "SysPulse", "report identifies SysPulse");
        require(summary.value("mode").toString() == "summary", "report defaults to summary");
        require(!summaryText.contains("private-user") && !summaryText.contains("\"command\"") && !summaryText.contains("\"output\""), "summary excludes diagnostic text and commands");
        require(summary.value("results").toArray().size() == 1, "summary includes completed result");

        bool detailsPreviewed = false;
        QTimer::singleShot(0, [&detailsPreviewed] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (widget->objectName() != "toolboxReportReview") continue;
                auto* details = widget->findChild<QCheckBox*>("toolboxReportDetails");
                auto* preview = widget->findChild<QPlainTextEdit*>("toolboxReportPreview");
                detailsPreviewed = details && preview && !details->isChecked() && !preview->toPlainText().contains("private-user");
                details->setChecked(true);
                detailsPreviewed = detailsPreviewed && preview->toPlainText().contains("private-user");
                qobject_cast<QDialog*>(widget)->reject();
            }
        });
        stable.findChild<QPushButton*>("toolboxExport")->click();
        require(detailsPreviewed, "detailed export requires explicit opt-in");
        bool resetConsent = false;
        QTimer::singleShot(0, [&resetConsent] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (widget->objectName() != "toolboxReportReview") continue;
                resetConsent = !widget->findChild<QCheckBox*>("toolboxReportDetails")->isChecked();
                qobject_cast<QDialog*>(widget)->reject();
            }
        });
        stable.findChild<QPushButton*>("toolboxExport")->click();
        require(resetConsent, "cancelled detailed export cannot retain consent");

        syspulse::ToolboxPage failed({"Storage"}, false,
            [](const QString&, tux::Output, const std::atomic_bool*) -> tux::Result {
                throw std::runtime_error("fixture query failed");
            });
        failed.setSearch("mounted filesystems free space");
        answerReview(QMessageBox::Ok);
        invokeRun(failed);
        require(waitUntil(app, [&] { return !failed.isBusy(); }), "failed worker resolves");
        require(failed.findChild<QLabel*>("toolboxStatus")->text().startsWith("Needs attention"), "exception never appears as success");

        std::atomic_bool entered{false};
        syspulse::ToolboxPage cancellable({"Storage"}, false,
            [&entered](const QString&, tux::Output, const std::atomic_bool* cancel) {
                entered = true;
                while (!cancel->load(std::memory_order_relaxed))
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                tux::Result result;
                result.code = 130;
                result.cancelled = true;
                return result;
            });
        cancellable.setSearch("mounted filesystems free space");
        answerReview(QMessageBox::Ok);
        invokeRun(cancellable);
        require(waitUntil(app, [&] { return entered.load(); }), "cancellable fixture starts");
        require(cancellable.isCancellable(), "read-only task permits cancellation");
        cancellable.requestStop();
        require(waitUntil(app, [&] { return !cancellable.isBusy(); }), "cancel resolves without blocking UI");
        require(cancellable.findChild<QLabel*>("toolboxStatus")->text().startsWith("Cancelled"), "cancel is a distinct outcome");

        std::atomic_bool sawUnsafeCancel{false};
        syspulse::ToolboxPage mutablePage({}, true,
            [&sawUnsafeCancel](const QString&, tux::Output, const std::atomic_bool* cancel) {
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
                sawUnsafeCancel = cancel->load(std::memory_order_relaxed);
                tux::Result result;
                result.code = 0;
                return result;
            });
        mutablePage.setSearch("clean downloaded packages");
        answerReview(QMessageBox::Ok);
        invokeRun(mutablePage);
        require(!mutablePage.isCancellable(), "mutable tasks cannot be interrupted through diagnostic cancel");
        mutablePage.requestStop();
        require(waitUntil(app, [&] { return !mutablePage.isBusy(); }), "simulated mutable task finishes");
        require(!sawUnsafeCancel.load(), "stop request does not cancel a mutable operation");

        // Render only deterministic fixture text, not real machine diagnostics.
        stable.findChild<QPlainTextEdit*>("toolboxOutput")->setPlainText(
            "Fixture diagnostic completed. Sample data only.\nSummary exports exclude command lines and raw diagnostics.");
        stable.resize(1100, 640);
        stable.show();
        app.processEvents();
        if (argc > 1) require(stable.grab().save(QString::fromLocal8Bit(argv[1])+".png"), "fixture screenshot saved");
        std::cout << "SysPulse toolbox: catalog preservation, filters, default-cancel review, async execution, failure/cancel states, privacy and opt-in export passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
