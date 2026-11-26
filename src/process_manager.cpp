// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_manager.hpp"
#include "tux/process_control.hpp"
#include "tux/process_tree.hpp"
#include "tux/system_monitor_tabs.hpp"

#include <QElapsedTimer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QSet>
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
#include <QListWidget>
#include <QTabBar>
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
    double diskReadBytesPerSecond = 0.0;
    double diskWriteBytesPerSecond = 0.0;
    bool diskIoAvailable = false;
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

struct ProcessIoCounters { quint64 readBytes = 0; quint64 writeBytes = 0; };

bool parseProcessIo(const QString &pidText, ProcessIoCounters *counters)
{
    QFile file(QStringLiteral("/proc/%1/io").arg(pidText));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    bool readOk = false;
    bool writeOk = false;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        if (line.startsWith("read_bytes:")) counters->readBytes = line.mid(sizeof("read_bytes:") - 1).trimmed().toULongLong(&readOk);
        else if (line.startsWith("write_bytes:")) counters->writeBytes = line.mid(sizeof("write_bytes:") - 1).trimmed().toULongLong(&writeOk);
    }
    return readOk && writeOk;
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

QString ioRateText(double bytesPerSecond)
{
    if (bytesPerSecond < 1024.0) return QStringLiteral("%1 B/s").arg(bytesPerSecond, 0, 'f', 0);
    if (bytesPerSecond < 1024.0 * 1024.0) return QStringLiteral("%1 KiB/s").arg(bytesPerSecond / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MiB/s").arg(bytesPerSecond / (1024.0 * 1024.0), 0, 'f', 1);
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
    enum Column { Name, Cpu, Memory, DiskRead, DiskWrite, Pid, ParentPid, Owner, State, ColumnCount };
    enum Role { PidRole = Qt::UserRole + 1, StartTimeRole, UidRole, CpuRole, MemoryRole, DiskReadRole, DiskWriteRole };

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
        if (role == DiskReadRole) return entry.diskReadBytesPerSecond;
        if (role == DiskWriteRole) return entry.diskWriteBytesPerSecond;
        if (role == Qt::ToolTipRole) {
            const QString command = processCommand(QString::number(entry.pid));
            return command.isEmpty() ? entry.name : command;
        }
        if (role != Qt::DisplayRole) return {};
        switch (index.column()) {
        case Name: return entry.name;
        case Cpu: return QStringLiteral("%1%").arg(entry.cpuPercent, 0, 'f', 1);
        case Memory: return memoryText(entry.residentBytes, entry.memoryPercent);
        case DiskRead: return entry.diskIoAvailable ? ioRateText(entry.diskReadBytesPerSecond) : QObject::tr("Unavailable");
        case DiskWrite: return entry.diskIoAvailable ? ioRateText(entry.diskWriteBytesPerSecond) : QObject::tr("Unavailable");
        case Pid: return entry.pid;
        case ParentPid: return entry.parentPid;
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
        case DiskRead: return QObject::tr("Disk read");
        case DiskWrite: return QObject::tr("Disk write");
        case Pid: return QObject::tr("PID");
        case ParentPid: return QObject::tr("Parent PID");
        case Owner: return QObject::tr("User");
        case State: return QObject::tr("Status");
        default: return {};
        }
    }

    void refresh()
    {
        const qint64 elapsedMs = qMax<qint64>(1, elapsed_.restart());
        QHash<QString, quint64> nextCpuTicks;
        QHash<QString, ProcessIoCounters> nextIoCounters;
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
            ProcessIoCounters io;
            if (parseProcessIo(pidText, &io)) {
                entry.diskIoAvailable = true;
                if (previousIoCounters_.contains(identity)) {
                    const ProcessIoCounters previous = previousIoCounters_.value(identity);
                    const double seconds = static_cast<double>(elapsedMs) / 1000.0;
                    entry.diskReadBytesPerSecond = static_cast<double>(io.readBytes >= previous.readBytes ? io.readBytes - previous.readBytes : 0) / seconds;
                    entry.diskWriteBytesPerSecond = static_cast<double>(io.writeBytes >= previous.writeBytes ? io.writeBytes - previous.writeBytes : 0) / seconds;
                }
                nextIoCounters.insert(identity, io);
            }
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
        previousIoCounters_ = std::move(nextIoCounters);
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

    const QList<ProcessEntry> &snapshot() const { return entries_; }

    QJsonArray reportRows(bool includePrivateDetails = false) const
    {
        QJsonArray rows;
        for (const ProcessEntry &entry : entries_) {
            QJsonObject row;
            row.insert(QStringLiteral("pid"), entry.pid);
            row.insert(QStringLiteral("parent_pid"), entry.parentPid);
            if (entry.diskIoAvailable) {
                row.insert(QStringLiteral("disk_read_bytes_per_second"), entry.diskReadBytesPerSecond);
                row.insert(QStringLiteral("disk_write_bytes_per_second"), entry.diskWriteBytesPerSecond);
            }
            row.insert(QStringLiteral("name"), entry.name);
            if (includePrivateDetails) row.insert(QStringLiteral("user"), entry.owner);
            row.insert(QStringLiteral("state"), entry.state);
            row.insert(QStringLiteral("cpu_percent"), entry.cpuPercent);
            row.insert(QStringLiteral("resident_bytes"), static_cast<qint64>(entry.residentBytes));
            if (includePrivateDetails) {
                const QString command = processCommand(QString::number(entry.pid));
                if (!command.isEmpty()) row.insert(QStringLiteral("command_line"), command);
            }
            rows.append(row);
        }
        return rows;
    }

    void resetCpuBaseline()
    {
        previousCpuTicks_.clear();
        previousIoCounters_.clear();
        elapsed_.restart();
    }

private:
    QList<ProcessEntry> entries_;
    QHash<QString, quint64> previousCpuTicks_;
    QHash<QString, ProcessIoCounters> previousIoCounters_;
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
        if (left.column() == ProcessTableModel::DiskRead)
            return source->data(left, ProcessTableModel::DiskReadRole).toDouble()
                < source->data(right, ProcessTableModel::DiskReadRole).toDouble();
        if (left.column() == ProcessTableModel::DiskWrite)
            return source->data(left, ProcessTableModel::DiskWriteRole).toDouble()
                < source->data(right, ProcessTableModel::DiskWriteRole).toDouble();
        if (left.column() == ProcessTableModel::Pid) {
            return source->data(left, ProcessTableModel::PidRole).toLongLong()
                < source->data(right, ProcessTableModel::PidRole).toLongLong();
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }
};
}

