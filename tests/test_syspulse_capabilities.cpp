// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/capabilities.hpp"
#include "tux/trusted_command.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
namespace {
int checks = 0;
void require(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        int probes = 0; QString opened;
        syspulse::CapabilitiesPanel panel([&](const tux::Action& tool) {
            ++probes; return tool.id == "storage" ? QString("Missing dependency: fixture-df") : QString();
        });
        panel.onOpenTool = [&](const QString& id) { opened = id; };
        require(probes == 0, "constructing the audit does not run probes or system actions");
        auto* table = panel.findChild<QTableWidget*>("capabilityTable");
        auto* refresh = panel.findChild<QPushButton*>("refreshCapabilities");
        auto* inspect = panel.findChild<QPushButton*>("inspectCapabilityTool");
        auto* search = panel.findChild<QLineEdit*>("capabilitySearch");
        auto* summary = panel.findChild<QLabel*>("capabilitySummary");
        require(table && refresh && inspect && search && summary && table->rowCount() == 0, "initial state is unscanned, not a fabricated readiness result");
        refresh->click();
        require(probes == tux::actions().size() && table->rowCount() == tux::actions().size(), "audit preserves every catalog tool including Experimental");
        require(table->rowCount() == 35 && summary->text().contains("1 unavailable"), "35 actual catalog tools and missing-dependency count");
        require(table->item(0, 2)->text() == "Unavailable" && table->item(0, 3)->text().contains("fixture-df"), "concrete dependency reason is displayed");
        table->setCurrentCell(0, 0); inspect->click();
        require(opened == "storage" && probes == 35, "inspection delegates only the tool ID and does not execute or rescan");
        search->setText("fixture-df"); int visible = 0;
        for (int row = 0; row < table->rowCount(); ++row) if (!table->isRowHidden(row)) ++visible;
        require(visible == 1 && summary->text().startsWith("1 of 35"), "search includes missing dependency explanations");
        search->setText("no-such-tool"); require(!inspect->isEnabled(), "hidden selection cannot activate a different tool");
        search->clear(); table->setCurrentCell(3, 0); refresh->click();
        require(table->currentRow() == 3 && probes == 70, "explicit refresh preserves tool identity and uses one probe per tool");
        require(!syspulse::desktopSettingsExecutable("sh").size(), "settings launcher cannot become an arbitrary command runner");
        require(syspulse::desktopSettingsExecutable("xfce4-panel --preferences").isEmpty(), "settings name cannot contain arguments");
        QTemporaryDir directory; require(directory.isValid(), "private PATH fixture");
        QFile shadow(directory.filePath("xfce4-display-settings")); require(shadow.open(QIODevice::WriteOnly), "shadow executable fixture");
        shadow.write("#!/bin/sh\nexit 88\n"); shadow.close(); shadow.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const auto previous = qgetenv("PATH"); qputenv("PATH", directory.path().toUtf8());
        const auto actual = syspulse::desktopSettingsExecutable("xfce4-display-settings"); qputenv("PATH", previous);
        require(actual.isEmpty() || !actual.startsWith(directory.path()), "untrusted PATH cannot replace a native settings launcher");
        require(actual == tux::trustedSystemExecutable("xfce4-display-settings"), "native settings use the existing protected-system resolver");
        const auto real = syspulse::auditToolAvailability();
        require(real.size() == 35, "real guest dependency audit is bounded to the maintained catalog");
        std::cout << checks << " availability/search/scope/selection/trusted-launch checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
