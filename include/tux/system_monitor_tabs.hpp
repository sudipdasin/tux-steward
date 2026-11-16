// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QComboBox>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QElapsedTimer>
#include <QLabel>
#include <QInputDialog>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QPushButton>
#include <QMessageBox>
#include <QSaveFile>
#include <QSizePolicy>
#include <QObject>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSet>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <unistd.h>

class ResourceHistoryChart final : public QWidget {
public:
    ResourceHistoryChart(QString title, QString unit, double fixedMaximum, QWidget *parent = nullptr)
        : QWidget(parent), title_(std::move(title)), unit_(std::move(unit)), fixedMaximum_(fixedMaximum)
    {
        setMinimumSize(260, 150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void addSample(double value)
    {
        samples_.append(qMax(0.0, value));
        if (samples_.size() > 90) samples_.remove(0, samples_.size() - 90);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRect area = rect().adjusted(1, 1, -1, -1);
        painter.setPen(palette().color(QPalette::Mid));
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawRoundedRect(area, 8, 8);

        const double current = samples_.isEmpty() ? 0.0 : samples_.back();
        painter.setPen(palette().color(QPalette::Text));
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(QRect(14, 9, width() - 28, 24), Qt::AlignLeft | Qt::AlignVCenter, title_);
        QFont valueFont = painter.font();
        valueFont.setBold(false);
        painter.setFont(valueFont);
        const QString value = QStringLiteral("%1 %2").arg(current, 0, 'f', unit_ == QStringLiteral("%") ? 0 : 1).arg(unit_);
        painter.drawText(QRect(14, 9, width() - 28, 24), Qt::AlignRight | Qt::AlignVCenter, value);

        const QRectF plot = QRectF(area).adjusted(14, 38, -14, -13);
        const QColor gridColor = palette().color(QPalette::Mid);
        painter.setPen(QPen(gridColor, 1, Qt::DotLine));
        for (int i = 0; i <= 3; ++i) {
            const qreal y = plot.top() + plot.height() * i / 3.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        if (samples_.size() < 2 || plot.width() <= 0 || plot.height() <= 0) return;
        double maximum = fixedMaximum_;
        if (maximum <= 0.0) {
            maximum = 1.0;
            for (double sample : samples_) maximum = qMax(maximum, sample);
            maximum *= 1.2;
        }
        QPainterPath line;
        for (qsizetype i = 0; i < samples_.size(); ++i) {
            const qreal x = plot.left() + plot.width() * static_cast<qreal>(i) / static_cast<qreal>(samples_.size() - 1);
            const qreal y = plot.bottom() - plot.height() * qMin(samples_.at(i), maximum) / maximum;
            if (i == 0) line.moveTo(x, y); else line.lineTo(x, y);
        }
        QPainterPath fill = line;
        fill.lineTo(plot.bottomRight());
        fill.lineTo(plot.bottomLeft());
        fill.closeSubpath();
        QColor accent = palette().color(QPalette::Highlight);
        QColor translucent = accent;
        translucent.setAlpha(42);
        painter.fillPath(fill, translucent);
        painter.setPen(QPen(accent, 2));
        painter.drawPath(line);
    }

private:
    QString title_;
    QString unit_;
    double fixedMaximum_ = 0.0;
    QVector<double> samples_;
};

// Lightweight Linux resource views for the Task Manager. Data comes directly
// from procfs/sysfs and standard Qt APIs; optional service/Bluetooth tools are
// detected at runtime, so they are not hard dependencies.
class SystemMonitorTabs final : public QObject {
public:
    explicit SystemMonitorTabs(QTabWidget *tabs) : QObject(tabs), tabs_(tabs), firstIndex_(tabs->count())
    {
        addReportTab(tr("Performance"), nullptr);
        addReportTab(tr("App history"), &history_);
        addReportTab(tr("Users"), &users_);
        addReportTab(tr("Details"), &details_);
        addReportTab(tr("Network"), &network_);
        addReportTab(tr("Disk"), &disk_);
        addReportTab(tr("Bluetooth"), nullptr);
        addReportTab(tr("Startup apps"), &startup_);
        addReportTab(tr("Services"), &services_);
        addReportTab(tr("GPU"), &gpu_);

        autoRefresh_ = new QTimer(tabs_);
        autoRefresh_->setInterval(2000);
        connect(autoRefresh_, &QTimer::timeout, tabs_, [this]() { refreshCurrent(); });
        connect(tabs_, &QTabWidget::currentChanged, tabs_, [this]() { refreshCurrent(); });
        refreshCurrent();
    }

    ~SystemMonitorTabs() override
    {
        if (bluetoothClient_ != nullptr && bluetoothClient_->state() != QProcess::NotRunning) {
            bluetoothClient_->write("scan off\nquit\n");
            bluetoothClient_->waitForBytesWritten(250);
            bluetoothClient_->terminate();
            if (!bluetoothClient_->waitForFinished(500)) bluetoothClient_->kill();
        }
    }

private:
    void addReportTab(const QString &name, QPlainTextEdit **view)
    {
        QWidget *page = new QWidget(tabs_);
        auto *layout = new QVBoxLayout(page);
        if (name == tr("Performance")) {
            performanceSummary_ = new QLabel(page);
            performanceSummary_->setWordWrap(true);
            layout->addWidget(performanceSummary_);
            auto *grid = new QGridLayout;
            cpuChart_ = new ResourceHistoryChart(tr("CPU"), QStringLiteral("%"), 100.0, page);
            memoryChart_ = new ResourceHistoryChart(tr("Memory"), QStringLiteral("%"), 100.0, page);
            diskChart_ = new ResourceHistoryChart(tr("Disk I/O"), tr("MiB/s"), 0.0, page);
            networkChart_ = new ResourceHistoryChart(tr("Network I/O"), tr("Mbit/s"), 0.0, page);
            cpuChart_->setObjectName(QStringLiteral("cpuUsageChart"));
            memoryChart_->setObjectName(QStringLiteral("memoryUsageChart"));
            diskChart_->setObjectName(QStringLiteral("diskIoChart"));
            networkChart_->setObjectName(QStringLiteral("networkIoChart"));
            grid->addWidget(cpuChart_, 0, 0);
            grid->addWidget(memoryChart_, 0, 1);
            grid->addWidget(diskChart_, 1, 0);
            grid->addWidget(networkChart_, 1, 1);
            layout->addLayout(grid, 1);
            tabs_->addTab(page, name);
            return;
        }
        if (name == tr("Bluetooth")) {
            bluetoothStatus_ = new QLabel(page);
            bluetoothStatus_->setWordWrap(true);
            layout->addWidget(bluetoothStatus_);
            auto *deviceRow = new QHBoxLayout;
            bluetoothDevices_ = new QComboBox(page);
            bluetoothDevices_->setObjectName(QStringLiteral("bluetoothDeviceSelect"));
            bluetoothDevices_->setMinimumWidth(240);
            deviceRow->addWidget(bluetoothDevices_, 1);
            auto *scan = new QPushButton(tr("Scan"), page);
            scan->setObjectName(QStringLiteral("bluetoothScanButton"));
            auto *pair = new QPushButton(tr("Pair"), page);
            pair->setObjectName(QStringLiteral("bluetoothPairButton"));
            deviceRow->addWidget(scan);
            deviceRow->addWidget(pair);
            layout->addLayout(deviceRow);
            auto *controlRow = new QHBoxLayout;
            const auto addBluetoothAction = [this, page, controlRow](const QString &label, const QString &action) {
                auto *button = new QPushButton(label, page);
                button->setObjectName(QStringLiteral("bluetoothAction_%1").arg(action));
                controlRow->addWidget(button);
                connect(button, &QPushButton::clicked, tabs_, [this, action]() { runBluetoothAction(action); });
            };
            addBluetoothAction(tr("Connect"), QStringLiteral("connect"));
            addBluetoothAction(tr("Disconnect"), QStringLiteral("disconnect"));
            addBluetoothAction(tr("Trust"), QStringLiteral("trust"));
            addBluetoothAction(tr("Forget"), QStringLiteral("remove"));
            auto *settings = new QPushButton(tr("Open Bluetooth settings…"), page);
            settings->setObjectName(QStringLiteral("bluetoothSettingsButton"));
            controlRow->addWidget(settings);
            layout->addLayout(controlRow);
            bluetoothLog_ = new QPlainTextEdit(page);
            bluetoothLog_->setReadOnly(true);
            bluetoothLog_->setMaximumBlockCount(300);
            bluetoothLog_->setPlaceholderText(tr("Bluetooth events and pairing prompts will appear here."));
            layout->addWidget(bluetoothLog_, 1);
            bluetoothScanTimer_ = new QTimer(this);
            bluetoothScanTimer_->setSingleShot(true);
            connect(scan, &QPushButton::clicked, tabs_, [this]() {
                ensureBluetoothClient();
                sendBluetoothCommand(QStringLiteral("scan on"));
                if (bluetoothScanTimer_ != nullptr) bluetoothScanTimer_->start(15000);
                if (bluetoothStatus_ != nullptr) bluetoothStatus_->setText(tr("Scanning for nearby devices for 15 seconds…"));
            });
            connect(bluetoothScanTimer_, &QTimer::timeout, tabs_, [this]() {
                sendBluetoothCommand(QStringLiteral("scan off"));
                sendBluetoothCommand(QStringLiteral("devices"));
                if (bluetoothStatus_ != nullptr) bluetoothStatus_->setText(bluetoothReport());
            });
            connect(pair, &QPushButton::clicked, tabs_, [this]() { runBluetoothAction(QStringLiteral("pair")); });
            connect(settings, &QPushButton::clicked, tabs_, [this, page]() { openBluetoothSettings(page); });
            tabs_->addTab(page, name);
            return;
        }
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
        if (name == tr("GPU")) text->setObjectName(QStringLiteral("gpuSummary"));
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
                else QMessageBox::information(page, QObject::tr("Bluetooth settings unavailable"),
                    QObject::tr("Install or open the Bluetooth settings app provided by your desktop environment."));
            });
        }
    }

