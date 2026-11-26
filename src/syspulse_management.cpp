// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/management.hpp"
#include "tux/async_command.hpp"
#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFormLayout>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace syspulse::management {
bool safeServiceUnit(const QString& unit)
{
    // Permit systemd's literal \\xHH escaping, while excluding argument prefixes,
    // separators, whitespace and arbitrary command syntax. Arguments also use --.
    static const QRegularExpression safe(QStringLiteral("\\A[A-Za-z0-9_@:.\\\\-]+\\.service\\z"));
    return unit.size() <= 255 && !unit.startsWith(QLatin1Char('-'))
        && safe.match(unit).hasMatch();
}

QVector<ServiceEntry> parseServiceLists(const QByteArray& unitFiles,
                                      const QByteArray& runtimeUnits, bool userScope)
{
    QMap<QString, ServiceEntry> entries;
    const QStringList startupStates{QStringLiteral("enabled"), QStringLiteral("enabled-runtime"),
        QStringLiteral("disabled"), QStringLiteral("static"), QStringLiteral("indirect"),
        QStringLiteral("alias"), QStringLiteral("linked"), QStringLiteral("linked-runtime"),
        QStringLiteral("masked"), QStringLiteral("masked-runtime"), QStringLiteral("generated"),
        QStringLiteral("transient"), QStringLiteral("bad")};
    for (const auto& line : QString::fromUtf8(unitFiles).split(QLatin1Char('\n'))) {
        const auto fields = line.simplified().split(QLatin1Char(' '));
        if (fields.size() < 2 || !safeServiceUnit(fields[0]) || !startupStates.contains(fields[1])) continue;
        auto& entry = entries[fields[0]];
        entry.unit = fields[0]; entry.user = userScope; entry.startup = fields[1];
    }
    for (auto line : QString::fromUtf8(runtimeUnits).split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.startsWith(QChar(0x25cf))) line = line.mid(1).trimmed();
        const auto fields = line.simplified().split(QLatin1Char(' '));
        if (fields.size() < 4 || !safeServiceUnit(fields[0])) continue;
        if (!QStringList{QStringLiteral("active"), QStringLiteral("inactive"), QStringLiteral("failed"),
                QStringLiteral("activating"), QStringLiteral("deactivating"), QStringLiteral("reloading"),
                QStringLiteral("maintenance"), QStringLiteral("refreshing")}.contains(fields[2])) continue;
        auto& entry = entries[fields[0]];
        entry.unit = fields[0]; entry.user = userScope;
        entry.loadState = fields[1]; entry.runtime = fields[2]; entry.substate = fields[3];
        entry.description = fields.mid(4).join(QLatin1Char(' '));
    }
    return entries.values().toVector();
}

QStringList availableServiceActions(const ServiceEntry& entry)
{
    if (!safeServiceUnit(entry.unit)) return {};
    QStringList actions;
    const bool loadable = entry.loadState == QStringLiteral("loaded")
        && entry.startup != QStringLiteral("masked")
        && entry.startup != QStringLiteral("masked-runtime");
    if (loadable) {
        if (entry.runtime == QStringLiteral("inactive") || entry.runtime == QStringLiteral("failed")) actions << QStringLiteral("start");
        if (entry.runtime == QStringLiteral("active") || entry.runtime == QStringLiteral("activating")
            || entry.runtime == QStringLiteral("reloading")) actions << QStringLiteral("stop");
        if (entry.runtime == QStringLiteral("active")) actions << QStringLiteral("restart");
        if (entry.startup == QStringLiteral("disabled")) actions << QStringLiteral("enable");
    }
    if (entry.startup == QStringLiteral("enabled") || entry.startup == QStringLiteral("enabled-runtime")) actions << QStringLiteral("disable");
    return actions;
}
}