ProcessManagerWidget::ProcessManagerWidget(QWidget *parent, bool includeSystemTabs)
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
    auto* navigation=new QListWidget(this);navigation->setObjectName("taskManagerNavigation");navigation->setAccessibleName("Task Manager pages");navigation->setFixedWidth(160);
    auto* workspace=new QHBoxLayout;workspace->addWidget(navigation);workspace->addWidget(tabs,1);layout->addLayout(workspace,1);tabs->tabBar()->hide();
    auto *processPage = new QWidget(tabs);
    auto *processLayout = new QVBoxLayout(processPage);
    processLayout->setContentsMargins(0, 8, 0, 0);
    tabs->addTab(processPage, tr("Processes"));
    if(includeSystemTabs) new SystemMonitorTabs(tabs);

    auto *toolbar = new QVBoxLayout;
    auto *searchRow = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(tr("Search processes by name, user, or PID"));
    search_->setClearButtonEnabled(true);
    search_->setObjectName(QStringLiteral("processSearch"));
    auto *refreshButton = new QPushButton(tr("Refresh"), this);
    endTaskButton_ = new QPushButton(tr("End task"), this);
    endTaskButton_->setObjectName(QStringLiteral("primaryAction"));
    forceStopButton_ = new QPushButton(tr("Force stop"), this);
    forceStopButton_->setObjectName(QStringLiteral("dangerAction"));
    detailsButton_ = new QPushButton(tr("Details"), this);
    detailsButton_->setObjectName(QStringLiteral("secondaryAction"));
    exportButton_ = new QPushButton(tr("Export report…"), this);
    exportButton_->setObjectName(QStringLiteral("processExportButton"));
    endTaskButton_->setEnabled(false);
    forceStopButton_->setEnabled(false);
    detailsButton_->setEnabled(false);
    searchRow->addWidget(search_, 1);
    refreshButton->setObjectName(QStringLiteral("processRefreshButton"));
    searchRow->addWidget(refreshButton);
    refreshSpeed_ = new QComboBox(this);
    refreshSpeed_->setObjectName(QStringLiteral("processRefreshSpeed"));
    refreshSpeed_->addItem(tr("Fast · 1 s"), 1000);
    refreshSpeed_->addItem(tr("Normal · 2 s"), 2000);
    refreshSpeed_->addItem(tr("Low overhead · 5 s"), 5000);
    refreshSpeed_->setCurrentIndex(1);
    searchRow->addWidget(refreshSpeed_);
    pauseButton_ = new QPushButton(tr("Pause processes"), this);
    pauseButton_->setObjectName(QStringLiteral("processPauseButton"));
    searchRow->addWidget(pauseButton_);
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
    table_->setColumnWidth(ProcessTableModel::DiskRead, 110);
    table_->setColumnWidth(ProcessTableModel::DiskWrite, 110);
    table_->setColumnWidth(ProcessTableModel::Pid, 90);
    table_->setColumnWidth(ProcessTableModel::ParentPid, 110);
    table_->setColumnWidth(ProcessTableModel::Owner, 100);
    table_->setColumnWidth(ProcessTableModel::State, 110);
    processLayout->addWidget(table_, 1);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("fileStatus"));
    status_->setText(tr("Process monitoring starts when you open this page."));
    processLayout->addWidget(status_);

    timer_ = new QTimer(this);
    timer_->setObjectName(QStringLiteral("processRefreshTimer"));
    timer_->setInterval(2000);
    connect(timer_, &QTimer::timeout, this, &ProcessManagerWidget::refreshProcesses);
    connect(refreshSpeed_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        timer_->setInterval(refreshSpeed_->currentData().toInt());
        model_->resetCpuBaseline();
        if (!paused_ && isVisible()) refreshProcesses();
    });
    connect(pauseButton_, &QPushButton::clicked, this, [this]() {
        paused_ = !paused_;
        pauseButton_->setText(paused_ ? tr("Resume processes") : tr("Pause processes"));
        if (paused_) {
            timer_->stop();
            status_->setText(tr("Process snapshot paused · Refresh updates once; resource tabs remain independent"));
        } else {
            model_->resetCpuBaseline();
            if (isVisible()) { refreshProcesses(); timer_->start(); }
        }
    });
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
    auto *treePage = new QWidget(tabs);
    auto *treeLayout = new QVBoxLayout(treePage);
    auto *treeInfo = new QLabel(tr("Parent/child process snapshot. Missing, reused or cyclic parents appear as roots. Double-click to inspect a process; this view never stops a process group."), treePage);
    treeInfo->setWordWrap(true);
    treeLayout->addWidget(treeInfo);
    auto *treeToolbar = new QHBoxLayout;
    treeSearch_ = new QLineEdit(treePage);
    treeSearch_->setObjectName(QStringLiteral("processTreeSearch"));
    treeSearch_->setPlaceholderText(tr("Find a process, user or PID in the tree"));
    treeSearch_->setClearButtonEnabled(true);
    treeToolbar->addWidget(treeSearch_, 1);
    auto *expand = new QPushButton(tr("Expand all"), treePage);
    auto *collapse = new QPushButton(tr("Collapse all"), treePage);
    treeToolbar->addWidget(expand); treeToolbar->addWidget(collapse);
    treeLayout->addLayout(treeToolbar);
    tree_ = new QTreeWidget(treePage);
    tree_->setObjectName(QStringLiteral("processTree"));
    tree_->setHeaderLabels({tr("Process"), tr("PID"), tr("CPU %"), tr("Memory"), tr("User")});
    tree_->setUniformRowHeights(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeLayout->addWidget(tree_, 1);
    tabs->addTab(treePage, tr("Process tree"));
    connect(expand, &QPushButton::clicked, tree_, &QTreeWidget::expandAll);
    connect(collapse, &QPushButton::clicked, tree_, &QTreeWidget::collapseAll);
    connect(treeSearch_, &QLineEdit::textChanged, this, [this]() { filterProcessTree(); });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        const qint64 pid = item->data(0, Qt::UserRole).toLongLong();
        const auto *entry = model_->find(pid);
        if (entry == nullptr || entry->startTicks != item->data(0, Qt::UserRole + 1).toULongLong()) return;
        const auto source = model_->indexForPid(pid);
        // A tree selection should be inspectable even when the list is filtered.
        search_->clear();
        table_->selectRow(proxy_->mapFromSource(source).row());
        showProcessDetails();
    });
    for(int i=0;i<tabs->count();++i)navigation->addItem(tabs->tabText(i));
    connect(navigation,&QListWidget::currentRowChanged,tabs,&QTabWidget::setCurrentIndex);
    connect(tabs,&QTabWidget::currentChanged,navigation,QOverload<int>::of(&QListWidget::setCurrentRow));
    navigation->setCurrentRow(0);
    if(!includeSystemTabs) {
        navigation->hide();
        tabs->tabBar()->show();
        title->hide(); description->hide();
        layout->setContentsMargins(0,0,0,0);
    }
}

void ProcessManagerWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!paused_) { refreshProcesses(); timer_->start(); }
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
    refreshProcessTree();
    status_->setText(paused_
        ? tr("%1 processes · paused snapshot · resource tabs remain independent").arg(model_->rowCount())
        : tr("%1 processes · refresh interval %2 seconds · CPU is a share of total system capacity")
            .arg(model_->rowCount()).arg(timer_->interval() / 1000));
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


void ProcessManagerWidget::refreshProcessTree()
{
    if (tree_ == nullptr) return;
    QSet<QString> expanded;
    QString selected;
    const auto identity = [](QTreeWidgetItem *item) {
        return QStringLiteral("%1:%2").arg(item->data(0, Qt::UserRole).toLongLong())
            .arg(item->data(0, Qt::UserRole + 1).toULongLong());
    };
    const bool initial = tree_->topLevelItemCount() == 0;
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        if ((*it)->isExpanded()) expanded.insert(identity(*it));
        if ((*it)->isSelected()) selected = identity(*it);
    }
    tree_->clear();
    const auto &snapshot = model_->snapshot();
    QVector<ProcessTreeIdentity> nodes;
    for (const auto &entry : snapshot) nodes.append({entry.pid, entry.parentPid, entry.startTicks});
    const auto parents = processTreeParents(nodes);
    QVector<QTreeWidgetItem *> items;
    for (const auto &entry : snapshot) {
        auto *item = new QTreeWidgetItem({entry.name, QString::number(entry.pid),
            QString::number(entry.cpuPercent, 'f', 1), memoryText(entry.residentBytes, entry.memoryPercent), entry.owner});
        item->setData(0, Qt::UserRole, entry.pid);
        item->setData(0, Qt::UserRole + 1, QVariant::fromValue(entry.startTicks));
        items.append(item);
    }
    for (int i = 0; i < items.size(); ++i) {
        if (parents[i] >= 0) items[parents[i]]->addChild(items[i]);
        else tree_->addTopLevelItem(items[i]);
    }
    for (auto *item : items) {
        item->setExpanded(expanded.contains(identity(item)) || (initial && item->parent() == nullptr));
        if (identity(item) == selected) tree_->setCurrentItem(item);
    }
    filterProcessTree();
}