    static QString read(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        return QString::fromLocal8Bit(file.readAll());
    }

    void refreshPerformance()
    {
        double cpuPercent = 0.0;
        const QStringList cpuFields = read(QStringLiteral("/proc/stat")).section(QLatin1Char('\n'), 0, 0)
            .simplified().split(QLatin1Char(' '));
        quint64 totalTicks = 0;
        for (qsizetype i = 1; i < cpuFields.size(); ++i) totalTicks += cpuFields.at(i).toULongLong();
        const quint64 idleTicks = cpuFields.value(4).toULongLong() + cpuFields.value(5).toULongLong();

        quint64 totalKiB = 0;
        quint64 availableKiB = 0;
        const QStringList memLines = read(QStringLiteral("/proc/meminfo")).split(QLatin1Char('\n'));
        for (const QString &line : memLines) {
            if (line.startsWith(QStringLiteral("MemTotal:"))) totalKiB = line.simplified().split(QLatin1Char(' ')).value(1).toULongLong();
            if (line.startsWith(QStringLiteral("MemAvailable:"))) availableKiB = line.simplified().split(QLatin1Char(' ')).value(1).toULongLong();
        }
        const quint64 usedKiB = totalKiB - qMin(totalKiB, availableKiB);
        const double memoryPercent = totalKiB > 0
            ? 100.0 * static_cast<double>(usedKiB) / static_cast<double>(totalKiB) : 0.0;

        QSet<QString> blockDevices;
        const QDir blockDirectory(QStringLiteral("/sys/block"));
        for (const QString &name : blockDirectory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) blockDevices.insert(name);
        quint64 diskSectorsRead = 0;
        quint64 diskSectorsWritten = 0;
        const QStringList diskLines = read(QStringLiteral("/proc/diskstats")).split(QLatin1Char('\n'));
        for (const QString &line : diskLines) {
            const QStringList fields = line.simplified().split(QLatin1Char(' '));
            if (fields.size() < 10 || !blockDevices.contains(fields.at(2))) continue;
            diskSectorsRead += fields.at(5).toULongLong();
            diskSectorsWritten += fields.at(9).toULongLong();
        }

        quint64 networkReceived = 0;
        quint64 networkSent = 0;
        const QStringList networkLines = read(QStringLiteral("/proc/net/dev")).split(QLatin1Char('\n'));
        for (qsizetype i = 2; i < networkLines.size(); ++i) {
            const qsizetype colon = networkLines.at(i).indexOf(QLatin1Char(':'));
            if (colon < 0) continue;
            const QStringList fields = networkLines.at(i).mid(colon + 1).simplified().split(QLatin1Char(' '));
            if (fields.size() < 9) continue;
            networkReceived += fields.at(0).toULongLong();
            networkSent += fields.at(8).toULongLong();
        }

        double diskMiBPerSecond = 0.0;
        double networkMbitPerSecond = 0.0;
        double elapsedSeconds = 0.0;
        if (performanceClock_.isValid()) {
            elapsedSeconds = static_cast<double>(performanceClock_.nsecsElapsed()) / 1.0e9;
            if (elapsedSeconds > 0.0) {
                if (previousTotalTicks_ > 0 && totalTicks > previousTotalTicks_) {
                    const quint64 deltaTotal = totalTicks - previousTotalTicks_;
                    const quint64 deltaIdle = idleTicks >= previousIdleTicks_ ? idleTicks - previousIdleTicks_ : 0;
                    cpuPercent = 100.0 * static_cast<double>(deltaTotal - qMin(deltaTotal, deltaIdle))
                                 / static_cast<double>(deltaTotal);
                }
                const quint64 diskSectors = (diskSectorsRead - qMin(diskSectorsRead, previousDiskSectorsRead_))
                    + (diskSectorsWritten - qMin(diskSectorsWritten, previousDiskSectorsWritten_));
                diskMiBPerSecond = static_cast<double>(diskSectors) * 512.0 / 1048576.0 / elapsedSeconds;
                const quint64 received = networkReceived - qMin(networkReceived, previousNetworkReceived_);
                const quint64 sent = networkSent - qMin(networkSent, previousNetworkSent_);
                networkMbitPerSecond = static_cast<double>(received + sent) * 8.0 / 1.0e6 / elapsedSeconds;
            }
        }
        previousTotalTicks_ = totalTicks;
        previousIdleTicks_ = idleTicks;
        previousDiskSectorsRead_ = diskSectorsRead;
        previousDiskSectorsWritten_ = diskSectorsWritten;
        previousNetworkReceived_ = networkReceived;
        previousNetworkSent_ = networkSent;
        performanceClock_.restart();

        cpuChart_->addSample(cpuPercent);
        memoryChart_->addSample(memoryPercent);
        diskChart_->addSample(diskMiBPerSecond);
        networkChart_->addSample(networkMbitPerSecond);
        const QString load = read(QStringLiteral("/proc/loadavg")).trimmed().section(QLatin1Char(' '), 0, 2);
        const QString memoryText = totalKiB > 0
            ? tr("%1 GiB of %2 GiB memory used (%3%)")
                .arg(static_cast<double>(usedKiB) / 1048576.0, 0, 'f', 2)
                .arg(static_cast<double>(totalKiB) / 1048576.0, 0, 'f', 2)
                .arg(memoryPercent, 0, 'f', 1)
            : tr("Memory data unavailable");
        performanceSummary_->setText(tr("CPU %1%  ·  %2  ·  Disk %3 MiB/s  ·  Network %4 Mbit/s  ·  Load %5")
            .arg(cpuPercent, 0, 'f', 1).arg(memoryText)
            .arg(diskMiBPerSecond, 0, 'f', 2).arg(networkMbitPerSecond, 0, 'f', 2)
            .arg(load.isEmpty() ? tr("unavailable") : load));
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
        out += QObject::tr("\nUse Scan to find devices, then Pair or Connect. Pairing confirmation requests appear below.");
        return out;
    }

