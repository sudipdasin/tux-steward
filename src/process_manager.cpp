// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_manager.hpp"
#include "tux/system_monitor_tabs.hpp"

#include <QElapsedTimer>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QHideEvent>
#include <QDir>
#include <QAbstractItemModel>
#include <QAbstractTableModel>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QShortcut>
#include <QThread>
#include <QVariant>
#include <QHash>
#include <QVector>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <limits>
#include <utility>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

namespace {
struct ProcessEntry {
    qint64 pid = 0;
    qint64 parentPid = 0;
    QString name;
    QString owner;
    QString state;
    quint64 startTicks = 0;
    quint64 cpuTicks = 0;
    quint64 residentBytes = 0;
    double cpuPercent = 0.0;
    double memoryPercent = 0.0;
    uint uid = std::numeric_limits<uint>::max();
};

bool parseProcessStat(const QString &pidText, ProcessEntry *entry)
{
    QFile file(QStringLiteral("/proc/%1/stat").arg(pidText));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray line = file.readLine().trimmed();
    const qsizetype openParen = line.indexOf('(');
    const qsizetype closeParen = line.lastIndexOf(')');
    if (openParen <= 0 || closeParen <= openParen || closeParen + 2 >= line.size()) {
        return false;
    }
    bool pidOk = false;
    const qint64 pid = line.left(openParen).trimmed().toLongLong(&pidOk);
    if (!pidOk) {
        return false;
    }
    const QList<QByteArray> fields = line.mid(closeParen + 2).simplified().split(' ');
    // The tail starts at proc(5) field 3 (state). CPU, start time, and RSS are
    // fields 14, 15, 22, and 24 in the full stat record.
    if (fields.size() < 22) {
        return false;
    }
    bool userTicksOk = false;
    bool systemTicksOk = false;
    bool parentPidOk = false;
    bool startTicksOk = false;
    bool rssOk = false;
    entry->pid = pid;
    entry->name = QString::fromLocal8Bit(line.mid(openParen + 1, closeParen - openParen - 1));
    entry->state = QString::fromLatin1(fields.at(0));
    entry->parentPid = fields.at(1).toLongLong(&parentPidOk);
    const quint64 userTicks = fields.at(11).toULongLong(&userTicksOk);
    const quint64 systemTicks = fields.at(12).toULongLong(&systemTicksOk);
    entry->startTicks = fields.at(19).toULongLong(&startTicksOk);
    const qint64 residentPages = fields.at(21).toLongLong(&rssOk);
    if (!parentPidOk || !userTicksOk || !systemTicksOk || !startTicksOk || !rssOk) {
        return false;
    }
    entry->cpuTicks = userTicks + systemTicks;
    const long pageSize = ::sysconf(_SC_PAGESIZE);
    if (residentPages > 0 && pageSize > 0) {
        entry->residentBytes = static_cast<quint64>(residentPages) * static_cast<quint64>(pageSize);
    }
    return true;
}

QString processOwner(const QString &pidText, uint *uidOut)
{
    QFile statusFile(QStringLiteral("/proc/%1/status").arg(pidText));
    if (!statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QObject::tr("Unknown");
    }
    while (!statusFile.atEnd()) {
        const QByteArray line = statusFile.readLine();
        if (!line.startsWith("Uid:")) {
            continue;
        }
        const QList<QByteArray> values = line.mid(4).simplified().split(' ');
        if (values.isEmpty()) {
            break;
        }
        bool ok = false;
        const uint uid = values.front().toUInt(&ok);
        if (!ok) {
            break;
        }
        *uidOut = uid;
        if (uid == static_cast<uint>(::geteuid())) {
            return QObject::tr("You");
        }
        static QHash<uint, QString> userNames;
        if (userNames.contains(uid)) {
            return userNames.value(uid);
        }
        long recommendedSize = ::sysconf(_SC_GETPW_R_SIZE_MAX);
        if (recommendedSize < 1024) {
            recommendedSize = 16384;
        }
        QVector<char> buffer(static_cast<qsizetype>(recommendedSize));
        struct passwd record {};
        struct passwd *found = nullptr;
        const int lookup = ::getpwuid_r(static_cast<uid_t>(uid), &record, buffer.data(),
                                        static_cast<size_t>(buffer.size()), &found);
        const QString name = lookup == 0 && found != nullptr
            ? QString::fromLocal8Bit(record.pw_name) : QString::number(uid);
        userNames.insert(uid, name);
        return name;
    }
    return QObject::tr("Unknown");
}

QString processCommand(const QString &pidText)
{
    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(pidText));
    if (!cmdline.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray command = cmdline.readAll();
    command.replace('\0', ' ');
    return QString::fromLocal8Bit(command.trimmed());
}

quint64 totalMemoryBytes()
{
    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (!meminfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    while (!meminfo.atEnd()) {
        const QByteArray line = meminfo.readLine();
        if (!line.startsWith("MemTotal:")) {
            continue;
        }
        const QList<QByteArray> values = line.mid(9).simplified().split(' ');
        bool ok = false;
        const quint64 kibibytes = values.value(0).toULongLong(&ok);
        return ok ? kibibytes * 1024U : 0;
    }
    return 0;
}

QString memoryText(quint64 bytes, double percent)
{
    const double mebibytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mebibytes >= 1024.0) {
        return QStringLiteral("%1 GiB (%2%)").arg(mebibytes / 1024.0, 0, 'f', 1).arg(percent, 0, 'f', 1);
    }
    return QStringLiteral("%1 MiB (%2%)").arg(mebibytes, 0, 'f', 1).arg(percent, 0, 'f', 1);
}

QString processStateName(const QString &state)
{
    if (state == QStringLiteral("R")) return QObject::tr("Running");
    if (state == QStringLiteral("S")) return QObject::tr("Sleeping");
    if (state == QStringLiteral("D")) return QObject::tr("Waiting");
    if (state == QStringLiteral("T")) return QObject::tr("Stopped");
    if (state == QStringLiteral("Z")) return QObject::tr("Zombie");
    return state;
}
}

