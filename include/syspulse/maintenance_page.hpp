// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QWidget>
#include <memory>
namespace syspulse {
class MaintenancePage final : public QWidget {
public:
    explicit MaintenancePage(bool recovery=false,QWidget* parent=nullptr);
    ~MaintenancePage() override;
    bool isBusy() const;
    bool isCancellable() const;
    void requestStop();
    void refresh();
    std::function<bool()> canExecute;
    std::function<void(bool)> onBusyChanged;
    std::function<void(const tux::Result&)> onResult;
    std::function<void()> onOpenRecovery;
private:
    struct Impl;std::unique_ptr<Impl> d;
};
}