    static QString gpuReport()
    {
        const QDir drm(QStringLiteral("/sys/class/drm"));
        const QRegularExpression cardName(QStringLiteral("^card[0-9]+$"));
        const QStringList entries = drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QString out = QObject::tr("Graphics devices (Linux DRM / sysfs)\n\n");
        int found = 0;
        for (const QString &entry : entries) {
            if (!cardName.match(entry).hasMatch()) continue;
            const QString devicePath = drm.filePath(entry + QStringLiteral("/device"));
            const QString vendorId = read(devicePath + QStringLiteral("/vendor")).trimmed().toLower();
            const QString deviceId = read(devicePath + QStringLiteral("/device")).trimmed().toLower();
            QString vendor = vendorId;
            if (vendorId == QStringLiteral("0x10de")) vendor = QObject::tr("NVIDIA");
            else if (vendorId == QStringLiteral("0x1002")) vendor = QObject::tr("AMD");
            else if (vendorId == QStringLiteral("0x8086")) vendor = QObject::tr("Intel");
            else if (vendorId == QStringLiteral("0x1af4")) vendor = QObject::tr("Virtio");
            out += QObject::tr("%1 · %2 · device %3\n").arg(entry, vendor,
                deviceId.isEmpty() ? QObject::tr("unavailable") : deviceId);

            const QString busy = read(devicePath + QStringLiteral("/gpu_busy_percent")).trimmed();
            bool busyOk = false;
            const int busyPercent = busy.toInt(&busyOk);
            if (busyOk && busyPercent >= 0 && busyPercent <= 100)
                out += QObject::tr("  GPU utilization: %1%\n").arg(busyPercent);

            bool totalOk = false;
            bool usedOk = false;
            const quint64 total = read(devicePath + QStringLiteral("/mem_info_vram_total")).trimmed().toULongLong(&totalOk);
            const quint64 used = read(devicePath + QStringLiteral("/mem_info_vram_used")).trimmed().toULongLong(&usedOk);
            if (totalOk && usedOk)
                out += QObject::tr("  Video memory: %1 used of %2\n").arg(formatBytes(used), formatBytes(total));
            if (!busyOk && !(totalOk && usedOk))
                out += QObject::tr("  Utilization and dedicated video-memory counters are not exposed by this driver.\n");
            ++found;
        }
        if (found == 0)
            out += QObject::tr("No graphics devices are exposed through /sys/class/drm. Some drivers require vendor tools for detailed metrics.");
        else
            out += QObject::tr("\nMetrics depend on the active kernel driver; unavailable values are omitted rather than estimated.");
        return out;
    }