class ProcessTableModel final : public QAbstractTableModel {
public:
    enum Column { Name, Cpu, Memory, Pid, Owner, State, ColumnCount };
    enum Role { PidRole = Qt::UserRole + 1, StartTimeRole, UidRole, CpuRole, MemoryRole };

    explicit ProcessTableModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent), cpuClockTicks_(static_cast<double>(::sysconf(_SC_CLK_TCK)))
    {
        if (cpuClockTicks_ <= 0.0) {
            cpuClockTicks_ = 100.0;
        }
        const int logicalCores = QThread::idealThreadCount();
        const uint cores = logicalCores > 0 ? static_cast<uint>(logicalCores) : 1U;
        coreCount_ = static_cast<double>(cores);
        totalMemory_ = totalMemoryBytes();
        elapsed_.start();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : entries_.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
            return {};
        }
        const ProcessEntry &entry = entries_.at(index.row());
        if (role == PidRole) return entry.pid;
        if (role == StartTimeRole) return QVariant::fromValue(entry.startTicks);
        if (role == UidRole) return entry.uid;
        if (role == CpuRole) return entry.cpuPercent;
        if (role == MemoryRole) return QVariant::fromValue(entry.residentBytes);
        if (role == Qt::ToolTipRole) {
            const QString command = processCommand(QString::number(entry.pid));
            return command.isEmpty() ? entry.name : command;
        }
        if (role != Qt::DisplayRole) return {};
        switch (index.column()) {
        case Name: return entry.name;
        case Cpu: return QStringLiteral("%1%").arg(entry.cpuPercent, 0, 'f', 1);
        case Memory: return memoryText(entry.residentBytes, entry.memoryPercent);
        case Pid: return entry.pid;
        case Owner: return entry.owner;
        case State: return processStateName(entry.state);
        default: return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return {};
        }
        switch (section) {
        case Name: return QObject::tr("Process");
        case Cpu: return QObject::tr("CPU");
        case Memory: return QObject::tr("Memory");
        case Pid: return QObject::tr("PID");
        case Owner: return QObject::tr("User");
        case State: return QObject::tr("Status");
        default: return {};
        }
    }

    void refresh()
    {
        const qint64 elapsedMs = qMax<qint64>(1, elapsed_.restart());
        QHash<QString, quint64> nextCpuTicks;
        QList<ProcessEntry> nextEntries;
        QDir proc(QStringLiteral("/proc"));
        const QStringList pids = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &pidText : pids) {
            if (pidText.isEmpty() || !pidText.front().isDigit()) {
                continue;
            }
            ProcessEntry entry;
            if (!parseProcessStat(pidText, &entry)) {
                continue;
            }
            const QString identity = QStringLiteral("%1:%2").arg(entry.pid).arg(entry.startTicks);
            nextCpuTicks.insert(identity, entry.cpuTicks);
            if (previousCpuTicks_.contains(identity)) {
                const quint64 delta = entry.cpuTicks - previousCpuTicks_.value(identity);
                const double cpuSeconds = static_cast<double>(delta) / cpuClockTicks_;
                entry.cpuPercent = qBound(0.0, cpuSeconds / (static_cast<double>(elapsedMs) / 1000.0)
                                                 / coreCount_ * 100.0, 100.0);
            }
            entry.owner = processOwner(pidText, &entry.uid);
            entry.memoryPercent = totalMemory_ == 0 ? 0.0
                : static_cast<double>(entry.residentBytes) / static_cast<double>(totalMemory_) * 100.0;
            nextEntries.push_back(std::move(entry));
        }
        std::sort(nextEntries.begin(), nextEntries.end(), [](const ProcessEntry &left, const ProcessEntry &right) {
            return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
        });
        beginResetModel();
        entries_ = std::move(nextEntries);
        previousCpuTicks_ = std::move(nextCpuTicks);
        endResetModel();
    }

    const ProcessEntry *find(qint64 pid) const
    {
        for (const ProcessEntry &entry : entries_) {
            if (entry.pid == pid) {
                return &entry;
            }
        }
        return nullptr;
    }

    QModelIndex indexForPid(qint64 pid) const
    {
        for (qsizetype row = 0; row < entries_.size(); ++row) {
            if (entries_.at(row).pid == pid) {
                return index(static_cast<int>(row), 0);
            }
        }
        return {};
    }

    bool stillSameProcess(qint64 pid, quint64 startTicks) const
    {
        ProcessEntry current;
        return parseProcessStat(QString::number(pid), &current) && current.startTicks == startTicks;
    }

    QJsonArray reportRows() const
    {
        QJsonArray rows;
        for (const ProcessEntry &entry : entries_) {
            QJsonObject row;
            row.insert(QStringLiteral("pid"), entry.pid);
            row.insert(QStringLiteral("name"), entry.name);
            row.insert(QStringLiteral("user"), entry.owner);
            row.insert(QStringLiteral("state"), entry.state);
            row.insert(QStringLiteral("cpu_percent"), entry.cpuPercent);
            row.insert(QStringLiteral("resident_bytes"), static_cast<qint64>(entry.residentBytes));
            const QString command = processCommand(QString::number(entry.pid));
            if (!command.isEmpty()) {
                row.insert(QStringLiteral("command_line"), command);
            }
            rows.append(row);
        }
        return rows;
    }

    void resetCpuBaseline()
    {
        previousCpuTicks_.clear();
        elapsed_.restart();
    }

