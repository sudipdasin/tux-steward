// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <utility>

struct AsyncCommandResult {
    QByteArray output;
    QString error;
    int exitCode = -1;
    bool timedOut = false;
    bool cancelled = false;
    bool truncated = false;
    bool ok() const {
        return exitCode == 0 && error.isEmpty() && !timedOut && !cancelled && !truncated;
    }
};

// Single-use, shell-free command. Output is capped while it arrives; all slots
// and completion callbacks run on the owner's Qt thread. Deleting the owner
// destroys the query without delivering a callback to a dead receiver.
// Queries have a deadline and may be cancelled. Reviewed service actions keep
// their busy state until actual process exit: stopping observation or trying
// to signal a privileged helper does not prove that its mutation has stopped.
class AsyncCommand final : public QObject {
public:
    using Completion = std::function<void(AsyncCommandResult)>;
    static AsyncCommand *run(QObject *owner, const QString &program, const QStringList &arguments,
                             int timeoutMs, int outputLimit, Completion completion) {
        auto *query = new AsyncCommand(owner, outputLimit, std::move(completion));
        query->deadline_.start(qMax(1, timeoutMs));
        query->process_.start(program, arguments);
        return query;
    }
    static AsyncCommand *runAction(QObject *owner, const QString &program, const QStringList &arguments,
                                   int outputLimit, Completion completion) {
        auto *action = new AsyncCommand(owner, outputLimit, std::move(completion));
        action->waitForExit_ = true;
        action->process_.start(program, arguments);
        return action;
    }
    ~AsyncCommand() override {
        disconnect(&process_, nullptr, this, nullptr);
        disconnect(&deadline_, nullptr, this, nullptr);
        deadline_.stop();
        if (process_.state() != QProcess::NotRunning) process_.kill();
    }
    bool active() const { return active_; }
    void cancel() {
        if (!active_ || waitForExit_) return;
        result_.cancelled = true;
        process_.kill();
        finish(125);
    }
private:
    AsyncCommand(QObject *owner, int limit, Completion completion)
        : QObject(owner), limit_(qMax(1, limit)), completion_(std::move(completion)) {
        process_.setProcessChannelMode(QProcess::MergedChannels);
        process_.setStandardInputFile(QProcess::nullDevice());
        deadline_.setSingleShot(true);
        connect(&process_, &QProcess::readyReadStandardOutput, this, [this] { drain(); });
        connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus status) {
            if (!active_) return;
            drain();
            if (status != QProcess::NormalExit) result_.error = tr("Query process terminated unexpectedly.");
            finish(status == QProcess::NormalExit ? code : -1);
        });
        connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (active_ && error == QProcess::FailedToStart) {
                result_.error = process_.errorString();
                finish(127);
            }
        });
        connect(&deadline_, &QTimer::timeout, this, [this] {
            if (!active_) return;
            drain();
            result_.timedOut = true;
            process_.kill();
            finish(124);
        });
    }
    void drain() {
        const auto bytes = process_.readAllStandardOutput();
        if (!active_) return;
        const int room = qMax(0, limit_ - result_.output.size());
        if (bytes.size() > room) result_.truncated = true;
        result_.output.append(bytes.left(room));
    }
    void finish(int code) {
        if (!active_) return;
        active_ = false;
        deadline_.stop();
        result_.exitCode = code;
        auto callback = std::move(completion_);
        const auto result = std::move(result_);
        if (process_.state() == QProcess::NotRunning) deleteLater();
        else connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     this, [this](int, QProcess::ExitStatus) { deleteLater(); });
        if (callback) callback(result);
    }
    QProcess process_;
    QTimer deadline_;
    int limit_;
    bool active_ = true;
    bool waitForExit_ = false;
    Completion completion_;
    AsyncCommandResult result_;
};