    void openBluetoothSettings(QWidget *parent)
    {
        const QString blueman = QStandardPaths::findExecutable(QStringLiteral("blueman-manager"));
        const QString gnome = QStandardPaths::findExecutable(QStringLiteral("gnome-control-center"));
        const QString xfce = QStandardPaths::findExecutable(QStringLiteral("xfce4-settings-manager"));
        if (!blueman.isEmpty()) QProcess::startDetached(blueman, {});
        else if (!gnome.isEmpty()) QProcess::startDetached(gnome, {QStringLiteral("bluetooth")});
        else if (!xfce.isEmpty()) QProcess::startDetached(xfce, {});
        else QMessageBox::information(parent, tr("Bluetooth settings unavailable"),
            tr("Install or open the Bluetooth settings app provided by your desktop environment."));
    }

    void ensureBluetoothClient()
    {
        if (bluetoothClient_ != nullptr && bluetoothClient_->state() != QProcess::NotRunning) return;
        const QString executable = QStandardPaths::findExecutable(QStringLiteral("bluetoothctl"));
        if (executable.isEmpty()) {
            if (bluetoothStatus_ != nullptr)
                bluetoothStatus_->setText(tr("bluetoothctl is unavailable. Use desktop Bluetooth settings to pair devices."));
            return;
        }
        bluetoothClient_ = new QProcess(this);
        bluetoothClient_->setProcessChannelMode(QProcess::SeparateChannels);
        connect(bluetoothClient_, &QProcess::started, this, [this]() {
            sendBluetoothCommand(QStringLiteral("agent on"));
            sendBluetoothCommand(QStringLiteral("default-agent"));
            sendBluetoothCommand(QStringLiteral("show"));
            sendBluetoothCommand(QStringLiteral("devices"));
            sendBluetoothCommand(QStringLiteral("paired-devices"));
        });
        connect(bluetoothClient_, &QProcess::readyReadStandardOutput, this, [this]() {
            handleBluetoothOutput(QString::fromLocal8Bit(bluetoothClient_->readAllStandardOutput()));
        });
        connect(bluetoothClient_, &QProcess::readyReadStandardError, this, [this]() {
            const QString error = QString::fromLocal8Bit(bluetoothClient_->readAllStandardError()).trimmed();
            if (!error.isEmpty() && bluetoothLog_ != nullptr) bluetoothLog_->appendPlainText(error);
        });
        connect(bluetoothClient_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int, QProcess::ExitStatus) {
            if (bluetoothStatus_ != nullptr && QStandardPaths::findExecutable(QStringLiteral("bluetoothctl")).isEmpty())
                bluetoothStatus_->setText(tr("Bluetooth control client exited."));
        });
        bluetoothClient_->start(executable, QStringList{});
    }

    void sendBluetoothCommand(const QString &command)
    {
        ensureBluetoothClient();
        if (bluetoothClient_ == nullptr || bluetoothClient_->state() == QProcess::NotRunning) return;
        bluetoothClient_->write(command.toLocal8Bit() + '\n');
        if (bluetoothLog_ != nullptr) bluetoothLog_->appendPlainText(QStringLiteral("> %1").arg(command));
    }

    QString selectedBluetoothAddress() const
    {
        if (bluetoothDevices_ == nullptr) return {};
        const QString address = bluetoothDevices_->currentData().toString();
        static const QRegularExpression validAddress(QStringLiteral("^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$"));
        return validAddress.match(address).hasMatch() ? address : QString();
    }

    void runBluetoothAction(const QString &action)
    {
        const QString address = selectedBluetoothAddress();
        if (address.isEmpty()) {
            if (bluetoothStatus_ != nullptr) bluetoothStatus_->setText(tr("Select a Bluetooth device first."));
            return;
        }
        if (action == QStringLiteral("pair")) {
            const QString name = bluetoothDevices_->currentText();
            const auto answer = QMessageBox::question(tabs_, tr("Pair Bluetooth device"),
                tr("Pair with %1 (%2)? Make sure this is the device you intend to use.").arg(name, address),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) return;
        } else if (action == QStringLiteral("remove")) {
            const auto answer = QMessageBox::warning(tabs_, tr("Forget Bluetooth device"),
                tr("Remove the pairing for %1? You will need to pair it again to reconnect.").arg(address),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) return;
        }
        sendBluetoothCommand(action + QLatin1Char(' ') + address);
    }

    void handleBluetoothOutput(const QString &output)
    {
        const QStringList lines = output.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
        static const QRegularExpression deviceLine(QStringLiteral("(?:\\[[A-Z]+\\]\\s*)?Device\\s+([0-9A-Fa-f:]{17})\\s+(.+)"));
        for (const QString &line : lines) {
            if (bluetoothLog_ != nullptr) bluetoothLog_->appendPlainText(line.trimmed());
            const auto match = deviceLine.match(line.trimmed());
            if (match.hasMatch() && bluetoothDevices_ != nullptr) {
                const QString address = match.captured(1);
                const QString name = match.captured(2).trimmed();
                const int existing = bluetoothDevices_->findData(address);
                if (existing < 0) bluetoothDevices_->addItem(QStringLiteral("%1  ·  %2").arg(name, address), address);
                else bluetoothDevices_->setItemText(existing, QStringLiteral("%1  ·  %2").arg(name, address));
            }
            if (line.contains(QStringLiteral("Confirm passkey"), Qt::CaseInsensitive)
                || line.contains(QStringLiteral("Authorize service"), Qt::CaseInsensitive)
                || line.contains(QStringLiteral("confirm value"), Qt::CaseInsensitive)) {
                const auto answer = QMessageBox::question(tabs_, tr("Confirm Bluetooth pairing"), line.trimmed(),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                sendBluetoothCommand(answer == QMessageBox::Yes ? QStringLiteral("yes") : QStringLiteral("no"));
            } else if (line.contains(QStringLiteral("Enter PIN code"), Qt::CaseInsensitive)
                       || line.contains(QStringLiteral("Enter passkey"), Qt::CaseInsensitive)) {
                bool accepted = false;
                const QString pin = QInputDialog::getText(tabs_, tr("Bluetooth PIN"), line.trimmed(),
                    QLineEdit::Normal, {}, &accepted);
                sendBluetoothCommand(accepted ? pin : QStringLiteral("no"));
            }
        }
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

    static QString appHistoryReport()
    {
        // Keep an in-memory, current-session history only. It is never written
        // to disk, and process identity includes start time to avoid PID reuse.
        static QHash<QString, quint64> previousTicks;
        static QHash<QString, quint64> usageByName;
        QHash<QString, quint64> currentTicks;
        QHash<QString, QString> namesByIdentity;
        const QStringList pids = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &pid : pids) {
            if (pid.isEmpty() || !pid.front().isDigit()) continue;
            const QString line = read(QStringLiteral("/proc/%1/stat").arg(pid)).trimmed();
            const qsizetype openParen = line.indexOf(QLatin1Char('('));
            const qsizetype closeParen = line.lastIndexOf(QLatin1Char(')'));
            if (openParen < 0 || closeParen <= openParen) continue;
            const QStringList fields = line.mid(closeParen + 2).simplified().split(QLatin1Char(' '));
            if (fields.size() < 20) continue;
            bool ticksOk = false;
            const quint64 ticks = fields.at(11).toULongLong(&ticksOk) + fields.at(12).toULongLong();
            const quint64 start = fields.at(19).toULongLong();
            if (!ticksOk) continue;
            const QString identity = pid + QLatin1Char(':') + QString::number(start);
            currentTicks.insert(identity, ticks);
            namesByIdentity.insert(identity, line.mid(openParen + 1, closeParen - openParen - 1));
            if (previousTicks.contains(identity) && ticks >= previousTicks.value(identity))
                usageByName[namesByIdentity.value(identity)] += ticks - previousTicks.value(identity);
        }
        previousTicks = std::move(currentTicks);
        QList<QPair<QString, quint64>> rows;
        for (auto it = usageByName.cbegin(); it != usageByName.cend(); ++it) rows.append({it.key(), it.value()});
        std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
        const long clockTicks = ::sysconf(_SC_CLK_TCK);
        QString out = QObject::tr("CPU time sampled during this Task Manager session (held in memory only)\n\n");
        int count = 0;
        for (const auto &row : rows) {
            const double seconds = static_cast<double>(row.second) / static_cast<double>(clockTicks > 0 ? clockTicks : 100);
            out += QStringLiteral("%1  ·  %2 s\n").arg(row.first, QString::number(seconds, 'f', 1));
            if (++count >= 50) break;
        }
        if (rows.isEmpty()) out += QObject::tr("Collecting the first sample. Open this tab for a few seconds to build session history.");
        return out;
    }

    void refreshCurrent()
    {
        const int index = tabs_->currentIndex() - firstIndex_;
        if (index == 0) refreshPerformance();
        else if (index == 1) history_->setPlainText(appHistoryReport());
        else if (index == 2) users_->setPlainText(usersReport());
        else if (index == 3) details_->setPlainText(detailsReport());
        else if (index == 4) network_->setPlainText(networkReport());
        else if (index == 5) disk_->setPlainText(diskReport());
        else if (index == 6) {
            ensureBluetoothClient();
            if (bluetoothStatus_ != nullptr && QStandardPaths::findExecutable(QStringLiteral("bluetoothctl")).isEmpty())
                bluetoothStatus_->setText(bluetoothReport() + tr("\n\nInstall BlueZ tools or use desktop Bluetooth settings to manage devices."));
        }
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
        } else if (index == 9) gpu_->setPlainText(gpuReport());
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
    QLabel *performanceSummary_ = nullptr;
    ResourceHistoryChart *cpuChart_ = nullptr;
    ResourceHistoryChart *memoryChart_ = nullptr;
    ResourceHistoryChart *diskChart_ = nullptr;
    ResourceHistoryChart *networkChart_ = nullptr;
    QElapsedTimer performanceClock_;
    quint64 previousTotalTicks_ = 0;
    quint64 previousIdleTicks_ = 0;
    quint64 previousDiskSectorsRead_ = 0;
    quint64 previousDiskSectorsWritten_ = 0;
    quint64 previousNetworkReceived_ = 0;
    quint64 previousNetworkSent_ = 0;
    QPlainTextEdit *history_ = nullptr;
    QPlainTextEdit *users_ = nullptr;
    QPlainTextEdit *details_ = nullptr;
    QPlainTextEdit *network_ = nullptr;
    QPlainTextEdit *disk_ = nullptr;
    QPlainTextEdit *bluetooth_ = nullptr;
    QComboBox *bluetoothDevices_ = nullptr;
    QLabel *bluetoothStatus_ = nullptr;
    QPlainTextEdit *bluetoothLog_ = nullptr;
    QProcess *bluetoothClient_ = nullptr;
    QTimer *bluetoothScanTimer_ = nullptr;
    QPlainTextEdit *startup_ = nullptr;
    QPlainTextEdit *services_ = nullptr;
    QPlainTextEdit *gpu_ = nullptr;
    QTimer *autoRefresh_ = nullptr;
    QElapsedTimer serviceRefresh_;
};
