// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QElapsedTimer>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QMessageBox>
#include <QSaveFile>
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
    explicit SystemMonitorTabs(QTabWidget *tabs) : QObject(tabs), tabs_(tabs), firstIndex_(tabs->count())
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
            filter->setCurrentIndex(1);
        }
        if (filter != nullptr) {
            filter->setMaximumWidth(260);
            layout->addWidget(filter, 0, Qt::AlignLeft);
            filters_.insert(name, filter);
            connect(filter, qOverload<int>(&QComboBox::currentIndexChanged), tabs_, [this, name]() {
                if (name == tr("Services")) serviceRefresh_.invalidate();
                refreshCurrent();
            });
        }
        if (name == tr("Startup apps")) {
            startupSelection_ = new QComboBox(page);
            startupSelection_->setMinimumWidth(250);
            auto *row = new QHBoxLayout;
            row->addWidget(startupSelection_, 1);
            auto *enable = new QPushButton(tr("Enable at sign-in"), page);
            auto *disable = new QPushButton(tr("Disable at sign-in"), page);
            row->addWidget(enable);
            row->addWidget(disable);
            layout->addLayout(row);
            connect(enable, &QPushButton::clicked, tabs_, [this]() { setStartupEnabled(true); });
            connect(disable, &QPushButton::clicked, tabs_, [this]() { setStartupEnabled(false); });
        }
        if (name == tr("Services")) {
            serviceSelection_ = new QComboBox(page);
            serviceSelection_->setMinimumWidth(250);
            auto *row = new QHBoxLayout;
            row->addWidget(serviceSelection_, 1);
            const auto addAction = [this, page, row](const QString &label, const QString &action) {
                auto *button = new QPushButton(label, page);
                row->addWidget(button);
                connect(button, &QPushButton::clicked, tabs_, [this, action]() { changeService(action); });
            };
            addAction(tr("Start"), QStringLiteral("start"));
            addAction(tr("Stop"), QStringLiteral("stop"));
            addAction(tr("Enable"), QStringLiteral("enable"));
            addAction(tr("Disable"), QStringLiteral("disable"));
            layout->addLayout(row);
        }
        auto *text = new QPlainTextEdit(page);
        text->setReadOnly(true);
        text->setLineWrapMode(QPlainTextEdit::NoWrap);
        text->setPlaceholderText(tr("Reading system information…"));
        *view = text;
        layout->addWidget(text, 1);
        if (name == tr("Startup apps") || name == tr("Services")) {
            auto *actionStatus = new QLabel(page);
            actionStatus->setWordWrap(true);
            layout->addWidget(actionStatus);
            actionStatusByTab_.insert(name, actionStatus);
        }
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

    void refreshStartupChoices(int filter)
    {
        if (startupSelection_ == nullptr) return;
        const QString selected = startupSelection_->currentData().toString();
        startupSelection_->clear();
        const QString userPath = QDir::home().filePath(QStringLiteral(".config/autostart"));
        const QString systemPath = QStringLiteral("/etc/xdg/autostart");
        QHash<QString, QString> effective;
        if (filter == 0 || filter == 2) {
            for (const QString &file : QDir(systemPath).entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name))
                effective.insert(file, QDir(systemPath).filePath(file));
        }
        if (filter == 0 || filter == 1) {
            for (const QString &file : QDir(userPath).entryList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name))
                effective.insert(file, QDir(userPath).filePath(file));
        }
        QStringList files = effective.keys();
        std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
        for (const QString &fileName : files) {
            const QString path = effective.value(fileName);
            const QString contents = read(path);
            QString name = fileName;
            for (const QString &line : contents.split(QLatin1Char('\n'))) {
                if (line.startsWith(QStringLiteral("Name="))) { name = line.mid(5).trimmed(); break; }
            }
            const bool disabled = contents.contains(QStringLiteral("Hidden=true"));
            const bool userEntry = path.startsWith(userPath + QLatin1Char('/'));
            startupSelection_->addItem(QStringLiteral("%1  ·  %2  ·  %3").arg(name,
                userEntry ? tr("User / third-party") : tr("System"), disabled ? tr("disabled") : tr("enabled")), path);
        }
        const int restore = startupSelection_->findData(selected);
        if (restore >= 0) startupSelection_->setCurrentIndex(restore);
    }

    void setStartupEnabled(bool enabled)
    {
        if (startupSelection_ == nullptr || startupSelection_->currentIndex() < 0) return;
        const QString sourcePath = startupSelection_->currentData().toString();
        const QFileInfo sourceInfo(sourcePath);
        const QString targetPath = sourcePath.startsWith(QDir::home().filePath(QStringLiteral(".config/autostart")) + QLatin1Char('/'))
            ? sourcePath : QDir::home().filePath(QStringLiteral(".config/autostart/") + sourceInfo.fileName());
        QString contents = read(sourcePath);
        if (contents.isEmpty()) {
            statusMessage_ = tr("Could not read this startup entry.");
            refreshCurrent();
            return;
        }
        QStringList lines = contents.split(QLatin1Char('\n'));
        bool found = false;
        for (QString &line : lines) {
            if (line.startsWith(QStringLiteral("Hidden="))) {
                line = QStringLiteral("Hidden=") + (enabled ? QStringLiteral("false") : QStringLiteral("true"));
                found = true;
            }
        }
        if (!found) {
            const int group = lines.indexOf(QStringLiteral("[Desktop Entry]"));
            if (group >= 0) lines.insert(group + 1, QStringLiteral("Hidden=") + (enabled ? QStringLiteral("false") : QStringLiteral("true")));
            else lines << QStringLiteral("[Desktop Entry]") << (QStringLiteral("Hidden=") + (enabled ? QStringLiteral("false") : QStringLiteral("true")));
        }
        QDir().mkpath(QFileInfo(targetPath).absolutePath());
        QSaveFile file(targetPath);
        const QByteArray bytes = lines.join(QLatin1Char('\n')).toLocal8Bit();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            statusMessage_ = tr("Could not update this startup entry. Check folder permissions.");
        else statusMessage_ = enabled ? tr("The app will start when you sign in.") : tr("The app is disabled at sign-in for this user.");
        if (actionStatusByTab_.value(tr("Startup apps")) != nullptr)
            actionStatusByTab_.value(tr("Startup apps"))->setText(statusMessage_);
        refreshCurrent();
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
        const int index = tabs_->currentIndex() - firstIndex_;
        if (index == 0) performance_->setPlainText(performanceReport());
        else if (index == 1) history_->setPlainText(tr("Per-app usage history is not available yet. Linux does not provide a portable historical usage database; this tab will report usage collected during this manager session in a future update."));
        else if (index == 2) users_->setPlainText(usersReport());
        else if (index == 3) details_->setPlainText(detailsReport());
        else if (index == 4) network_->setPlainText(networkReport());
        else if (index == 5) disk_->setPlainText(diskReport());
        else if (index == 6) bluetooth_->setPlainText(bluetoothReport());
        else if (index == 7) {
            const int filter = filters_.value(tr("Startup apps"))->currentIndex();
            refreshStartupChoices(filter);
            startup_->setPlainText(startupReport(filter));
        } else if (index == 8) {
            if (!serviceRefresh_.isValid() || serviceRefresh_.elapsed() >= 10000) {
                const int filter = filters_.value(tr("Services"))->currentIndex();
                refreshServiceChoices(filter);
                services_->setPlainText(servicesReport(filter));
                serviceRefresh_.restart();
            }
        }
        if (index == 7 && actionStatusByTab_.value(tr("Startup apps")) != nullptr)
            actionStatusByTab_.value(tr("Startup apps"))->setText(statusMessage_);
        if (index == 8 && actionStatusByTab_.value(tr("Services")) != nullptr)
            actionStatusByTab_.value(tr("Services"))->setText(statusMessage_);
    }

    void refreshServiceChoices(int filter)
    {
        if (serviceSelection_ == nullptr) return;
        const QString selected = serviceSelection_->currentData().toMap().value(QStringLiteral("unit")).toString();
        serviceSelection_->clear();
        const QString systemctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
        if (systemctl.isEmpty()) return;
        for (bool user : {false, true}) {
            if ((filter == 1 && user) || (filter == 2 && !user)) continue;
            QProcess process;
            QStringList args;
            if (user) args << QStringLiteral("--user");
            args << QStringLiteral("list-unit-files") << QStringLiteral("--type=service")
                 << QStringLiteral("--no-legend") << QStringLiteral("--no-pager");
            process.start(systemctl, args);
            if (!process.waitForFinished(700) || process.exitCode() != 0) { process.kill(); continue; }
            const QStringList lines = QString::fromLocal8Bit(process.readAllStandardOutput()).split(QLatin1Char('\n'));
            for (const QString &line : lines) {
                const QString unit = line.simplified().section(QLatin1Char(' '), 0, 0);
                if (!unit.endsWith(QStringLiteral(".service"))) continue;
                QVariantMap data;
                data.insert(QStringLiteral("unit"), unit);
                data.insert(QStringLiteral("user"), user);
                serviceSelection_->addItem(filter == 0
                    ? QStringLiteral("%1  ·  %2").arg(unit, user ? tr("User") : tr("System")) : unit, data);
            }
        }
        const int restore = serviceSelection_->findData(selected);
        if (restore >= 0) serviceSelection_->setCurrentIndex(restore);
    }

    void changeService(const QString &action)
    {
        if (serviceSelection_ == nullptr || serviceSelection_->currentIndex() < 0) return;
        const QVariantMap data = serviceSelection_->currentData().toMap();
        const QString unit = data.value(QStringLiteral("unit")).toString();
        const bool user = data.value(QStringLiteral("user")).toBool();
        if (unit.isEmpty()) return;
        const QString scope = user ? tr("your user session") : tr("the whole system");
        const auto answer = QMessageBox::warning(tabs_, tr("Change service"),
            tr("Run ‘%1’ for %2 in %3? Changing system services can affect networking, login, or other apps.")
                .arg(action, unit, scope), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) return;

        const QString systemctl = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
        QString program = systemctl;
        QStringList args;
        if (user) args << QStringLiteral("--user") << action << unit;
        else {
            const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
            if (pkexec.isEmpty()) {
                statusMessage_ = tr("Administrator authorization (pkexec) is unavailable.");
                if (actionStatusByTab_.value(tr("Services")) != nullptr)
                    actionStatusByTab_.value(tr("Services"))->setText(statusMessage_);
                refreshCurrent();
                return;
            }
            program = pkexec;
            args << systemctl << action << unit;
        }
        if (program.isEmpty()) return;
        auto *process = new QProcess(this);
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this, process, action, unit](int code, QProcess::ExitStatus status) {
            if (status == QProcess::NormalExit && code == 0)
                statusMessage_ = tr("‘%1’ completed for %2.").arg(action, unit);
            else
                statusMessage_ = tr("Could not %1 %2: %3").arg(action, unit, QString::fromLocal8Bit(process->readAllStandardError()).trimmed());
            process->deleteLater();
            serviceRefresh_.invalidate();
            refreshCurrent();
        });
        statusMessage_ = tr("Waiting for service manager…");
        if (actionStatusByTab_.value(tr("Services")) != nullptr)
            actionStatusByTab_.value(tr("Services"))->setText(statusMessage_);
        process->start(program, args);
    }

    QTabWidget *tabs_ = nullptr;
    int firstIndex_ = 0;
    QHash<QString, QComboBox *> filters_;
    QHash<QString, QLabel *> actionStatusByTab_;
    QComboBox *startupSelection_ = nullptr;
    QComboBox *serviceSelection_ = nullptr;
    QString statusMessage_;
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
    QElapsedTimer serviceRefresh_;
};