private:
    QList<ProcessEntry> entries_;
    QHash<QString, quint64> previousCpuTicks_;
    QElapsedTimer elapsed_;
    double cpuClockTicks_ = 100.0;
    double coreCount_ = 1.0;
    quint64 totalMemory_ = 0;
};

namespace {
class ProcessSortProxyModel final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        const auto *source = static_cast<const ProcessTableModel *>(sourceModel());
        if (left.column() == ProcessTableModel::Cpu) {
            return source->data(left, ProcessTableModel::CpuRole).toDouble()
                < source->data(right, ProcessTableModel::CpuRole).toDouble();
        }
        if (left.column() == ProcessTableModel::Memory) {
            return source->data(left, ProcessTableModel::MemoryRole).toULongLong()
                < source->data(right, ProcessTableModel::MemoryRole).toULongLong();
        }
        if (left.column() == ProcessTableModel::Pid) {
            return source->data(left, ProcessTableModel::PidRole).toLongLong()
                < source->data(right, ProcessTableModel::PidRole).toLongLong();
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }
};
}

ProcessManagerWidget::ProcessManagerWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("Task Manager"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *description = new QLabel(
        tr("See which processes are using your computer. Tux Steward only stops processes owned by your account; system services stay protected."), this);
    description->setObjectName(QStringLiteral("intro"));
    description->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(description);

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("taskManagerTabs"));
    layout->addWidget(tabs, 1);
    auto *processPage = new QWidget(tabs);
    auto *processLayout = new QVBoxLayout(processPage);
    processLayout->setContentsMargins(0, 8, 0, 0);
    tabs->addTab(processPage, tr("Processes"));
    new SystemMonitorTabs(tabs);

    auto *toolbar = new QVBoxLayout;
    auto *searchRow = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Search processes by name, user, or PID"));
    search_->setClearButtonEnabled(true);
    auto *refreshButton = new QPushButton(tr("Refresh"), this);
    endTaskButton_ = new QPushButton(tr("End task"), this);
    endTaskButton_->setObjectName(QStringLiteral("primaryAction"));
    forceStopButton_ = new QPushButton(tr("Force stop"), this);
    forceStopButton_->setObjectName(QStringLiteral("dangerAction"));
    detailsButton_ = new QPushButton(tr("Details"), this);
    detailsButton_->setObjectName(QStringLiteral("secondaryAction"));
    exportButton_ = new QPushButton(tr("Export report…"), this);
    exportButton_->setObjectName(QStringLiteral("secondaryAction"));
    endTaskButton_->setEnabled(false);
    forceStopButton_->setEnabled(false);
    detailsButton_->setEnabled(false);
    searchRow->addWidget(search_, 1);
    searchRow->addWidget(refreshButton);
    toolbar->addLayout(searchRow);
    auto *actionsRow = new QHBoxLayout;
    actionsRow->addWidget(endTaskButton_);
    actionsRow->addWidget(forceStopButton_);
    actionsRow->addWidget(detailsButton_);
    actionsRow->addWidget(exportButton_);
    toolbar->addLayout(actionsRow);
    processLayout->addLayout(toolbar);

    model_ = new ProcessTableModel(this);
    proxy_ = new ProcessSortProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy_->setFilterKeyColumn(-1);
    proxy_->setSortRole(Qt::DisplayRole);

    table_ = new QTableView(this);
    table_->setObjectName(QStringLiteral("processTable"));
    table_->setModel(proxy_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setSortingEnabled(true);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(ProcessTableModel::Name, QHeaderView::Stretch);
    table_->setColumnWidth(ProcessTableModel::Cpu, 90);
    table_->setColumnWidth(ProcessTableModel::Memory, 160);
    table_->setColumnWidth(ProcessTableModel::Pid, 90);
    table_->setColumnWidth(ProcessTableModel::Owner, 100);
    table_->setColumnWidth(ProcessTableModel::State, 110);
    processLayout->addWidget(table_, 1);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("fileStatus"));
    status_->setText(tr("Process monitoring starts when you open this page."));
    processLayout->addWidget(status_);

    timer_ = new QTimer(this);
    timer_->setInterval(2000);
    connect(timer_, &QTimer::timeout, this, &ProcessManagerWidget::refreshProcesses);
    connect(refreshButton, &QPushButton::clicked, this, &ProcessManagerWidget::refreshProcesses);
    connect(search_, &QLineEdit::textChanged, proxy_, &QSortFilterProxyModel::setFilterFixedString);
    connect(endTaskButton_, &QPushButton::clicked, this, [this]() { stopSelectedProcess(false); });
    connect(forceStopButton_, &QPushButton::clicked, this, [this]() { stopSelectedProcess(true); });
    connect(exportButton_, &QPushButton::clicked, this, &ProcessManagerWidget::exportProcessReport);
    connect(detailsButton_, &QPushButton::clicked, this, &ProcessManagerWidget::showProcessDetails);
    connect(table_, &QTableView::doubleClicked, this, [this]() { showProcessDetails(); });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const qint64 pid = selectedPid();
        const ProcessEntry *entry = model_->find(pid);
        const bool canStop = entry != nullptr && entry->uid == static_cast<uint>(::geteuid()) && pid != ::getpid();
        endTaskButton_->setEnabled(canStop);
        forceStopButton_->setEnabled(canStop);
        detailsButton_->setEnabled(entry != nullptr);
    });
    connect(new QShortcut(QKeySequence(Qt::Key_Delete), this), &QShortcut::activated,
            this, [this]() { stopSelectedProcess(false); });
}

void ProcessManagerWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshProcesses();
    timer_->start();
}

void ProcessManagerWidget::hideEvent(QHideEvent *event)
{
    timer_->stop();
    model_->resetCpuBaseline();
    QWidget::hideEvent(event);
}

void ProcessManagerWidget::refreshProcesses()
{
    const qint64 selected = selectedPid();
    model_->refresh();
    status_->setText(tr("%1 processes  ·  refreshes every 2 seconds  ·  CPU is shown as a share of total system capacity")
                         .arg(model_->rowCount()));
    if (selected > 0) {
        const QModelIndex sourceIndex = model_->indexForPid(selected);
        if (sourceIndex.isValid()) {
            const QModelIndex visibleIndex = proxy_->mapFromSource(sourceIndex);
            if (visibleIndex.isValid()) {
                table_->selectRow(visibleIndex.row());
            }
        }
    }
}

void ProcessManagerWidget::exportProcessReport()
{
    const auto answer = QMessageBox::warning(
        this, tr("Review report privacy"),
        tr("This report can include process names, user names, command-line arguments, and file paths. It is saved only on this computer. Review it before sharing."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export process report"), QStringLiteral("tux-process-report.json"),
        tr("JSON report (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonObject report;
    report.insert(QStringLiteral("created_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    report.insert(QStringLiteral("process_count"), model_->rowCount());
    report.insert(QStringLiteral("processes"), model_->reportRows());
    const QByteArray bytes = QJsonDocument(report).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        QMessageBox::critical(this, tr("Export failed"),
                              tr("Could not save the report. Check that you have permission and enough disk space."));
        return;
    }
    status_->setText(tr("Process report saved locally · review it before sharing"));
}

void ProcessManagerWidget::showProcessDetails()
{
    const qint64 pid = selectedPid();
    const ProcessEntry *entry = model_->find(pid);
    if (entry == nullptr) {
        status_->setText(tr("Select a process to view its details."));
        return;
    }
    const ProcessEntry process = *entry;
    if (!model_->stillSameProcess(process.pid, process.startTicks)) {
        status_->setText(tr("That process has already exited. Refreshing the list."));
        model_->refresh();
        return;
    }

    const QString processId = QString::number(process.pid);
    const QString command = processCommand(processId);
    const QString executable = QFileInfo(QStringLiteral("/proc/%1/exe").arg(processId)).symLinkTarget();
    const QString workingDirectory = QFileInfo(QStringLiteral("/proc/%1/cwd").arg(processId)).symLinkTarget();
    const ProcessEntry *parent = model_->find(process.parentPid);
    const QString parentName = parent != nullptr
        ? QStringLiteral("%1 (%2)").arg(parent->name).arg(process.parentPid)
        : QString::number(process.parentPid);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Process details — %1").arg(process.name));
    dialog.setMinimumWidth(540);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    const auto addValue = [form, &dialog](const QString &label, const QString &value) {
        auto *text = new QLabel(value.isEmpty() ? QObject::tr("Unavailable") : value, &dialog);
        text->setTextFormat(Qt::PlainText);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setWordWrap(true);
        form->addRow(label, text);
    };
    addValue(tr("Process"), process.name);
    addValue(tr("Process ID"), QString::number(process.pid));
    addValue(tr("Parent process"), parentName);
    addValue(tr("User"), process.owner);
    addValue(tr("Status"), processStateName(process.state));
    addValue(tr("CPU"), QStringLiteral("%1%").arg(process.cpuPercent, 0, 'f', 1));
    addValue(tr("Memory"), memoryText(process.residentBytes, process.memoryPercent));
    addValue(tr("Executable"), executable);
    addValue(tr("Working directory"), workingDirectory);
    layout->addLayout(form);

    auto *commandHeading = new QLabel(tr("Command line"), &dialog);
    layout->addWidget(commandHeading);
    auto *commandText = new QPlainTextEdit(&dialog);
    commandText->setReadOnly(true);
    commandText->setPlainText(command.isEmpty() ? tr("Unavailable") : command);
    commandText->setMaximumHeight(100);
    layout->addWidget(commandText);
    auto *closeButton = new QPushButton(tr("Close"), &dialog);
    closeButton->setDefault(true);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    dialog.exec();
}

qint64 ProcessManagerWidget::selectedPid() const
{
    const QModelIndexList rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return -1;
    }
    return proxy_->data(proxy_->index(rows.front().row(), ProcessTableModel::Pid), Qt::DisplayRole).toLongLong();
}

void ProcessManagerWidget::stopSelectedProcess(bool force)
{
    const qint64 pid = selectedPid();
    const ProcessEntry *entry = model_->find(pid);
    if (entry == nullptr) {
        status_->setText(tr("Select a process you own first."));
        return;
    }
    if (entry->uid != static_cast<uint>(::geteuid()) || pid == ::getpid()) {
        QMessageBox::information(this, tr("Process protected"),
                                 tr("Tux Steward can only stop processes owned by your account, and it can’t stop itself."));
        return;
    }
    const quint64 startTicks = entry->startTicks;
    const QString processName = entry->name;
    if (!model_->stillSameProcess(pid, startTicks)) {
        status_->setText(tr("That process has already exited. Refreshing the list."));
        model_->refresh();
        return;
    }
    const QString prompt = force
        ? tr("Force stop %1? The application won’t have a chance to save its work.").arg(processName)
        : tr("End %1? Tux Steward will ask the application to close normally. Unsaved work could be lost.").arg(processName);
    const auto answer = QMessageBox::warning(this, force ? tr("Force stop process") : tr("End task"), prompt,
                                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }
    if (!model_->stillSameProcess(pid, startTicks)) {
        status_->setText(tr("That process has already exited."));
        return;
    }
    if (::kill(static_cast<pid_t>(pid), force ? SIGKILL : SIGTERM) != 0) {
        status_->setText(tr("Couldn’t stop %1: %2").arg(processName, QString::fromLocal8Bit(std::strerror(errno))));
        return;
    }
    status_->setText(force ? tr("Force-stop signal sent to %1.").arg(processName)
                           : tr("Close request sent to %1.").arg(processName));
}
