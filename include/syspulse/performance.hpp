// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QString>
#include <functional>
#include <memory>

class QTabWidget;

namespace syspulse {

class PerformancePage final : public QWidget {
public:
    explicit PerformancePage(QWidget* parent = nullptr);
    ~PerformancePage() override;
    QString cpuValue() const;
    QString memoryValue() const;
    QString diskValue() const;
    QString networkValue() const;
    void setPaused(bool paused);
    bool isPaused() const;
    void setSamplingEnabled(bool enabled);
    void refresh();
    QTabWidget* detailTabs() const;

    std::function<void()> onMetricsChanged;
    std::function<void()> onOpenStartup;
    std::function<void()> onOpenServices;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace syspulse
