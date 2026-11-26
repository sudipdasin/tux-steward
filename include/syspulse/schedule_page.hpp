// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QWidget>
#include <memory>
namespace syspulse {
class SchedulePage final : public QWidget {
public:
    explicit SchedulePage(QWidget* parent=nullptr);
    ~SchedulePage() override;
    bool isBusy() const;
    QString automationSummary() const;
    std::function<bool()> canExecute;
    std::function<void(bool)> onBusyChanged;
    std::function<void(const tux::Result&)> onResult;
private:struct Impl;std::unique_ptr<Impl> d;
};
}
