// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QWidget>
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
namespace syspulse {
struct ToolAvailability {
    QString id, title, group, problem;
    bool experimental = false;
};
using AvailabilityProbe = std::function<QString(const tux::Action&)>;
QVector<ToolAvailability> auditToolAvailability(AvailabilityProbe probe = {});
// Only the existing native XFCE settings launchers, resolved in protected bins.
QString desktopSettingsExecutable(const QString& name);
class CapabilitiesPanel final : public QWidget {
public:
    explicit CapabilitiesPanel(QWidget* parent = nullptr);
    CapabilitiesPanel(AvailabilityProbe probe, QWidget* parent = nullptr);
    std::function<void(const QString&)> onOpenTool;
private:
    AvailabilityProbe probe_;
    QVector<ToolAvailability> snapshot_;
    QTableWidget* table_ = nullptr;
    QLabel* summary_ = nullptr;
    QLineEdit* search_ = nullptr;
    QPushButton* open_ = nullptr;
    QString checkedAt_;
    void refresh();
    void filter();
};
}
