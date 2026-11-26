// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/management.hpp"
#include "syspulse/window.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void require(bool condition, const char* message)
{
    ++checks; if (!condition) throw std::runtime_error(message);
}
void write(const QString& path, const QByteArray& contents)
{
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "create fixture directory");
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size(), "write startup fixture");
}
int visibleRows(QTableWidget* table)
{
    int count = 0;
    for (int row = 0; row < table->rowCount(); ++row) if (!table->isRowHidden(row)) ++count;
    return count;
}
bool selectId(QTableWidget* table, const QString& id)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0)->data(Qt::UserRole).toString() == id) {
            table->selectRow(row); return true;
        }
    }
    return false;
}
const syspulse::management::ServiceEntry* find(const QVector<syspulse::management::ServiceEntry>& entries, const QString& unit)
{
    for (const auto& entry : entries) if (entry.unit == unit) return &entry;
    return nullptr;
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    syspulse::applyPrecisionTheme();
    try {
        using namespace syspulse::management;
        {
            BusyState state;
            QStringList transitions;
            bool admissionOpen = true, closeAllowed = true;
            const auto notify = [&](bool busy) {
                require(state.isBusy() == busy, "busy observers read the updated guard state within the same call");
                admissionOpen = !busy;
                closeAllowed = !busy;
                transitions.append(busy ? QStringLiteral("busy") : QStringLiteral("idle"));
            };
            require(!state.isBusy(), "service mutation state starts idle without a notification");
            state.transition(true, notify);
            require(!admissionOpen && !closeAllowed && transitions == QStringList{QStringLiteral("busy")},
                    "starting a service mutation closes admission and the close guard synchronously without event processing");
            state.transition(true, notify);
            require(transitions.size() == 1, "an unchanged busy state does not generate redundant admission updates");
            state.transition(false, notify);
            require(admissionOpen && closeAllowed && transitions == QStringList{QStringLiteral("busy"), QStringLiteral("idle")},
                    "service completion immediately reopens admission and the close guard without a timer tick");
            state.transition(false, notify);
            require(transitions.size() == 2, "an unchanged idle state does not generate redundant admission updates");
            state.transition(true, {});
            require(state.isBusy(), "the guard remains correct before an observer is attached");
        }
        require(safeServiceUnit(QStringLiteral("NetworkManager.service")), "ordinary systemd unit allowed");
        require(safeServiceUnit(QStringLiteral("getty@tty1.service")), "systemd instantiated unit allowed");
        require(safeServiceUnit(QStringLiteral("modprobe@drm\\x2dcard0.service")), "escaped unit argument preserved");
        for (const auto& unsafe : {QStringLiteral("-evil.service"), QStringLiteral("../escape.service"),
                 QStringLiteral("name.service\nstop ssh.service"), QStringLiteral("name;shutdown.service"),
                 QStringLiteral("name.service\n"), QStringLiteral("name.service\r"),
                 QStringLiteral("name$(id).service"), QStringLiteral("name.socket"), QStringLiteral(" spaced.service")})
            require(!safeServiceUnit(unsafe), "unsafe or non-service argument refused");
        const QByteArray files =
            "UNIT FILE STATE PRESET\n"
            "NetworkManager.service enabled enabled\n"
            "bluetooth.service disabled enabled\n"
            "cups.service enabled enabled\n"
            "ssh.service disabled disabled\n"
            "masked.service masked enabled\n"
            "static.service static -\n"
            "../escape.service enabled -\n"
            "unsupported.service unknown-state -\n";
        const QByteArray runtime =
            "NetworkManager.service loaded active running Network connectivity\n"
            "bluetooth.service loaded inactive dead Bluetooth support\n"
            "cups.service loaded active running Print service\n"
            "ssh.service loaded failed failed Remote access\n"
            "masked.service masked inactive dead Masked fixture\n"
            "static.service loaded active exited Static unit\n"
            "transient.service loaded active running Dynamic fixture\n"
            "garbage\n"
            "bad.service loaded future-state dead Invalid state\n";
        const auto entries = parseServiceLists(files, runtime, false);
        require(entries.size() == 7, "service lists merge by identity and reject malformed entries");
        const auto* network = find(entries, QStringLiteral("NetworkManager.service"));
        require(network && !network->user && network->description == QStringLiteral("Network connectivity"), "runtime description and scope retained");
        auto actions = availableServiceActions(*network);
        require(actions.contains(QStringLiteral("stop")) && actions.contains(QStringLiteral("restart")) && actions.contains(QStringLiteral("disable")), "active enabled service permits runtime and independent startup actions");
        require(!actions.contains(QStringLiteral("start")) && !actions.contains(QStringLiteral("enable")), "redundant state actions disabled");
        const auto* bluetooth = find(entries, QStringLiteral("bluetooth.service"));
        actions = availableServiceActions(*bluetooth);
        require(actions.contains(QStringLiteral("start")) && actions.contains(QStringLiteral("enable")) && !actions.contains(QStringLiteral("stop")), "inactive disabled service exposes distinct start and enable");
        require(availableServiceActions(*find(entries, QStringLiteral("masked.service"))).isEmpty(), "masked units cannot be started or enabled through this surface");
        require(!availableServiceActions(*find(entries, QStringLiteral("static.service"))).contains(QStringLiteral("enable")), "static units cannot be enabled");
        require(find(entries, QStringLiteral("transient.service"))->startup == QStringLiteral("Unknown"), "runtime-only unit never falsely marked disabled");
        const auto userEntries = parseServiceLists(files, runtime, true);
        require(find(userEntries, QStringLiteral("NetworkManager.service"))->user, "user scope remains explicit in parsed identity");
        const auto fileOnly = parseServiceLists("offline.service disabled -\n", {}, false);
        require(fileOnly.size() == 1 && fileOnly[0].runtime == QStringLiteral("Not loaded"), "missing runtime data never fabricated as active");
        require(availableServiceActions(fileOnly[0]).isEmpty(), "unverified load state cannot enable mutations");
        auto invalidLoad = *bluetooth; invalidLoad.loadState = QStringLiteral("bad-setting");
        require(availableServiceActions(invalidLoad).isEmpty(), "invalid unit load state cannot start or enable a service");

        {
            syspulse::ManagementPage::Options emptyOptions; emptyOptions.fixtureMode = true;
            syspulse::ManagementPage emptyFixture(emptyOptions);
            require(emptyFixture.findChild<QTableWidget*>(QStringLiteral("startupTable"))->rowCount() == 0,
                    "empty fixture paths never fall back to real user startup configuration");
            emptyFixture.showServices();
            require(emptyFixture.findChild<QTableWidget*>(QStringLiteral("serviceTable"))->rowCount() == 0,
                    "empty fixture service data never falls back to live systemctl queries");
        }

        QTemporaryDir directory; require(directory.isValid(), "temporary fixture root");
        tux::autostart::Paths paths{directory.filePath(QStringLiteral("user")), {directory.filePath(QStringLiteral("system"))}, {QStringLiteral("XFCE")}};
        const auto system = [&](const QString& name) { return QDir(paths.configDirs[0]).filePath(QStringLiteral("autostart/") + name); };
        const auto user = [&](const QString& name) { return QDir(paths.configHome).filePath(QStringLiteral("autostart/") + name); };
        const QByteArray desktop = "[Desktop Entry]\nType=Application\nExec=/bin/true\n";
        write(system(QStringLiteral("update.desktop")), desktop + "Name=Update Manager\nOnlyShowIn=XFCE;\n");
        write(system(QStringLiteral("clipboard.desktop")), desktop + "Name=Clipboard Manager\nOnlyShowIn=XFCE;\n");
        write(system(QStringLiteral("bluetooth.desktop")), desktop + "Name=Bluetooth Applet\nOnlyShowIn=XFCE;\n");
        write(user(QStringLiteral("bluetooth.desktop")), desktop + "Name=Bluetooth Applet\nHidden=true\n");
        write(user(QStringLiteral("notes.desktop")), desktop + "Name=Notes\nOnlyShowIn=XFCE;\n");
        write(system(QStringLiteral("other.desktop")), desktop + "Name=Other Desktop App\nOnlyShowIn=GNOME;\n");
        {
            syspulse::ManagementPage::Options liveFixture;
            liveFixture.startupPaths = paths; // Live code, isolated temporary files only.
            syspulse::ManagementPage guarded(liveFixture);
            auto* table = guarded.findChild<QTableWidget*>(QStringLiteral("startupTable"));
            require(selectId(table, QStringLiteral("update.desktop")), "guard fixture startup entry selected");
            auto* change = guarded.findChild<QPushButton*>(QStringLiteral("startupDisable"));
            require(change && change->isEnabled(), "temporary live startup action available");
            int admissions = 0;
            guarded.canMutate = [&] { ++admissions; return false; };
            change->click();
            require(admissions == 1 && !QFileInfo::exists(user(QStringLiteral("update.desktop"))), "busy-module guard prevents even a startup override before modal review");
            admissions = 0;
            guarded.canMutate = [&] { return ++admissions == 1; };
            QTimer::singleShot(0, [] {
                for (auto* top : QApplication::topLevelWidgets())
                    if (auto* dialog = qobject_cast<QMessageBox*>(top)) dialog->done(QMessageBox::Yes);
            });
            change->click();
            require(admissions == 2 && !QFileInfo::exists(user(QStringLiteral("update.desktop"))), "busy-module guard is rechecked after modal review before any write");
        }
        syspulse::ManagementPage::Options options;
        options.fixtureMode = true; options.startupPaths = paths;
        options.serviceUnitFilesFixture = files; options.serviceRuntimeFixture = runtime;
        options.serviceDetailsFixture = "Id=NetworkManager.service\nLoadState=loaded\nActiveState=active\nSubState=running\nUnitFileState=enabled\nDescription=Network connectivity\nRequires=dbus.service\nAfter=network.target\n";
        syspulse::ManagementPage page(options);
        page.resize(1080, 680); page.show(); app.processEvents();
        require(page.fixtureMode() && !page.isBusy() && !page.hasPendingAction(), "explicit fixture mode is read-only and has no mutations");
        auto* startup = page.findChild<QTableWidget*>(QStringLiteral("startupTable"));
        auto* startupSearch = page.findChild<QLineEdit*>(QStringLiteral("startupSearch"));
        auto* source = page.findChild<QComboBox*>(QStringLiteral("startupSourceFilter"));
        auto* state = page.findChild<QComboBox*>(QStringLiteral("startupStateFilter"));
        auto* enable = page.findChild<QPushButton*>(QStringLiteral("startupEnable"));
        auto* disable = page.findChild<QPushButton*>(QStringLiteral("startupDisable"));
        require(startup && startupSearch && source && state && enable && disable, "complete startup management controls exist");
        require(startup->rowCount() == 5, "effective XDG entries shown without duplicate override");
        require(selectId(startup, QStringLiteral("bluetooth.desktop")), "startup selection identity independent of sort order");
        require(!enable->isEnabled() && !disable->isEnabled(), "fixture startup mutations disabled");
        require(page.findChild<QLabel*>(QStringLiteral("startupDetails"))->text().contains(QStringLiteral("User override")), "startup inspector distinguishes user override");
        if (argc > 1) require(page.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-startup.png")), "startup fixture screenshot saved");
        source->setCurrentIndex(1); require(visibleRows(startup) == 3, "system-source filter excludes user override and user-only entries");
        require(page.findChild<QLabel*>(QStringLiteral("startupSelectedName"))->text() == QStringLiteral("Select an application"), "filter clears hidden startup inspector selection");
        source->setCurrentIndex(2); require(visibleRows(startup) == 2, "user-source filter includes override and user-only entry");
        source->setCurrentIndex(0); state->setCurrentIndex(1); require(visibleRows(startup) == 3, "enabled filter respects desktop eligibility");
        state->setCurrentIndex(2); require(visibleRows(startup) == 1, "disabled filter respects effective Hidden flag");
        state->setCurrentIndex(3); require(visibleRows(startup) == 1, "unavailable filter distinguishes another desktop");
        state->setCurrentIndex(0); startupSearch->setText(QStringLiteral("CLIPBOARD")); require(visibleRows(startup) == 1, "startup search case-insensitive");
        startupSearch->clear();

        page.showServices(); app.processEvents();
        auto* services = page.findChild<QTableWidget*>(QStringLiteral("serviceTable"));
        auto* serviceSearch = page.findChild<QLineEdit*>(QStringLiteral("serviceSearch"));
        auto* serviceState = page.findChild<QComboBox*>(QStringLiteral("serviceStateFilter"));
        auto* serviceScope = page.findChild<QComboBox*>(QStringLiteral("serviceScopeFilter"));
        require(services && serviceSearch && serviceState && serviceScope, "service manager controls exist");
        require(services->rowCount() == 7, "service fixture loads merged table");
        require(selectId(services, QStringLiteral("NetworkManager.service")), "service selection uses stable unit identity");
        require(page.findChild<QLabel*>(QStringLiteral("serviceDependencies"))->text().contains(QStringLiteral("dbus.service")), "service inspector displays dependencies");
        for (const auto& verb : {QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("restart"), QStringLiteral("enable"), QStringLiteral("disable")})
            require(!page.findChild<QPushButton*>(QStringLiteral("serviceAction_") + verb)->isEnabled(), "all fixture service mutations disabled");
        if (argc > 1) require(page.grab().save(QString::fromLocal8Bit(argv[1]) + QStringLiteral("-services.png")), "services fixture screenshot saved");
        serviceState->setCurrentIndex(1); require(visibleRows(services) == 4, "running filter uses runtime state rather than startup state");
        serviceState->setCurrentIndex(3); require(visibleRows(services) == 1, "failed filter preserves independent runtime state");
        require(page.findChild<QLabel*>(QStringLiteral("serviceSelectedName"))->text() == QStringLiteral("Select a service"), "hidden service row cannot retain mutation context");
        serviceState->setCurrentIndex(5); require(visibleRows(services) == 2, "startup-disabled filter distinct from inactive runtime");
        serviceState->setCurrentIndex(0); serviceSearch->setText(QStringLiteral("PRINT")); require(visibleRows(services) == 1, "service search includes description");
        serviceSearch->clear();
        require(selectId(services, QStringLiteral("bluetooth.service")), "inactive service selected");
        require(page.findChild<QLabel*>(QStringLiteral("serviceDetails"))->text().contains(QStringLiteral("inactive")), "fixture selected details never overwrite another unit with a different sample identity");
        serviceScope->setCurrentIndex(1); app.processEvents();
        require(selectId(services, QStringLiteral("NetworkManager.service")), "scope refresh preserves queryable unit identity");
        require(page.findChild<QLabel*>(QStringLiteral("serviceDetails"))->text().contains(QStringLiteral("Scope: User")), "scope switch cannot retain system-scope action context");
        require(!page.isBusy(), "fixture navigation and filtering never initiate service mutations");
        std::cout << checks << " SysPulse management checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
