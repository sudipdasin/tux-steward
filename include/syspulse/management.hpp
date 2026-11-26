// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "tux/autostart.hpp"
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <functional>
#include <memory>

class QHideEvent;
class QShowEvent;

namespace syspulse {
namespace management {
struct ServiceEntry {
    QString unit;
    QString runtime = QStringLiteral("Not loaded");
    QString substate;
    QString startup = QStringLiteral("Unknown");
    QString description;
    QString loadState;
    bool user = false;
};
bool safeServiceUnit(const QString& unit);
QVector<ServiceEntry> parseServiceLists(const QByteArray& unitFiles,
                                      const QByteArray& runtimeUnits, bool userScope);
QStringList availableServiceActions(const ServiceEntry& entry);

// Service-operation state only; this does not execute or authorize a command.
// Updating the state before notifying observers lets admission/close guards
// read the new value synchronously, without a repeating GUI polling timer.
class BusyState final {
public:
    bool isBusy() const noexcept { return busy_; }
    explicit operator bool() const noexcept { return busy_; }
    void transition(bool busy, const std::function<void(bool)>& changed)
    {
        if (busy_ == busy) return;
        busy_ = busy;
        if (changed) changed(busy);
    }
private:
    bool busy_ = false;
};
}

// Native management views. Merely opening the widget never changes the system.
// fixtureMode is an explicit, read-only presentation/test mode; it never invokes
// systemctl or permits startup/service mutations.
class ManagementPage final : public QWidget {
public:
    struct Options {
        bool fixtureMode = false;
        // Empty paths use the environment in live mode and an empty dataset in
        // fixture mode. A fixture must explicitly supply its temporary paths.
        tux::autostart::Paths startupPaths;
        QByteArray serviceUnitFilesFixture;
        QByteArray serviceRuntimeFixture;
        QByteArray serviceDetailsFixture;
    };
    explicit ManagementPage(QWidget* parent = nullptr);
    explicit ManagementPage(Options options, QWidget* parent = nullptr);
    ~ManagementPage() override;
    void showStartup();
    void showServices();
    bool hasPendingAction() const;
    bool isBusy() const; // Mutations only; read-only queries may be cancelled.
    bool fixtureMode() const;
    // Main-shell admission guard, checked before and after modal review so a
    // different module cannot begin a simultaneous system-changing task.
    std::function<bool()> canMutate;
    // UI-thread notification after the service mutation state changes. Fires
    // synchronously once per busy/idle edge; read-only queries do not emit it.
    std::function<void(bool)> onBusyChanged;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
