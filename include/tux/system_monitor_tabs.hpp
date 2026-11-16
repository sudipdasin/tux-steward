// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QObject>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

// Lightweight Linux resource views for the Task Manager. Data comes directly
// from procfs/sysfs and standard Qt APIs; optional service/Bluetooth tools are
// detected at runtime, so they are not hard dependencies.
class SystemMonitorTabs final : public QObject {
public:
    explicit SystemMonitorTabs(QTabWidget *tabs) : QObject(tabs), tabs_(tabs)
    {
        addReportTab(tr("Performance"), &performance_);
        addReportTab(tr("App history"), &history_);
        addReportTab(tr("Users"), &users_);
        addReportTab(tr("Details"), &details_);
        addReportTab(tr("Network"), &network_);
        addReportTab(tr("Disk"), &disk_);
        addReportTab(tr("Bluetooth"), &bluetooth_);
        addReportTab(tr("Startup apps"), &startup_);
        addReportTab(tr("Services"), &services_);

        autoRefresh_ = new QTimer(tabs_);
        autoRefresh_->setInterval(2000);
        connect(autoRefresh_, &QTimer::timeout, tabs_, [this]() { refreshCurrent(); });
        connect(tabs_, &QTabWidget::currentChanged, tabs_, [this]() { refreshCurrent(); });
        refreshCurrent();
    }

private:
    void addReportTab(const QString &name, QPlainTextEdit **view)
    {
        QWidget *page = new QWidget(tabs_);
        auto *layout = new QVBoxLayout(page);
        QComboBox *filter = nullptr;
        if (name == tr("Startup apps")) {
            filter = new QComboBox(page);
            filter->addItems({tr("All apps"), tr("User / third-party apps"), tr("System apps")});
        } else if (name == tr("Services")) {
            filter = new QComboBox(page);
            filter->addItems({tr("All services"), tr("System services"), tr("User services")});
        }
        if (filter != nullptr) {
            filter->setMaximumWidth(260);
            layout->addWidget(filter, 0, Qt::AlignLeft);
            filters_.insert(name, filter);
            connect(filter, qOverload<int>(&QComboBox::currentIndexChanged), tabs_, [this]() { refreshCurrent(); });
        }
        auto *text = new QPlainTextEdit(page);
        text->setReadOnly(true);
        text->setLineWrapMode(QPlainTextEdit::NoWrap);
        text->setPlaceholderText(tr("Reading system information…"));
        *view = text;
        layout->addWidget(text, 1);
        tabs_->addTab(page, name);
        if (name == tr("Bluetooth")) {
            auto *openSettings = new QPushButton(tr("Open Bluetooth settings…"), page);
            layout->addWidget(openSettings, 0, Qt::AlignLeft);
            connect(openSettings, &QPushButton::clicked, tabs_, [page]() {
                const QString blueman = QStandardPaths::findExecutable(QStringLiteral("blueman-manager"));
                const QString gnome = QStandardPaths::findExecutable(QStringLiteral("gnome-control-center"));
                const QString xfce = QStandardPaths::findExecutable(QStringLiteral("xfce4-settings-manager"));
                if (!blueman.isEmpty()) QProcess::startDetached(blueman, {});
                else if (!gnome.isEmpty()) QProcess::startDetached(gnome, {QStringLiteral("bluetooth")});
                else if (!xfce.isEmpty()) QProcess::startDetached(xfce, {});
            });
        }
    }

