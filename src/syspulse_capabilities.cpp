// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/capabilities.hpp"
#include "tux/trusted_command.hpp"
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

namespace syspulse {
QVector<ToolAvailability> auditToolAvailability(AvailabilityProbe probe) {
    QVector<ToolAvailability> rows;
    for (const auto& tool : tux::actions())
        rows.append({tool.id, tool.title, tool.group, probe ? probe(tool) : tux::unavailable(tool), tool.changes});
    return rows;
}
QString desktopSettingsExecutable(const QString& name) {
    static const QStringList allowed = {"xfce4-display-settings", "xfce4-keyboard-settings", "xfce4-mouse-settings",
        "xfce4-power-manager-settings", "xfce4-panel", "xfce4-session-settings", "xfce4-appearance-settings", "xfwm4-settings"};
    return allowed.contains(name) ? tux::trustedSystemExecutable(name) : QString();
}
CapabilitiesPanel::CapabilitiesPanel(QWidget* parent) : CapabilitiesPanel(AvailabilityProbe{}, parent) {}
CapabilitiesPanel::CapabilitiesPanel(AvailabilityProbe probe, QWidget* parent) : QWidget(parent), probe_(std::move(probe)) {
    setObjectName("capabilitiesPanel");
    auto* layout = new QVBoxLayout(this);
    auto* notice = new QLabel("Check dependencies and session requirements for every catalog tool. This does not run the tools or request authorization. A dependency being found does not establish permissions, PolicyKit-agent availability or successful operation.");
    notice->setTextFormat(Qt::PlainText); notice->setWordWrap(true); layout->addWidget(notice);
    auto* row = new QHBoxLayout;
    search_ = new QLineEdit; search_->setObjectName("capabilitySearch"); search_->setAccessibleName("Filter tool availability");
    search_->setPlaceholderText("Filter by tool, group or missing dependency"); search_->setClearButtonEnabled(true); row->addWidget(search_, 1);
    auto* refreshButton = new QPushButton("Refresh availability"); refreshButton->setObjectName("refreshCapabilities"); row->addWidget(refreshButton); layout->addLayout(row);
    summary_ = new QLabel("Not checked in this session. Click Refresh availability."); summary_->setObjectName("capabilitySummary"); summary_->setWordWrap(true); layout->addWidget(summary_);
    table_ = new QTableWidget(0, 4); table_->setObjectName("capabilityTable"); table_->setAccessibleName("Catalog dependency and session audit");
    table_->setHorizontalHeaderLabels({"Tool", "Group", "Availability", "Scope / reason"}); table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection); table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); table_->setColumnWidth(1, 105); table_->setColumnWidth(2, 155);
    table_->setWordWrap(true); layout->addWidget(table_, 1);
    open_ = new QPushButton("Inspect selected tool"); open_->setObjectName("inspectCapabilityTool"); open_->setEnabled(false); layout->addWidget(open_);
    connect(refreshButton, &QPushButton::clicked, this, [this] { refresh(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this] { const int row = table_->currentRow(); open_->setEnabled(row >= 0 && !table_->isRowHidden(row)); });
    connect(open_, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row >= 0 && row < snapshot_.size() && !table_->isRowHidden(row) && onOpenTool) onOpenTool(snapshot_[row].id);
    });
}
void CapabilitiesPanel::refresh() {
    QString selected;
    if (table_->currentRow() >= 0 && table_->currentRow() < snapshot_.size()) selected = snapshot_[table_->currentRow()].id;
    snapshot_ = auditToolAvailability(probe_); table_->setRowCount(snapshot_.size());
    for (int row = 0; row < snapshot_.size(); ++row) {
        const auto& item = snapshot_[row];
        table_->setItem(row, 0, new QTableWidgetItem(item.title)); table_->setItem(row, 1, new QTableWidgetItem(item.group));
        table_->setItem(row, 2, new QTableWidgetItem(item.problem.isEmpty() ? "Dependencies found" : "Unavailable"));
        table_->setItem(row, 3, new QTableWidgetItem(!item.problem.isEmpty() ? item.problem : item.experimental ? "Experimental change · In-app review required" : "Read-only · Permissions and session still apply"));
        if (item.id == selected) table_->setCurrentCell(row, 0);
    }
    checkedAt_ = QDateTime::currentDateTime().toString("HH:mm:ss");
    table_->resizeRowsToContents(); filter();
}
void CapabilitiesPanel::filter() {
    const auto words = search_->text().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    int visible = 0, available = 0;
    for (int row = 0; row < snapshot_.size(); ++row) {
        const auto& item = snapshot_[row];
        if (item.problem.isEmpty()) ++available;
        const auto text = item.title + ' ' + item.group + ' ' + item.problem + (item.problem.isEmpty() ? " dependencies found" : " unavailable");
        bool match = true; for (const auto& word : words) if (!text.contains(word, Qt::CaseInsensitive)) match = false;
        table_->setRowHidden(row, !match); if (match) ++visible;
    }
    if (!checkedAt_.isEmpty()) summary_->setText(QString("%1 of %2 tools shown · %3 dependencies found · %4 unavailable · Checked %5").arg(visible).arg(snapshot_.size()).arg(available).arg(snapshot_.size() - available).arg(checkedAt_));
    const int row = table_->currentRow(); open_->setEnabled(row >= 0 && !table_->isRowHidden(row));
}
}