void ProcessManagerWidget::filterProcessTree()
{
    if (tree_ == nullptr) return;
    const QString query = treeSearch_->text().trimmed();
    QSet<QTreeWidgetItem *> shown;
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        auto *item = *it;
        bool matches = query.isEmpty();
        for (int column = 0; !matches && column < tree_->columnCount(); ++column)
            matches = item->text(column).contains(query, Qt::CaseInsensitive);
        if (!matches) continue;
        if (query.isEmpty()) shown.insert(item);
        else {
            for (auto *ancestor = item; ancestor != nullptr && !shown.contains(ancestor); ancestor = ancestor->parent()) {
                shown.insert(ancestor);
                if (ancestor != item) ancestor->setExpanded(true);
            }
        }
    }
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) (*it)->setHidden(!shown.contains(*it));
}

void ProcessManagerWidget::exportProcessReport()
{
    QDialog privacy(this);
    privacy.setObjectName(QStringLiteral("processReportPrivacy"));
    privacy.setWindowTitle(tr("Choose report details"));
    auto *layout = new QVBoxLayout(&privacy);
    auto *warning = new QLabel(tr("The standard report omits usernames and command lines. Process names, PIDs and metrics can still reveal activity. Review any report before sharing. Files are saved locally; nothing is uploaded."), &privacy);
    warning->setWordWrap(true);
    layout->addWidget(warning);
    auto *privateDetails = new QCheckBox(tr("Include usernames and command lines (may contain paths or secrets)"), &privacy);
    privateDetails->setObjectName(QStringLiteral("includePrivateProcessDetails"));
    privateDetails->setChecked(false);
    layout->addWidget(privateDetails);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &privacy);
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, &privacy, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &privacy, &QDialog::reject);
    layout->addWidget(buttons);
    if (privacy.exec() != QDialog::Accepted) return;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export process report"), QStringLiteral("tux-process-report.json"),
        tr("JSON report (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonObject report;
    report.insert(QStringLiteral("created_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    report.insert(QStringLiteral("process_count"), model_->rowCount());
    report.insert(QStringLiteral("includes_private_details"), privateDetails->isChecked());
    report.insert(QStringLiteral("processes"), model_->reportRows(privateDetails->isChecked()));
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
    const auto outcome=tux::signalVerifiedProcess(static_cast<pid_t>(pid),startTicks,static_cast<uid_t>(::geteuid()),force?SIGKILL:SIGTERM);
    if (!outcome.sent) {
        status_->setText(tr("Could not stop %1: %2").arg(processName,outcome.message));
        return;
    }
    status_->setText(force ? tr("Force-stop signal sent to %1.").arg(processName)
                           : tr("Close request sent to %1.").arg(processName));
}