namespace syspulse {
namespace {
QLabel* textLabel(const QString& text = {}, const QString& objectName = {}, QWidget* parent = nullptr)
{
    auto* label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    if (!objectName.isEmpty()) label->setObjectName(objectName);
    return label;
}
QFrame* panel(QWidget* parent = nullptr)
{
    auto* frame = new QFrame(parent);
    frame->setProperty("role", "panel");
    frame->setFrameShape(QFrame::StyledPanel);
    return frame;
}
void configureTable(QTableWidget* table, const QStringList& headings)
{
    table->setColumnCount(headings.size());
    table->setHorizontalHeaderLabels(headings);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(false);
    table->setShowGrid(false);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(34);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(80);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSortingEnabled(true);
}
QString resultProblem(const AsyncCommandResult& result)
{
    if (result.cancelled) return QStringLiteral("Cancelled");
    if (result.timedOut) return QStringLiteral("Timed out");
    if (result.truncated) return QStringLiteral("Output limit exceeded; results are incomplete");
    if (!result.error.isEmpty()) return result.error.left(250);
    const auto detail = QString::fromUtf8(result.output).trimmed();
    return QStringLiteral("Exit code %1%2").arg(result.exitCode)
        .arg(detail.isEmpty() ? QString() : QStringLiteral(" · ") + detail.left(300));
}
QString trustedSystemTool(const QString& name)
{
    // Never elevate a PATH-resolved program from a writable user directory.
    // Other layouts can be added through an explicit distribution adapter.
    for (const auto& directory : {QStringLiteral("/usr/bin/"), QStringLiteral("/bin/"),
                                 QStringLiteral("/usr/sbin/"), QStringLiteral("/sbin/")}) {
        const QFileInfo candidate(directory + name);
        const auto canonical = candidate.canonicalFilePath();
        const QFileInfo actual(canonical);
        if (canonical.isEmpty() || !actual.isFile() || !actual.isExecutable() || actual.ownerId() != 0) continue;
        if (actual.permissions() & (QFileDevice::WriteGroup | QFileDevice::WriteOther)) continue;
        if (canonical.startsWith(QStringLiteral("/usr/bin/")) || canonical.startsWith(QStringLiteral("/bin/"))
            || canonical.startsWith(QStringLiteral("/usr/sbin/")) || canonical.startsWith(QStringLiteral("/sbin/"))) return canonical;
    }
    return {};
}
QMap<QString, QString> properties(const QByteArray& data)
{
    QMap<QString, QString> values;
    for (const auto& line : QString::fromUtf8(data).split(QLatin1Char('\n'))) {
        const auto equal = line.indexOf(QLatin1Char('='));
        if (equal > 0) values.insert(line.left(equal), line.mid(equal + 1).trimmed());
    }
    return values;
}
QString startupState(const tux::autostart::Entry& entry)
{
    if (!entry.valid) return QStringLiteral("Invalid entry");
    if (entry.hidden) return QStringLiteral("Disabled");
    return entry.eligible ? QStringLiteral("Enabled") : QStringLiteral("Not for this session");
}
QString startupSource(const tux::autostart::Entry& entry)
{
    if (!entry.userOverride) return QStringLiteral("System default");
    return entry.hasSystemDefault ? QStringLiteral("User override") : QStringLiteral("User entry");
}
}

class ManagementPage::Impl final : public QObject {
public:
    explicit Impl(ManagementPage* page, Options options)
        : QObject(page), owner(page), options(std::move(options))
    {
        if (!this->options.fixtureMode && this->options.startupPaths.configHome.isEmpty())
            this->options.startupPaths = tux::autostart::Paths::environment();
        owner->setObjectName(QStringLiteral("managementPage"));
        owner->setStyleSheet(QStringLiteral(
            "QFrame[role=panel]{background:#ffffff;border:1px solid #d5dae3;border-radius:9px;}"
            "QLabel[role=heading]{font-size:17px;font-weight:600;color:#202124;}"
            "QLabel[role=muted]{color:#667085;}"
            "QTableWidget{background:#ffffff;border:1px solid #d5dae3;border-radius:7px;selection-background-color:#fbe9ed;selection-color:#861027;}"
            "QHeaderView::section{background:#f7f8fa;color:#202124;border:0;border-bottom:1px solid #d5dae3;padding:8px;font-weight:600;}"
            "QTableWidget::item{padding:5px;}"));
        auto* layout = new QVBoxLayout(owner);
        layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(10);
        if (this->options.fixtureMode) {
            auto* notice = textLabel(QStringLiteral("Design preview · Sample data · Changes disabled"), QStringLiteral("managementFixtureNotice"));
            notice->setProperty("role", "muted"); layout->addWidget(notice);
        }
        tabs = new QTabWidget(owner); tabs->setObjectName(QStringLiteral("managementTabs"));
        layout->addWidget(tabs, 1);
        buildStartup(); buildServices(); buildActivity(layout);
        connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
            if (index == 1 && !serviceDataReady && !mutationBusy) refreshServices();
            else if (index == 0) cancelReads();
        });
        refreshStartup();
    }
    ~Impl() override
    {
        cancelReads();
        if (watchedWindow) watchedWindow->removeEventFilter(this);
    }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == watchedWindow && event->type() == QEvent::Close && mutationBusy) {
            QMessageBox::information(owner, QStringLiteral("Service action in progress"),
                QStringLiteral("Wait for the service manager to finish before closing SysPulse. Its final state will be refreshed when the action completes."));
            event->ignore(); return true;
        }
        return QObject::eventFilter(watched, event);
    }
    void attachWindow()
    {
        if (watchedWindow != owner->window()) {
            if (watchedWindow) watchedWindow->removeEventFilter(this);
            watchedWindow = owner->window();
            if (watchedWindow) watchedWindow->installEventFilter(this);
        }
        if (tabs->currentIndex() == 1 && !serviceDataReady && !serviceLoading && !mutationBusy) refreshServices();
    }
    void log(const QString& action, const QString& outcome)
    {
        activity->insertRow(0);
        const QStringList values{QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), action, outcome};
        for (int column = 0; column < values.size(); ++column) activity->setItem(0, column, new QTableWidgetItem(values[column]));
        if (activity->rowCount() > 100) activity->setRowCount(100);
    }
    void buildActivity(QVBoxLayout* layout)
    {
        auto* frame = panel(); auto* box = new QVBoxLayout(frame); box->setContentsMargins(12, 8, 12, 8);
        auto* row = new QHBoxLayout;
        auto* heading = textLabel(QStringLiteral("Activity")); heading->setProperty("role", "heading"); row->addWidget(heading);
        row->addStretch();
        auto* clear = new QPushButton(QStringLiteral("Clear view")); clear->setObjectName(QStringLiteral("managementClearActivity"));
        clear->setToolTip(QStringLiteral("Clear the displayed activity; this does not undo changes."));
        row->addWidget(clear); box->addLayout(row);
        activity = new QTableWidget(0, 3); activity->setObjectName(QStringLiteral("managementActivity"));
        configureTable(activity, {QStringLiteral("Time"), QStringLiteral("Event"), QStringLiteral("Outcome")});
        activity->setSortingEnabled(false);
        activity->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        activity->setMinimumHeight(86); activity->setMaximumHeight(118);
        box->addWidget(activity); layout->addWidget(frame);
        connect(clear, &QPushButton::clicked, this, [this] { activity->setRowCount(0); });
    }
    void buildStartup()
    {
        auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 10, 0, 0);
        auto* toolbar = new QHBoxLayout;
        sourceFilter = new QComboBox; sourceFilter->setObjectName(QStringLiteral("startupSourceFilter"));
        sourceFilter->addItems({QStringLiteral("All sources"), QStringLiteral("System defaults"), QStringLiteral("User entries")});
        sourceFilter->setAccessibleName(QStringLiteral("Filter startup source")); toolbar->addWidget(sourceFilter);
        startupFilter = new QComboBox; startupFilter->setObjectName(QStringLiteral("startupStateFilter"));
        startupFilter->addItems({QStringLiteral("All states"), QStringLiteral("Enabled"), QStringLiteral("Disabled"), QStringLiteral("Unavailable")});
        startupFilter->setAccessibleName(QStringLiteral("Filter startup state")); toolbar->addWidget(startupFilter);
        auto* refresh = new QPushButton(QStringLiteral("Refresh")); refresh->setObjectName(QStringLiteral("startupRefresh")); toolbar->addWidget(refresh);
        startupSearch = new QLineEdit; startupSearch->setObjectName(QStringLiteral("startupSearch")); startupSearch->setPlaceholderText(QStringLiteral("Filter startup apps"));
        startupSearch->setAccessibleName(QStringLiteral("Search startup applications")); startupSearch->setClearButtonEnabled(true); toolbar->addWidget(startupSearch, 1); layout->addLayout(toolbar);
        startupSummary = textLabel({}, QStringLiteral("startupSummary")); startupSummary->setProperty("role", "muted"); layout->addWidget(startupSummary);
        auto* splitter = new QSplitter(Qt::Horizontal); splitter->setObjectName(QStringLiteral("startupSplitter"));
        startupTable = new QTableWidget(0, 4); startupTable->setObjectName(QStringLiteral("startupTable"));
        configureTable(startupTable, {QStringLiteral("Application"), QStringLiteral("Status"), QStringLiteral("Source"), QStringLiteral("Entry")});
        startupTable->setAccessibleName(QStringLiteral("Startup applications")); splitter->addWidget(startupTable);
        auto* inspector = panel(); inspector->setMinimumWidth(245); auto* details = new QVBoxLayout(inspector); details->setContentsMargins(16, 14, 16, 14);
        auto* title = textLabel(QStringLiteral("Application details")); title->setProperty("role", "heading"); details->addWidget(title);
        startupName = textLabel(QStringLiteral("Select an application"), QStringLiteral("startupSelectedName")); startupName->setProperty("role", "heading"); details->addWidget(startupName);
        startupDetails = textLabel({}, QStringLiteral("startupDetails")); startupDetails->setTextInteractionFlags(Qt::TextSelectableByMouse); details->addWidget(startupDetails);
        startupReason = textLabel({}, QStringLiteral("startupSelectedReason")); startupReason->setProperty("role", "muted"); details->addWidget(startupReason);
        auto* changeRow = new QHBoxLayout;
        startupEnable = new QPushButton(QStringLiteral("Enable at login")); startupEnable->setObjectName(QStringLiteral("startupEnable"));
        startupDisable = new QPushButton(QStringLiteral("Disable at login")); startupDisable->setObjectName(QStringLiteral("startupDisable"));
        changeRow->addWidget(startupEnable); changeRow->addWidget(startupDisable); details->addLayout(changeRow);
        startupUndo = new QPushButton(QStringLiteral("Undo last change")); startupUndo->setObjectName(QStringLiteral("startupUndo")); details->addWidget(startupUndo);
        auto* explanation = textLabel(QStringLiteral("System defaults are preserved through a per-user override. Changing a login entry does not start or stop its service."));
        explanation->setProperty("role", "muted"); details->addWidget(explanation); details->addStretch();
        auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(inspector); splitter->addWidget(scroll);
        splitter->setStretchFactor(0, 3); splitter->setStretchFactor(1, 2); splitter->setSizes({650, 340}); layout->addWidget(splitter, 1);
        tabs->addTab(page, QStringLiteral("Startup apps"));
        connect(refresh, &QPushButton::clicked, this, [this] { refreshStartup(); });
        connect(sourceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { filterStartup(); });
        connect(startupFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { filterStartup(); });
        connect(startupSearch, &QLineEdit::textChanged, this, [this] { filterStartup(); });
        connect(startupTable, &QTableWidget::itemSelectionChanged, this, [this] { inspectStartup(); });
        connect(startupEnable, &QPushButton::clicked, this, [this] { changeStartup(true); });
        connect(startupDisable, &QPushButton::clicked, this, [this] { changeStartup(false); });
        connect(startupUndo, &QPushButton::clicked, this, [this] { undoStartup(); });
        inspectStartup();
    }
    const tux::autostart::Entry* selectedStartup() const
    {
        const auto row = startupTable->currentRow();
        if (row < 0 || startupTable->isRowHidden(row) || startupTable->selectedItems().isEmpty()) return nullptr;
        const auto id = startupTable->item(row, 0)->data(Qt::UserRole).toString();
        for (const auto& entry : startupEntries) if (entry.id == id) return &entry;
        return nullptr;
    }
    void refreshStartup()
    {
        const auto* selected = selectedStartup(); const auto id = selected ? selected->id : QString();
        startupEntries = options.fixtureMode && options.startupPaths.configHome.isEmpty()
            ? QVector<tux::autostart::Entry>{} : tux::autostart::discover(options.startupPaths);
        const QSignalBlocker block(startupTable);
        startupTable->setSortingEnabled(false); startupTable->setRowCount(startupEntries.size());
        for (int row = 0; row < startupEntries.size(); ++row) {
            auto& entry = startupEntries[row]; entry.contents.clear();
            const QStringList fields{entry.name, startupState(entry), startupSource(entry), entry.id};
            for (int column = 0; column < fields.size(); ++column) {
                auto* item = new QTableWidgetItem(fields[column]); item->setData(Qt::UserRole, entry.id);
                startupTable->setItem(row, column, item);
            }
            startupTable->item(row, 0)->setToolTip(entry.reason);
        }
        startupTable->setSortingEnabled(true);
        for (int row = 0; row < startupTable->rowCount(); ++row)
            if (startupTable->item(row, 0)->data(Qt::UserRole).toString() == id) startupTable->selectRow(row);
        filterStartup(); log(QStringLiteral("Startup entries loaded"), QStringLiteral("Read-only · %1 effective entries").arg(startupEntries.size()));
    }
    void filterStartup()
    {
        int shown = 0;
        for (int row = 0; row < startupTable->rowCount(); ++row) {
            const auto id = startupTable->item(row, 0)->data(Qt::UserRole).toString();
            const auto found = std::find_if(startupEntries.cbegin(), startupEntries.cend(), [&](const auto& e) { return e.id == id; });
            if (found == startupEntries.cend()) continue;
            const auto& entry = *found;
            bool matches = (entry.name + QLatin1Char(' ') + entry.id + QLatin1Char(' ') + entry.command + QLatin1Char(' ') + entry.reason)
                .contains(startupSearch->text().trimmed(), Qt::CaseInsensitive);
            const int source = sourceFilter->currentIndex();
            matches = matches && (source == 0 || (source == 1 && !entry.userOverride) || (source == 2 && entry.userOverride));
            const int state = startupFilter->currentIndex();
            matches = matches && (state == 0 || (state == 1 && entry.valid && entry.eligible && !entry.hidden)
                || (state == 2 && entry.valid && entry.hidden) || (state == 3 && (!entry.valid || (!entry.hidden && !entry.eligible))));
            startupTable->setRowHidden(row, !matches); if (matches) ++shown;
        }
        if (startupTable->currentRow() >= 0 && startupTable->isRowHidden(startupTable->currentRow())) startupTable->clearSelection();
        startupSummary->setText(QStringLiteral("%1 of %2 entries shown · Eligibility does not guarantee a successful launch").arg(shown).arg(startupEntries.size()));
        inspectStartup();
    }
    void inspectStartup()
    {
        const auto* entry = selectedStartup();
        const bool allow = entry && entry->valid && !options.fixtureMode && !mutationBusy;
        startupEnable->setEnabled(allow && entry->hidden);
        startupDisable->setEnabled(allow && !entry->hidden);
        startupUndo->setEnabled(entry && !options.fixtureMode && !mutationBusy && tux::autostart::hasUndo(options.startupPaths, entry->id));
        if (!entry) {
            startupName->setText(QStringLiteral("Select an application")); startupDetails->clear(); startupReason->clear(); return;
        }
        startupName->setText(entry->name);
        startupDetails->setText(QStringLiteral("Status: %1\nSource: %2\nEntry: %3\n\nCommand (display only):\n%4")
            .arg(startupState(*entry), startupSource(*entry), entry->id, entry->command));
        startupReason->setText(entry->reason);
    }
    bool confirm(const QString& title, const QString& text)
    {
        QMessageBox dialog(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::Cancel, owner);
        dialog.setTextFormat(Qt::PlainText); dialog.setDefaultButton(QMessageBox::Cancel);
        return dialog.exec() == QMessageBox::Yes;
    }
    bool admission()
    {
        if (!mutationBusy && (!owner->canMutate || owner->canMutate())) return true;
        log(QStringLiteral("Change deferred"), QStringLiteral("Another system task is active. Wait for it to finish before changing startup or services."));
        return false;
    }
    void changeStartup(bool enabled)
    {
        const auto* selected = selectedStartup();
        if (!selected || !selected->valid || options.fixtureMode || mutationBusy) return;
        if (!admission()) return;
        const auto entry = *selected;
        if (!confirm(enabled ? QStringLiteral("Enable startup application") : QStringLiteral("Disable startup application"),
                QStringLiteral("%1 %2 at login for your account?\n\nThis does not start or stop it now. System defaults remain unchanged. The previous user state will be saved for Undo last change.")
                    .arg(enabled ? QStringLiteral("Enable") : QStringLiteral("Disable"), entry.name))) return;
        if (!admission()) return;
        const auto result = tux::autostart::setEnabled(options.startupPaths, entry.id, enabled);
        refreshStartup(); log(enabled ? QStringLiteral("Enable startup entry") : QStringLiteral("Disable startup entry"), result.message);
        if (!result.ok) QMessageBox::warning(owner, QStringLiteral("Startup change not applied"), result.message);
    }
    void undoStartup()
    {
        const auto* selected = selectedStartup();
        if (!selected || options.fixtureMode || mutationBusy) return;
        if (!admission()) return;
        const auto entry = *selected;
        if (!confirm(QStringLiteral("Undo startup change"), QStringLiteral("Restore the saved previous user state for %1?\n\nEntries edited outside SysPulse will not be overwritten.").arg(entry.name))) return;
        if (!admission()) return;
        const auto result = tux::autostart::undo(options.startupPaths, entry.id);
        refreshStartup(); log(QStringLiteral("Undo startup change"), result.message);
        if (!result.ok) QMessageBox::warning(owner, QStringLiteral("Startup undo not applied"), result.message);
    }
    void buildServices()
    {
        auto* page = new QWidget; auto* layout = new QVBoxLayout(page); layout->setContentsMargins(0, 10, 0, 0);
        auto* toolbar = new QHBoxLayout;
        scope = new QComboBox; scope->setObjectName(QStringLiteral("serviceScopeFilter"));
        scope->addItems({QStringLiteral("System scope"), QStringLiteral("User scope")}); scope->setAccessibleName(QStringLiteral("Service scope")); toolbar->addWidget(scope);
        serviceFilter = new QComboBox; serviceFilter->setObjectName(QStringLiteral("serviceStateFilter"));
        serviceFilter->addItems({QStringLiteral("All states"), QStringLiteral("Running"), QStringLiteral("Inactive"), QStringLiteral("Failed"), QStringLiteral("Enabled at boot"), QStringLiteral("Disabled at boot")});
        serviceFilter->setAccessibleName(QStringLiteral("Filter service state")); toolbar->addWidget(serviceFilter);
        serviceRefresh = new QPushButton(QStringLiteral("Refresh")); serviceRefresh->setObjectName(QStringLiteral("serviceRefresh")); toolbar->addWidget(serviceRefresh);
        serviceSearch = new QLineEdit; serviceSearch->setObjectName(QStringLiteral("serviceSearch")); serviceSearch->setPlaceholderText(QStringLiteral("Filter services"));
        serviceSearch->setAccessibleName(QStringLiteral("Search services")); serviceSearch->setClearButtonEnabled(true); toolbar->addWidget(serviceSearch, 1); layout->addLayout(toolbar);
        serviceSummary = textLabel(QStringLiteral("Select Services to load the service manager."), QStringLiteral("serviceSummary")); serviceSummary->setProperty("role", "muted"); layout->addWidget(serviceSummary);
        auto* splitter = new QSplitter(Qt::Horizontal); splitter->setObjectName(QStringLiteral("serviceSplitter"));
        serviceTable = new QTableWidget(0, 4); serviceTable->setObjectName(QStringLiteral("serviceTable"));
        configureTable(serviceTable, {QStringLiteral("Unit"), QStringLiteral("Runtime"), QStringLiteral("Startup"), QStringLiteral("Description")});
        serviceTable->setAccessibleName(QStringLiteral("Systemd services")); splitter->addWidget(serviceTable);
        auto* inspector = panel(); inspector->setMinimumWidth(245); auto* details = new QVBoxLayout(inspector); details->setContentsMargins(16, 14, 16, 14);
        auto* heading = textLabel(QStringLiteral("Unit details")); heading->setProperty("role", "heading"); details->addWidget(heading);
        serviceName = textLabel(QStringLiteral("Select a service"), QStringLiteral("serviceSelectedName")); serviceName->setProperty("role", "heading"); details->addWidget(serviceName);
        serviceDetails = textLabel({}, QStringLiteral("serviceDetails")); serviceDetails->setTextInteractionFlags(Qt::TextSelectableByMouse); details->addWidget(serviceDetails);
        auto* firstRow = new QHBoxLayout; auto* secondRow = new QHBoxLayout;
        const QStringList verbs{QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("restart"), QStringLiteral("enable"), QStringLiteral("disable")};
        for (const auto& verb : verbs) {
            auto title = verb; title[0] = title[0].toUpper();
            auto* button = new QPushButton(title + QStringLiteral("…")); button->setObjectName(QStringLiteral("serviceAction_") + verb);
            serviceActions.insert(verb, button); (serviceActions.size() <= 3 ? firstRow : secondRow)->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, verb] { changeService(verb); });
        }
        details->addLayout(firstRow); details->addLayout(secondRow);
        auto* note = textLabel(QStringLiteral("Runtime and startup settings are separate. System actions may require administrator approval.")); note->setProperty("role", "muted"); details->addWidget(note);
        serviceDependencies = textLabel({}, QStringLiteral("serviceDependencies")); details->addWidget(serviceDependencies);
        journal = new QPushButton(QStringLiteral("Open service journal…")); journal->setObjectName(QStringLiteral("serviceJournal")); details->addWidget(journal);
        details->addStretch();
        auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(inspector); splitter->addWidget(scroll);
        splitter->setStretchFactor(0, 3); splitter->setStretchFactor(1, 2); splitter->setSizes({650, 340}); layout->addWidget(splitter, 1);
        tabs->addTab(page, QStringLiteral("Services"));
        connect(scope, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { if (!mutationBusy) refreshServices(); });
        connect(serviceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { filterServices(); });
        connect(serviceSearch, &QLineEdit::textChanged, this, [this] { filterServices(); });
        connect(serviceRefresh, &QPushButton::clicked, this, [this] { if (!mutationBusy) refreshServices(); });
        connect(serviceTable, &QTableWidget::itemSelectionChanged, this, [this] { inspectService(); });
        connect(journal, &QPushButton::clicked, this, [this] { showJournal(); }); updateServiceActions();
    }
    management::ServiceEntry* selectedService()
    {
        const int row = serviceTable->currentRow();
        if (row < 0 || serviceTable->isRowHidden(row) || serviceTable->selectedItems().isEmpty()) return nullptr;
        const auto unit = serviceTable->item(row, 0)->data(Qt::UserRole).toString();
        for (auto& entry : serviceEntries) if (entry.unit == unit) return &entry;
        return nullptr;
    }
    void cancelReads()
    {
        ++generation; ++detailGeneration; serviceLoading = false;
        const auto queries = readQueries; readQueries.clear();
        for (const auto& query : queries) if (query && query->active()) query->cancel();
        if (detailQuery && detailQuery->active()) detailQuery->cancel();
        detailQuery = nullptr;
        if (journalQuery && journalQuery->active()) journalQuery->cancel();
        journalQuery = nullptr;
        detailsReady = false; serviceDataReady = false;
        updateServiceActions();
    }
    void refreshServices()
    {
        if (mutationBusy) return;
        const auto* previous = selectedService(); const auto id = previous ? previous->unit : QString();
        cancelReads();
        serviceEntries.clear(); serviceTable->setRowCount(0); serviceName->setText(QStringLiteral("Select a service")); serviceDetails->clear(); serviceDependencies->clear();
        listError.clear(); unitFiles.clear(); runtimeUnits.clear(); listOk = runtimeOk = false;
        const bool user = scope->currentIndex() == 1;
        const int run = generation;
        if (options.fixtureMode) {
            unitFiles = options.serviceUnitFilesFixture; runtimeUnits = options.serviceRuntimeFixture;
            listOk = runtimeOk = true; finishServiceRefresh(id); return;
        }
        const auto program = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
        if (program.isEmpty()) { serviceSummary->setText(QStringLiteral("systemctl is unavailable on this system. No action was started.")); return; }
        serviceLoading = true; servicePending = 2; serviceSummary->setText(QStringLiteral("Loading services… Other workspaces remain available."));
        updateServiceActions();
        QStringList prefix{user ? QStringLiteral("--user") : QStringLiteral("--system"), QStringLiteral("--no-pager"), QStringLiteral("--no-legend"), QStringLiteral("--plain"), QStringLiteral("--full"), QStringLiteral("--no-ask-password")};
        auto* filesQuery = AsyncCommand::run(owner, program, prefix + QStringList{QStringLiteral("list-unit-files"), QStringLiteral("--type=service")}, 5000, 512 * 1024,
            [this, run, id](AsyncCommandResult result) {
                if (run != generation) return;
                listOk = result.ok(); if (listOk) unitFiles = result.output;
                else listError << QStringLiteral("Startup-state query: ") + resultProblem(result);
                if (--servicePending == 0) finishServiceRefresh(id);
            }); readQueries.append(filesQuery);
        auto* runtimeQuery = AsyncCommand::run(owner, program, prefix + QStringList{QStringLiteral("list-units"), QStringLiteral("--type=service"), QStringLiteral("--all")}, 5000, 512 * 1024,
            [this, run, id](AsyncCommandResult result) {
                if (run != generation) return;
                runtimeOk = result.ok(); if (runtimeOk) runtimeUnits = result.output;
                else listError << QStringLiteral("Runtime query: ") + resultProblem(result);
                if (--servicePending == 0) finishServiceRefresh(id);
            }); readQueries.append(runtimeQuery);
    }
    void finishServiceRefresh(const QString& selected)
    {
        serviceLoading = false; readQueries.clear(); serviceDataReady = listOk && runtimeOk;
        serviceEntries = management::parseServiceLists(unitFiles, runtimeUnits, scope->currentIndex() == 1);
        const QSignalBlocker block(serviceTable);
        serviceTable->setSortingEnabled(false); serviceTable->setRowCount(serviceEntries.size());
        for (int row = 0; row < serviceEntries.size(); ++row) {
            auto& entry = serviceEntries[row];
            if (!runtimeOk) entry.runtime = QStringLiteral("Unavailable");
            if (!listOk) entry.startup = QStringLiteral("Unavailable");
            const QStringList values{entry.unit, entry.runtime, entry.startup, entry.description};
            for (int column = 0; column < values.size(); ++column) {
                auto* item = new QTableWidgetItem(values[column]); item->setData(Qt::UserRole, entry.unit);
                serviceTable->setItem(row, column, item);
            }
        }
        serviceTable->setSortingEnabled(true);
        for (int row = 0; row < serviceTable->rowCount(); ++row)
            if (serviceTable->item(row, 0)->data(Qt::UserRole).toString() == selected) serviceTable->selectRow(row);
        filterServices();
        if (!listError.isEmpty()) {
            serviceSummary->setText(QStringLiteral("Service data incomplete. Changes are disabled. %1").arg(listError.join(QStringLiteral(" · "))));
            log(QStringLiteral("Service query incomplete"), QStringLiteral("Read-only · No changes applied"));
        } else log(QStringLiteral("Service list refreshed"), QStringLiteral("Read-only · %1 units").arg(serviceEntries.size()));
        inspectService();
    }
    void filterServices()
    {
        int shown = 0;
        for (int row = 0; row < serviceTable->rowCount(); ++row) {
            QStringList values; for (int column = 0; column < serviceTable->columnCount(); ++column) values << serviceTable->item(row, column)->text();
            bool matches = values.join(QLatin1Char(' ')).contains(serviceSearch->text().trimmed(), Qt::CaseInsensitive);
            const int filter = serviceFilter->currentIndex(); const auto state = values.value(1), startup = values.value(2);
            matches = matches && (filter == 0 || (filter == 1 && state == QStringLiteral("active"))
                || (filter == 2 && (state == QStringLiteral("inactive") || state == QStringLiteral("Not loaded")))
                || (filter == 3 && state == QStringLiteral("failed"))
                || (filter == 4 && (startup == QStringLiteral("enabled") || startup == QStringLiteral("enabled-runtime")))
                || (filter == 5 && startup == QStringLiteral("disabled")));
            serviceTable->setRowHidden(row, !matches); if (matches) ++shown;
        }
        if (serviceTable->currentRow() >= 0 && serviceTable->isRowHidden(serviceTable->currentRow())) serviceTable->clearSelection();
        if (listError.isEmpty() && !serviceLoading)
            serviceSummary->setText(QStringLiteral("%1 of %2 services shown · %3 scope · Runtime and startup settings are distinct")
                .arg(shown).arg(serviceEntries.size()).arg(scope->currentIndex() == 1 ? QStringLiteral("User") : QStringLiteral("System")));
        if (!selectedService()) { detailsReady = false; serviceName->setText(QStringLiteral("Select a service")); serviceDetails->clear(); serviceDependencies->clear(); }
        updateServiceActions();
    }
    void inspectService()
    {
        ++detailGeneration; detailsReady = false;
        if (detailQuery && detailQuery->active()) detailQuery->cancel();
        detailQuery = nullptr;
        auto* entry = selectedService();
        if (!entry) { serviceName->setText(QStringLiteral("Select a service")); serviceDetails->clear(); serviceDependencies->clear(); updateServiceActions(); return; }
        serviceName->setText(entry->unit); serviceDependencies->setText(QStringLiteral("Loading unit details…"));
        displayService(*entry); updateServiceActions();
        const int detailRun = detailGeneration, run = generation; const auto unit = entry->unit; const bool user = entry->user;
        if (options.fixtureMode) {
            const auto sample = properties(options.serviceDetailsFixture);
            if (sample.value(QStringLiteral("Id")) == entry->unit) {
                applyDetails(options.serviceDetailsFixture, *entry);
                synchronizeServiceRow(*entry);
            }
            else serviceDependencies->setText(QStringLiteral("Sample unit details · No real service was queried."));
            auto* remaining = selectedService();
            detailsReady = remaining && remaining->unit == unit;
            if (detailsReady) displayService(*remaining);
            updateServiceActions(); return;
        }
        if (!serviceDataReady || !management::safeServiceUnit(unit)) { serviceDependencies->setText(QStringLiteral("A complete service refresh is required before making changes.")); return; }
        const auto program = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
        if (program.isEmpty()) return;
        QStringList args{user ? QStringLiteral("--user") : QStringLiteral("--system"), QStringLiteral("--no-pager"), QStringLiteral("--no-ask-password"),
            QStringLiteral("show"), QStringLiteral("--property=Id,Description,LoadState,ActiveState,SubState,UnitFileState,Requires,Wants,After"), QStringLiteral("--"), unit};
        detailQuery = AsyncCommand::run(owner, program, args, 4000, 128 * 1024,
            [this, detailRun, run, unit, user](AsyncCommandResult result) {
                if (run != generation || detailRun != detailGeneration) return;
                auto* current = selectedService();
                if (!current || current->unit != unit || current->user != user) return;
                if (!result.ok()) { serviceDependencies->setText(QStringLiteral("Unit details unavailable: %1\nChanges are disabled until a successful refresh.").arg(resultProblem(result))); updateServiceActions(); return; }
                const auto fields = properties(result.output);
                if (fields.value(QStringLiteral("Id")) != unit || fields.value(QStringLiteral("LoadState")).isEmpty()) {
                    serviceDependencies->setText(QStringLiteral("The unit identity could not be verified. Refresh services before making changes.")); updateServiceActions(); return;
                }
                applyDetails(result.output, *current);
                synchronizeServiceRow(*current);
                auto* remaining = selectedService();
                detailsReady = remaining && remaining->unit == unit && remaining->user == user;
                if (detailsReady) displayService(*remaining);
                updateServiceActions();
            });
    }
    void synchronizeServiceRow(const management::ServiceEntry& entry)
    {
        {
            const QSignalBlocker block(serviceTable);
            serviceTable->setSortingEnabled(false);
            for (int row = 0; row < serviceTable->rowCount(); ++row) {
                if (serviceTable->item(row, 0)->data(Qt::UserRole).toString() != entry.unit) continue;
                serviceTable->item(row, 1)->setText(entry.runtime);
                serviceTable->item(row, 2)->setText(entry.startup);
                serviceTable->item(row, 3)->setText(entry.description);
                break;
            }
            serviceTable->setSortingEnabled(true);
            for (int row = 0; row < serviceTable->rowCount(); ++row)
                if (serviceTable->item(row, 0)->data(Qt::UserRole).toString() == entry.unit) serviceTable->selectRow(row);
        }
        filterServices();
    }
    void applyDetails(const QByteArray& bytes, management::ServiceEntry& entry)
    {
        const auto fields = properties(bytes);
        if (fields.contains(QStringLiteral("ActiveState"))) entry.runtime = fields.value(QStringLiteral("ActiveState"));
        if (fields.contains(QStringLiteral("SubState"))) entry.substate = fields.value(QStringLiteral("SubState"));
        if (fields.contains(QStringLiteral("UnitFileState"))) entry.startup = fields.value(QStringLiteral("UnitFileState")).isEmpty()
            ? QStringLiteral("Not applicable") : fields.value(QStringLiteral("UnitFileState"));
        if (fields.contains(QStringLiteral("LoadState"))) entry.loadState = fields.value(QStringLiteral("LoadState"));
        if (fields.contains(QStringLiteral("Description"))) entry.description = fields.value(QStringLiteral("Description"));
        QStringList dependencies;
        for (const auto& key : {QStringLiteral("Requires"), QStringLiteral("Wants"), QStringLiteral("After")})
            if (!fields.value(key).isEmpty()) dependencies << key + QStringLiteral(": ") + fields.value(key);
        serviceDependencies->setText(dependencies.isEmpty() ? QStringLiteral("No dependency details returned.") : QStringLiteral("Dependencies\n") + dependencies.join(QLatin1Char('\n')));
    }
    void displayService(const management::ServiceEntry& entry)
    {
        serviceDetails->setText(QStringLiteral("Scope: %1\nRuntime: %2\nSubstate: %3\nStartup: %4\n\n%5")
            .arg(entry.user ? QStringLiteral("User") : QStringLiteral("System"), entry.runtime,
                 entry.substate.isEmpty() ? QStringLiteral("Unknown") : entry.substate, entry.startup, entry.description));
    }
    void updateServiceActions()
    {
        if (!serviceTable) return;
        auto* entry = selectedService();
        const bool canChange = entry && !options.fixtureMode && !mutationBusy && !serviceLoading && serviceDataReady && detailsReady;
        const auto allowed = entry ? management::availableServiceActions(*entry) : QStringList();
        for (auto it = serviceActions.cbegin(); it != serviceActions.cend(); ++it) it.value()->setEnabled(canChange && allowed.contains(it.key()));
        journal->setEnabled(entry && !serviceLoading && !mutationBusy);
        scope->setEnabled(!mutationBusy); serviceRefresh->setEnabled(!mutationBusy);
        serviceTable->setEnabled(!mutationBusy); serviceSearch->setEnabled(!mutationBusy); serviceFilter->setEnabled(!mutationBusy);
    }
    void setMutationBusy(bool busy)
    {
        mutationBusy.transition(busy, [this](bool active) {
            owner->setProperty("serviceMutationBusy", active);
            updateServiceActions();
            inspectStartup();
            if (owner->onBusyChanged) owner->onBusyChanged(active);
        });
    }
    void changeService(const QString& verb)
    {
        auto* selected = selectedService();
        if (!selected || options.fixtureMode || mutationBusy || !detailsReady || !serviceDataReady
            || !management::availableServiceActions(*selected).contains(verb)) return;
        if (!admission()) return;
        const auto entry = *selected; const int run = generation;
        const auto effect = (verb == QStringLiteral("enable") || verb == QStringLiteral("disable"))
            ? QStringLiteral("This changes startup configuration. It does not start or stop the service now.")
            : QStringLiteral("This changes runtime state and may interrupt networking, login, printing, or other applications.");
        if (!confirm(QStringLiteral("Review service action"), QStringLiteral("Action: %1\nUnit: %2\nScope: %3\n\n%4\n%5")
                .arg(verb, entry.unit, entry.user ? QStringLiteral("User") : QStringLiteral("System"), effect,
                     entry.user ? QStringLiteral("Runs as your desktop user.") : QStringLiteral("Administrator authorization will be requested if required.")))) return;
        if (!admission()) return;
        if (run != generation || !selectedService() || selectedService()->unit != entry.unit) return;
        const auto systemctl = trustedSystemTool(QStringLiteral("systemctl"));
        const auto pkexec = trustedSystemTool(QStringLiteral("pkexec"));
        if (systemctl.isEmpty() || (!entry.user && pkexec.isEmpty())) {
            serviceSummary->setText(QStringLiteral("Required service manager or authorization helper is unavailable. No action started.")); return;
        }
        QString program = systemctl;
        QStringList args{entry.user ? QStringLiteral("--user") : QStringLiteral("--system"), QStringLiteral("--no-pager"), QStringLiteral("--no-ask-password"), verb, QStringLiteral("--"), entry.unit};
        if (!entry.user) { args.prepend(systemctl); program = pkexec; }
        setMutationBusy(true);
        serviceSummary->setText(QStringLiteral("Waiting for service manager: %1 %2…").arg(verb, entry.unit));
        log(QStringLiteral("Service action requested"), verb + QLatin1Char(' ') + entry.unit + (entry.user ? QStringLiteral(" · User") : QStringLiteral(" · System")));
        AsyncCommand::runAction(owner, program, args, 64 * 1024,
            [this, entry, verb](AsyncCommandResult result) {
                setMutationBusy(false);
                const QString outcome = result.ok() ? QStringLiteral("Command completed; refreshing actual state")
                    : QStringLiteral("Action not verified: ") + resultProblem(result);
                log(verb + QLatin1Char(' ') + entry.unit, outcome);
                refreshServices(); inspectStartup();
                if (!result.ok()) QMessageBox::warning(owner, QStringLiteral("Service action not verified"), outcome + QStringLiteral("\nThe service list is being refreshed. No success has been assumed."));
            });
    }
    void showJournal()
    {
        const auto* selected = selectedService(); if (!selected) return;
        const auto entry = *selected;
        auto* dialog = new QDialog(owner); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(QStringLiteral("Service journal · ") + entry.unit); dialog->resize(760, 460);
        auto* layout = new QVBoxLayout(dialog);
        layout->addWidget(textLabel(QStringLiteral("Recent journal entries · %1 scope · Read-only").arg(entry.user ? QStringLiteral("User") : QStringLiteral("System"))));
        auto* output = new QPlainTextEdit; output->setReadOnly(true); output->setObjectName(QStringLiteral("serviceJournalOutput")); layout->addWidget(output, 1);
        auto* close = new QPushButton(QStringLiteral("Close")); layout->addWidget(close); connect(close, &QPushButton::clicked, dialog, &QDialog::close); dialog->show();
        if (options.fixtureMode) { output->setPlainText(QStringLiteral("Sample journal preview. No real service journal was accessed.")); return; }
        const auto program = QStandardPaths::findExecutable(QStringLiteral("journalctl"));
        if (program.isEmpty()) { output->setPlainText(QStringLiteral("journalctl is unavailable.")); return; }
        QStringList args{QStringLiteral("--no-pager"), QStringLiteral("--quiet"), QStringLiteral("-n"), QStringLiteral("100"), QStringLiteral("--output=short-iso"),
            entry.user ? QStringLiteral("--user-unit=") + entry.unit : QStringLiteral("--unit=") + entry.unit};
        output->setPlainText(QStringLiteral("Loading readable journal entries…"));
        const QPointer<QPlainTextEdit> guarded(output);
        journalQuery = AsyncCommand::run(dialog, program, args, 5000, 256 * 1024, [guarded](AsyncCommandResult result) {
            if (!guarded) return;
            guarded->setPlainText(result.ok() ? (result.output.isEmpty() ? QStringLiteral("No readable journal entries found.") : QString::fromUtf8(result.output))
                                            : QStringLiteral("Journal unavailable: ") + resultProblem(result));
        });
    }

    ManagementPage* owner;
    Options options;
    QTabWidget* tabs = nullptr;
    QTableWidget *activity = nullptr, *startupTable = nullptr, *serviceTable = nullptr;
    QComboBox *sourceFilter = nullptr, *startupFilter = nullptr, *scope = nullptr, *serviceFilter = nullptr;
    QLineEdit *startupSearch = nullptr, *serviceSearch = nullptr;
    QLabel *startupSummary = nullptr, *startupName = nullptr, *startupDetails = nullptr, *startupReason = nullptr;
    QLabel *serviceSummary = nullptr, *serviceName = nullptr, *serviceDetails = nullptr, *serviceDependencies = nullptr;
    QPushButton *startupEnable = nullptr, *startupDisable = nullptr, *startupUndo = nullptr, *serviceRefresh = nullptr, *journal = nullptr;
    QMap<QString, QPushButton*> serviceActions;
    QVector<tux::autostart::Entry> startupEntries;
    QVector<management::ServiceEntry> serviceEntries;
    QVector<QPointer<AsyncCommand>> readQueries;
    QPointer<AsyncCommand> detailQuery, journalQuery;
    QPointer<QWidget> watchedWindow;
    QByteArray unitFiles, runtimeUnits;
    QStringList listError;
    int generation = 0, detailGeneration = 0, servicePending = 0;
    bool serviceLoading = false, serviceDataReady = false, detailsReady = false;
    bool listOk = false, runtimeOk = false;
    management::BusyState mutationBusy;
};

ManagementPage::ManagementPage(QWidget* parent) : ManagementPage(Options{}, parent) {}
ManagementPage::ManagementPage(Options options, QWidget* parent)
    : QWidget(parent), impl_(std::make_unique<Impl>(this, std::move(options))) {}
ManagementPage::~ManagementPage()
{
    onBusyChanged = {};
    canMutate = {};
}
void ManagementPage::showStartup() { impl_->tabs->setCurrentIndex(0); }
void ManagementPage::showServices() { impl_->tabs->setCurrentIndex(1); }
bool ManagementPage::hasPendingAction() const { return impl_->mutationBusy.isBusy(); }
bool ManagementPage::isBusy() const { return impl_->mutationBusy.isBusy(); }
bool ManagementPage::fixtureMode() const { return impl_->options.fixtureMode; }
void ManagementPage::showEvent(QShowEvent* event) { QWidget::showEvent(event); impl_->attachWindow(); }
void ManagementPage::hideEvent(QHideEvent* event) { if (!impl_->mutationBusy) impl_->cancelReads(); QWidget::hideEvent(event); }
}