    static QString read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        return QString::fromLocal8Bit(file.readAll());
    }

    static QString performanceReport()
    {
        const QString stat = read(QStringLiteral("/proc/stat"));
        QString cpu = QObject::tr("CPU usage: unavailable");
        const QStringList lines = stat.split(QLatin1Char('\n'));
        if (!lines.isEmpty() && lines.front().startsWith(QStringLiteral("cpu "))) {
            static quint64 previousTotal = 0;
            static quint64 previousIdle = 0;
            const QStringList values = lines.front().simplified().split(QLatin1Char(' '));
            quint64 total = 0;
            bool ok = false;
            for (qsizetype i = 1; i < values.size(); ++i) {
                const quint64 value = values.at(i).toULongLong(&ok);
                if (ok) total += value;
            }
            const quint64 idle = values.value(4).toULongLong();
            if (previousTotal != 0 && total > previousTotal) {
                const quint64 deltaTotal = total - previousTotal;
                const quint64 deltaIdle = idle - previousIdle;
                const double usage = 100.0 * static_cast<double>(deltaTotal - qMin(deltaTotal, deltaIdle))
                                     / static_cast<double>(deltaTotal);
                cpu = QObject::tr("CPU usage: %1%").arg(usage, 0, 'f', 1);
            } else {
                cpu = QObject::tr("CPU usage: collecting first sample…");
            }
            previousTotal = total;
            previousIdle = idle;
        }

        QString memory;
        const QStringList memLines = read(QStringLiteral("/proc/meminfo")).split(QLatin1Char('\n'));
        quint64 totalKiB = 0, availableKiB = 0;
        for (const QString &line : memLines) {
            if (line.startsWith(QStringLiteral("MemTotal:"))) totalKiB = line.simplified().split(QLatin1Char(' ')).value(1).toULongLong();
            if (line.startsWith(QStringLiteral("MemAvailable:"))) availableKiB = line.simplified().split(QLatin1Char(' ')).value(1).toULongLong();
        }
        if (totalKiB > 0) {
            const quint64 usedKiB = totalKiB - qMin(totalKiB, availableKiB);
            memory = QObject::tr("Memory: %1 GiB used of %2 GiB (%3%)")
                .arg(static_cast<double>(usedKiB) / 1048576.0, 0, 'f', 2)
                .arg(static_cast<double>(totalKiB) / 1048576.0, 0, 'f', 2)
                .arg(100.0 * static_cast<double>(usedKiB) / static_cast<double>(totalKiB), 0, 'f', 1);
        } else memory = QObject::tr("Memory: unavailable");

        const QString load = read(QStringLiteral("/proc/loadavg")).trimmed();
        const QStringList cpuDirs = QDir(QStringLiteral("/sys/devices/system/cpu"))
            .entryList({QStringLiteral("cpu[0-9]*")}, QDir::Dirs);
        return QStringLiteral("%1\n%2\n%3\n\n%4")
            .arg(cpu, memory, QObject::tr("Logical processors: %1").arg(cpuDirs.size()),
                 QObject::tr("System load averages (1, 5, 15 min): %1").arg(load.isEmpty() ? QObject::tr("unavailable") : load));
    }

    static QString networkReport()
    {
        const QStringList lines = read(QStringLiteral("/proc/net/dev")).split(QLatin1Char('\n'));
        QString out = QObject::tr("Interface                         Received       Sent\n");
        for (qsizetype i = 2; i < lines.size(); ++i) {
            const QString line = lines.at(i).trimmed();
            if (line.isEmpty() || !line.contains(QLatin1Char(':'))) continue;
            const qsizetype colon = line.indexOf(QLatin1Char(':'));
            const QString iface = line.left(colon).trimmed();
            const QStringList counters = line.mid(colon + 1).simplified().split(QLatin1Char(' '));
            if (counters.size() < 9) continue;
            const quint64 received = counters.at(0).toULongLong();
            const quint64 sent = counters.at(8).toULongLong();
            out += QStringLiteral("%1  %2  %3\n").arg(iface.leftJustified(30))
                .arg(formatBytes(received).rightJustified(12), formatBytes(sent).rightJustified(12));
        }
        return out + QObject::tr("\nTotals are since boot. Per-interface counters are read from Linux procfs.");
    }

    static QString formatBytes(quint64 bytes)
    {
        double value = static_cast<double>(bytes);
        const QStringList units{QStringLiteral("B"), QStringLiteral("KiB"), QStringLiteral("MiB"), QStringLiteral("GiB"), QStringLiteral("TiB")};
        qsizetype unit = 0;
        while (value >= 1024.0 && unit < units.size() - 1) { value /= 1024.0; ++unit; }
        return QStringLiteral("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 1).arg(units.at(unit));
    }

    static QString diskReport()
    {
        QString out = QObject::tr("Mounted filesystems\n\n");
        const auto volumes = QStorageInfo::mountedVolumes();
        for (const QStorageInfo &volume : volumes) {
            if (!volume.isValid() || !volume.isReady()) continue;
            const quint64 total = volume.bytesTotal() > 0 ? static_cast<quint64>(volume.bytesTotal()) : 0;
            const quint64 free = volume.bytesAvailable() > 0 ? static_cast<quint64>(volume.bytesAvailable()) : 0;
            out += QStringLiteral("%1  ·  %2  ·  %3 free of %4\n")
                .arg(volume.rootPath(), volume.fileSystemType(), formatBytes(free), formatBytes(total));
        }
        out += QObject::tr("\nPhysical block devices\n\n");
        const QDir blocks(QStringLiteral("/sys/block"));
        for (const QString &device : blocks.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QString sectorsText = read(blocks.filePath(device + QStringLiteral("/size"))).trimmed();
            bool ok = false;
            const quint64 sectors = sectorsText.toULongLong(&ok);
            out += QStringLiteral("%1  ·  %2\n").arg(device, ok ? formatBytes(sectors * 512U) : QObject::tr("size unavailable"));
        }
        return out;
    }

    static QString bluetoothReport()
    {
        const QDir adapters(QStringLiteral("/sys/class/bluetooth"));
        const QStringList names = adapters.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        if (names.isEmpty()) return QObject::tr("No Bluetooth adapters are currently visible.\n\nConnect or enable an adapter, then refresh this page.");
        QString out = QObject::tr("Bluetooth adapters\n\n");
        for (const QString &name : names) {
            const QString address = read(adapters.filePath(name + QStringLiteral("/address"))).trimmed();
            out += QStringLiteral("%1  ·  %2\n").arg(name, address.isEmpty() ? QObject::tr("address unavailable") : address);
        }
        const QString tool = QStandardPaths::findExecutable(QStringLiteral("bluetoothctl"));
        if (!tool.isEmpty()) {
            QProcess process;
            process.start(tool, {QStringLiteral("show")});
            if (process.waitForFinished(800) && process.exitStatus() == QProcess::NormalExit) {
                out += QStringLiteral("\n") + QString::fromLocal8Bit(process.readAllStandardOutput());
            }
        }
        out += QObject::tr("\nPairing and device controls are provided by the desktop Bluetooth manager.");
        return out;
    }

    static QString startupReport(int filter)
    {
        const QString userPath = QDir::home().filePath(QStringLiteral(".config/autostart"));
        const QStringList systemPaths{QStringLiteral("/etc/xdg/autostart")};
        QString out = QObject::tr("Autostart entries (user applications may include third-party software)\n\n");
        QStringList paths;
        if (filter == 0 || filter == 1) paths << userPath;
        if (filter == 0 || filter == 2) paths << systemPaths.front();
        for (const QString &path : paths) {
            const QStringList files = QDir(path).entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name);
            for (const QString &fileName : files) {
                QFile file(QDir(path).filePath(fileName));
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                const QString data = QString::fromLocal8Bit(file.readAll());
                QString name = fileName;
                for (const QString &line : data.split(QLatin1Char('\n'))) {
                    if (line.startsWith(QStringLiteral("Name="))) { name = line.mid(5).trimmed(); break; }
                }
                const bool disabled = data.contains(QStringLiteral("Hidden=true"));
                out += QStringLiteral("%1  ·  %2  ·  %3\n").arg(name, path == userPath ? QObject::tr("User / third-party") : QObject::tr("System default"), disabled ? QObject::tr("disabled") : QObject::tr("enabled"));
            }
        }
        out += QObject::tr("\nEntries are read from standard XDG .desktop autostart locations.");
        return out;
    }

    static QString servicesReport(int filter)
    {
        if (!QFileInfo::exists(QStringLiteral("/run/systemd/system")) && filter != 2) {
            return QObject::tr("This system does not appear to use systemd. Services are managed by the active init system and are not exposed here.");
        }
        const QString executable = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
        if (executable.isEmpty()) return QObject::tr("systemctl is unavailable on this system.");
        QString out;
        const auto appendScope = [&out, &executable](bool user) {
            QProcess process;
            QStringList args;
            if (user) args << QStringLiteral("--user");
            args << QStringLiteral("list-unit-files") << QStringLiteral("--type=service")
                 << QStringLiteral("--no-legend") << QStringLiteral("--no-pager");
            process.start(executable, args);
            if (!process.waitForFinished(1200)) {
                process.kill();
                out += QObject::tr("%1 services: query timed out.\n").arg(user ? QObject::tr("User") : QObject::tr("System"));
                return;
            }
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                out += QObject::tr("%1 services: %2\n").arg(user ? QObject::tr("User") : QObject::tr("System"),
                    QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
                return;
            }
            out += QObject::tr("%1 services\n\n").arg(user ? QObject::tr("User") : QObject::tr("System"));
            out += QString::fromLocal8Bit(process.readAllStandardOutput()) + QLatin1Char('\n');
        };
        if (filter == 0 || filter == 1) appendScope(false);
        if (filter == 0 || filter == 2) appendScope(true);
        return out.isEmpty() ? QObject::tr("No service entries are available.") : out;
    }

    static QString usersReport()
    {
        QString out = QObject::tr("User accounts\n\n");
        QFile file(QStringLiteral("/etc/passwd"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QObject::tr("User account list is unavailable.");
        while (!file.atEnd()) {
            const QStringList fields = QString::fromLocal8Bit(file.readLine()).trimmed().split(QLatin1Char(':'));
            if (fields.size() < 7) continue;
            bool ok = false;
            const uint uid = fields.at(2).toUInt(&ok);
            if (!ok || (uid != 0 && uid < 1000)) continue;
            out += QStringLiteral("%1  ·  UID %2  ·  %3\n").arg(fields.at(0)).arg(uid)
                .arg(fields.at(6).isEmpty() ? QObject::tr("no login shell") : fields.at(6));
        }
        return out;
    }

    static QString detailsReport()
    {
        const QStringList pids = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QString out = QObject::tr("PID     Process\n\n");
        int count = 0;
        for (const QString &pid : pids) {
            if (pid.isEmpty() || !pid.front().isDigit()) continue;
            const QString stat = read(QStringLiteral("/proc/%1/comm").arg(pid)).trimmed();
            if (stat.isEmpty()) continue;
            out += QStringLiteral("%1  %2\n").arg(pid.rightJustified(7), stat);
            if (++count >= 1000) break;
        }
        return out;
    }

    void refreshCurrent()
    {
        const int index = tabs_->currentIndex();
        if (index == 0) performance_->setPlainText(performanceReport());
        else if (index == 1) history_->setPlainText(tr("App history is collected while Task Manager is open. Historical usage is not available yet.\n\nLinux does not keep a portable per-app history by default."));
        else if (index == 2) users_->setPlainText(usersReport());
        else if (index == 3) details_->setPlainText(detailsReport());
        else if (index == 4) network_->setPlainText(networkReport());
        else if (index == 5) disk_->setPlainText(diskReport());
        else if (index == 6) bluetooth_->setPlainText(bluetoothReport());
        else if (index == 7) startup_->setPlainText(startupReport(filters_.value(tr("Startup apps"))->currentIndex()));
        else if (index == 8) services_->setPlainText(servicesReport(filters_.value(tr("Services"))->currentIndex()));
    }

    QTabWidget *tabs_ = nullptr;
    QHash<QString, QComboBox *> filters_;
    QPlainTextEdit *performance_ = nullptr;
    QPlainTextEdit *history_ = nullptr;
    QPlainTextEdit *users_ = nullptr;
    QPlainTextEdit *details_ = nullptr;
    QPlainTextEdit *network_ = nullptr;
    QPlainTextEdit *disk_ = nullptr;
    QPlainTextEdit *bluetooth_ = nullptr;
    QPlainTextEdit *startup_ = nullptr;
    QPlainTextEdit *services_ = nullptr;
    QTimer *autoRefresh_ = nullptr;
};
