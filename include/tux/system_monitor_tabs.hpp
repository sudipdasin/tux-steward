// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/autostart.hpp"
#include "tux/resource_detail.hpp"
#include "tux/trusted_command.hpp"
#include <QTableWidget>
#include <QListWidget>
#include <QTabBar>
#include <QHeaderView>
#include <QSignalBlocker>
#include "tux/async_command.hpp"
#include <QPointer>
#include <QMap>

#include <QComboBox>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QElapsedTimer>
#include <QEvent>
#include <QCloseEvent>
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
#include <memory>
#include <unistd.h>

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
        addReportTab(tr("Startup apps"), nullptr);
        addReportTab(tr("Services"), &services_);
        addReportTab(tr("GPU"), &gpu_);

        // Keep page identities stable when grouping unfinished workflows.
        for (int i=0;i<10;++i) resourcePages_.append(tabs_->widget(firstIndex_+i));
        experimentalTabs_ = new QTabWidget(tabs_);
        experimentalTabs_->setObjectName(QStringLiteral("experimentalResourceTabs"));
        for (int i : {6,9}) {
            QWidget *page=resourcePages_[i];
            const int oldIndex=tabs_->indexOf(page);
            const QString title=tabs_->tabText(oldIndex);
            tabs_->removeTab(oldIndex);
            auto *notice=new QLabel(tr("Experimental beta feature — desktop/hardware validation is incomplete. Review changes carefully and report problems."),page);
            notice->setWordWrap(true);
            static_cast<QVBoxLayout *>(page->layout())->insertWidget(0,notice);
            experimentalTabs_->addTab(page,title);
        }
        tabs_->addTab(experimentalTabs_,tr("Experimental"));
        autoRefresh_ = new QTimer(tabs_);
        autoRefresh_->setObjectName(QStringLiteral("taskManagerResourceRefreshTimer"));
        autoRefresh_->setInterval(2000);
        connect(autoRefresh_, &QTimer::timeout, tabs_, [this]() { refreshCurrent(); });
        connect(tabs_, &QTabWidget::currentChanged, tabs_, [this]() {
            if (currentView() != 8) cancelServiceQueries();
            updateRefreshSchedule();
            if (tabs_->isVisible()) refreshCurrent();
        });
        connect(experimentalTabs_, &QTabWidget::currentChanged, this, [this]() {
            if (currentView()!=8) cancelServiceQueries();
            updateRefreshSchedule();
            if (tabs_->isVisible()) refreshCurrent();
        });
        tabs_->installEventFilter(this);
        watchCloseWindow();
        updateRefreshSchedule();
        if (tabs_->isVisible()) refreshCurrent();
    }

    // Plan the exact argument vector without executing anything. System-scope
    // mutations must never turn a desktop user's PATH into a privileged target.
    // User-scope commands retain normal PATH resolution for session tooling.
    static QStringList serviceActionCommand(bool user, const QString &action, const QString &unit)
    {
        static const QRegularExpression unitName(QStringLiteral("\\A[A-Za-z0-9_@:.\\\\-]+\\.service\\z"));
        if (unit.size()>255 || unit.startsWith(QLatin1Char('-')) || !unitName.match(unit).hasMatch()
            || !QStringList{QStringLiteral("start"),QStringLiteral("stop"),QStringLiteral("restart"),QStringLiteral("enable"),QStringLiteral("disable")}.contains(action)) return {};
        const auto systemctl = user ? QStandardPaths::findExecutable(QStringLiteral("systemctl"))
                                    : tux::trustedSystemExecutable(QStringLiteral("systemctl"));
        if (systemctl.isEmpty()) return {};
        if (user) return {systemctl, QStringLiteral("--user"), action, QStringLiteral("--"), unit};
        const auto pkexec = tux::trustedSystemExecutable(QStringLiteral("pkexec"));
        if (pkexec.isEmpty()) return {};
        return {pkexec, systemctl, action, QStringLiteral("--"), unit};
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
    int currentView() const
    {
        QWidget *page=tabs_->currentWidget();
        if (page==experimentalTabs_) page=experimentalTabs_->currentWidget();
        return resourcePages_.indexOf(page);
    }

    void watchCloseWindow()
    {
        QWidget *window = tabs_->window();
        if (closeWindow_ == window) return;
        if (closeWindow_ != nullptr && closeWindow_ != tabs_)
            closeWindow_->removeEventFilter(this);
        closeWindow_ = window;
        if (window != tabs_) window->installEventFilter(this);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if ((watched == closeWindow_ || watched == tabs_) && event->type() == QEvent::Close
            && serviceMutationBusy_) {
            static_cast<QCloseEvent *>(event)->ignore();
            // Normal closing must not destroy a command awaiting PolicyKit or
            // detach the interface from a partially applied service operation.
            QMessageBox::information(tabs_, tr("Service action still running"),
                tr("Wait for the service action to finish before closing Task Manager. "
                   "If authorization is open, complete or cancel it in your desktop authentication dialog."));
            return true;
        }
        if (watched == tabs_) {
            if (event->type() == QEvent::Show) {
                watchCloseWindow();
                updateRefreshSchedule();
                refreshCurrent();
            } else if (event->type() == QEvent::Hide) {
                autoRefresh_->stop();
                performanceClock_.invalidate();
                if(resourceDetail_)resourceDetail_->resetBaseline();
                cancelServiceQueries();
            }
        }
        return QObject::eventFilter(watched, event);
    }

    void updateRefreshSchedule()
    {
        const int index = currentView();
        // Poll only live, read-only resource views. Opening Startup, Services
        // or Bluetooth refreshes that view once without recurring commands.
        const bool liveView = (index >= 0 && index <= 5) || index == 9;
        if (tabs_->isVisible() && liveView && !(index==0 && performancePaused_)) {
            const int interval=index==0?performanceInterval_:2000;
            if(autoRefresh_->interval()!=interval)autoRefresh_->setInterval(interval);
            if (!autoRefresh_->isActive()) autoRefresh_->start();
        } else {
            autoRefresh_->stop();
        }
        // Return to Performance with a fresh baseline rather than averaging
        // counters over time spent hidden or on another page.
        if (index != 0 || !tabs_->isVisible()) {performanceClock_.invalidate(); if(resourceDetail_)resourceDetail_->resetBaseline();}
    }

    void addReportTab(const QString &name, QPlainTextEdit **view)
    {
        QWidget *page = new QWidget(tabs_);
        auto *layout = new QVBoxLayout(page);
        if (name == tr("Performance")) {
            performanceSummary_ = new QLabel(page);
            performanceSummary_->setWordWrap(true);
            layout->addWidget(performanceSummary_);
            auto* controls=new QHBoxLayout;
            auto* speed=new QComboBox;speed->setObjectName("performanceRefreshRate");speed->setAccessibleName("Performance refresh interval");speed->addItems({"1 second","2 seconds","5 seconds"});speed->setCurrentIndex(1);controls->addWidget(speed);
            auto* pause=new QPushButton("Pause graphs");pause->setObjectName("performancePause");pause->setCheckable(true);controls->addWidget(pause);
            auto* refresh=new QPushButton("Refresh now");refresh->setObjectName("performanceRefresh");controls->addWidget(refresh);layout->addLayout(controls);
            auto* hint=new QLabel("Live local counters · Latest 90 samples · CPU/RAM: 0–100% · I/O: automatic scale · Polling stops when hidden");hint->setWordWrap(true);layout->addWidget(hint);
            resourceDetail_=new ResourceDetail(page);
            auto* resourceRow=new QHBoxLayout;auto* resourceNavigation=new QListWidget;resourceNavigation->setObjectName("performanceNavigation");resourceNavigation->setAccessibleName("Performance resources");resourceNavigation->setFixedWidth(145);resourceNavigation->addItems({"Overview","CPU","RAM","Disks","Network"});resourceRow->addWidget(resourceNavigation);resourceRow->addWidget(resourceDetail_,1);layout->addLayout(resourceRow,1);resourceDetail_->tabBar()->hide();
            connect(resourceNavigation,&QListWidget::currentRowChanged,resourceDetail_,&QTabWidget::setCurrentIndex);
            connect(resourceDetail_,&QTabWidget::currentChanged,resourceNavigation,QOverload<int>::of(&QListWidget::setCurrentRow));
            auto* overview=new QWidget;auto* grid=new QGridLayout(overview);resourceDetail_->insertTab(0,overview,"Overview");resourceDetail_->setCurrentIndex(1);resourceNavigation->setCurrentRow(1);
            connect(speed,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index){performanceInterval_=index==0?1000:index==1?2000:5000;autoRefresh_->setInterval(performanceInterval_);performanceClock_.invalidate();resourceDetail_->resetBaseline();});
            connect(pause,&QPushButton::toggled,this,[this,pause](bool paused){performancePaused_=paused;pause->setText(paused?"Resume graphs":"Pause graphs");performanceClock_.invalidate();resourceDetail_->resetBaseline();updateRefreshSchedule();});
            connect(refresh,&QPushButton::clicked,this,[this]{refreshPerformance();});
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
            filter->addItems({tr("All entries"), tr("User entries / overrides"), tr("System defaults + overrides")});
            filter->setObjectName(QStringLiteral("startupScopeFilter"));
        } else if (name == tr("Services")) {
            filter = new QComboBox(page);
            filter->addItems({tr("All services"), tr("System services"), tr("User services")});
            filter->setObjectName(QStringLiteral("serviceScopeFilter"));
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
            auto *intro = new QLabel(tr("Manage sign-in applications for your account. System defaults are never edited. Desktop restrictions and missing programs may prevent an enabled entry from starting."), page);
            intro->setWordWrap(true); layout->addWidget(intro);
            startupSearch_ = new QLineEdit(page);
            startupSearch_->setObjectName(QStringLiteral("startupSearch"));
            startupSearch_->setPlaceholderText(tr("Search application, filename or command"));
            startupSearch_->setClearButtonEnabled(true); layout->addWidget(startupSearch_);
            auto *row = new QHBoxLayout;
            startupEnable_ = new QPushButton(tr("Enable at sign-in"), page);
            startupDisable_ = new QPushButton(tr("Disable at sign-in"), page);
            startupUndo_ = new QPushButton(tr("Undo last change"), page);
            startupDetails_ = new QPushButton(tr("Details"), page);
            auto *refresh = new QPushButton(tr("Refresh"), page);
            startupEnable_->setObjectName(QStringLiteral("startupEnable"));
            startupDisable_->setObjectName(QStringLiteral("startupDisable"));
            startupUndo_->setObjectName(QStringLiteral("startupUndo"));
            startupDetails_->setObjectName(QStringLiteral("startupDetails"));
            refresh->setObjectName(QStringLiteral("startupRefresh"));
            for (auto *button : {startupEnable_, startupDisable_, startupUndo_, startupDetails_, refresh}) row->addWidget(button);
            layout->addLayout(row);
            startupTable_ = new QTableWidget(0, 4, page);
            startupTable_->setObjectName(QStringLiteral("startupTable"));
            startupTable_->setHorizontalHeaderLabels({tr("Application"),tr("Sign-in status"),tr("Source"),tr("Command")});
            startupTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
            startupTable_->setSelectionMode(QAbstractItemView::SingleSelection);
            startupTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
            startupTable_->setSortingEnabled(true);
            startupTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            startupTable_->verticalHeader()->hide(); layout->addWidget(startupTable_, 1);
            startupSummary_ = new QLabel(page); startupSummary_->setWordWrap(true); layout->addWidget(startupSummary_);
            auto *actionStatus = new QLabel(page); actionStatus->setWordWrap(true); layout->addWidget(actionStatus);
            actionStatusByTab_.insert(name, actionStatus);
            connect(startupSearch_, &QLineEdit::textChanged, this, [this] { filterStartupEntries(); });
            connect(startupTable_, &QTableWidget::itemSelectionChanged, this, [this] { updateStartupActions(); });
            connect(startupEnable_, &QPushButton::clicked, this, [this] { setStartupEnabled(true); });
            connect(startupDisable_, &QPushButton::clicked, this, [this] { setStartupEnabled(false); });
            connect(startupUndo_, &QPushButton::clicked, this, [this] { undoStartupChange(); });
            connect(startupDetails_, &QPushButton::clicked, this, [this] { showStartupDetails(); });
            connect(refresh, &QPushButton::clicked, this, [this] { refreshCurrent(); });
            tabs_->addTab(page, name); updateStartupActions(); return;
        }
        if (name == tr("Services")) {
            auto* intro=new QLabel(tr("Manage systemd services. Startup state and runtime status are distinct. System changes require administrator authorization; all actions require confirmation."),page);intro->setWordWrap(true);layout->addWidget(intro);
            serviceSearch_=new QLineEdit(page);serviceSearch_->setObjectName("serviceSearch");serviceSearch_->setPlaceholderText("Search service, scope, startup or runtime status…");serviceSearch_->setClearButtonEnabled(true);layout->addWidget(serviceSearch_);
            auto* refresh=new QPushButton("Refresh services");refresh->setObjectName("serviceRefresh");layout->addWidget(refresh);
            connect(refresh,&QPushButton::clicked,this,[this]{if(!serviceMutationBusy_){serviceRefresh_.invalidate();refreshCurrent();}});
            serviceSelection_ = new QComboBox(page);
            serviceSelection_->setObjectName(QStringLiteral("serviceSelection"));
            serviceSelection_->setMinimumWidth(250);
            serviceSelection_->hide();
            auto *row = new QHBoxLayout;
            row->addWidget(serviceSelection_, 1);
            const auto addAction = [this, page, row](const QString &label, const QString &action) {
                auto *button = new QPushButton(label, page);
                button->setObjectName(QStringLiteral("serviceAction_%1").arg(action));
                serviceActionButtons_.append(button);
                button->setEnabled(false);
                row->addWidget(button);
                connect(button, &QPushButton::clicked, tabs_, [this, action]() { changeService(action); });
            };
            addAction(tr("Start"), QStringLiteral("start"));
            addAction(tr("Stop"), QStringLiteral("stop"));
            addAction(tr("Restart"), QStringLiteral("restart"));
            addAction(tr("Enable"), QStringLiteral("enable"));
            addAction(tr("Disable"), QStringLiteral("disable"));
            layout->addLayout(row);
            serviceTable_=new QTableWidget(0,5,page);serviceTable_->setObjectName("serviceTable");serviceTable_->setHorizontalHeaderLabels({"Service","Scope","Startup state","Runtime","Substate"});serviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);serviceTable_->setSelectionMode(QAbstractItemView::SingleSelection);serviceTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);serviceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);serviceTable_->verticalHeader()->hide();layout->addWidget(serviceTable_,3);
            connect(serviceSearch_,&QLineEdit::textChanged,this,[this]{filterServices();});
            connect(serviceTable_,&QTableWidget::itemSelectionChanged,this,[this]{const int row=serviceTable_->currentRow();if(row>=0&&!serviceTable_->isRowHidden(row))serviceSelection_->setCurrentIndex(row);setServiceActionsEnabled(row>=0&&!serviceTable_->isRowHidden(row)&&!serviceLoading_);});
            connect(serviceSelection_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int row){if(serviceTable_&&row>=0&&row<serviceTable_->rowCount()&&!serviceTable_->isRowHidden(row))serviceTable_->selectRow(row);});
        }
        auto *text = new QPlainTextEdit(page);
        if (name == tr("GPU")) text->setObjectName(QStringLiteral("gpuSummary"));
        if (name == tr("Services")) text->setObjectName(QStringLiteral("serviceReport"));
        text->setReadOnly(true);
        text->setLineWrapMode(QPlainTextEdit::NoWrap);
        text->setPlaceholderText(tr("Reading system information…"));
        *view = text;
        layout->addWidget(text, 1);
        if (name == tr("Startup apps") || name == tr("Services")) {
            auto *actionStatus = new QLabel(page);
            if (name==tr("Services")) actionStatus->setObjectName(QStringLiteral("serviceActionStatus"));
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
        resourceDetail_->sample();
        double cpuPercent = 0.0;
        const auto cpu=tux::cpuTicks(read(QStringLiteral("/proc/stat"))).value("cpu");
        const quint64 totalTicks=cpu.total,idleTicks=cpu.idle;

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

    const tux::autostart::Entry *selectedStartupEntry() const
    {
        if (startupTable_ == nullptr || startupTable_->currentRow() < 0
            || startupTable_->isRowHidden(startupTable_->currentRow())) return nullptr;
        const auto *item = startupTable_->item(startupTable_->currentRow(),0);
        if (item == nullptr) return nullptr;
        const auto id=item->data(Qt::UserRole).toString();
        for (const auto &entry:startupEntries_) if (entry.id==id) return &entry;
        return nullptr;
    }

    void updateStartupActions()
    {
        if (startupEnable_==nullptr) return;
        const auto *entry=selectedStartupEntry();
        startupEnable_->setEnabled(entry!=nullptr && entry->valid && entry->hidden);
        startupDisable_->setEnabled(entry!=nullptr && entry->valid && !entry->hidden);
        startupUndo_->setEnabled(entry!=nullptr && tux::autostart::hasUndo(tux::autostart::Paths::environment(),entry->id));
        startupDetails_->setEnabled(entry!=nullptr);
    }

    void filterStartupEntries()
    {
        if (startupTable_==nullptr) return;
        const auto query=startupSearch_->text().trimmed();
        for (int row=0;row<startupTable_->rowCount();++row) {
            QString text;
            for (int column=0;column<4;++column) text+=startupTable_->item(row,column)->text()+QLatin1Char(' ');
            text+=startupTable_->item(row,0)->data(Qt::UserRole).toString()+startupTable_->item(row,3)->toolTip();
            startupTable_->setRowHidden(row,!text.contains(query,Qt::CaseInsensitive));
        }
        if (startupTable_->currentRow()>=0 && startupTable_->isRowHidden(startupTable_->currentRow())) {
            startupTable_->clearSelection(); startupTable_->setCurrentItem(nullptr);
        }
        updateStartupActions();
    }

    void refreshStartupEntries(int filter)
    {
        if (startupTable_==nullptr) return;
        const auto *selected=selectedStartupEntry();
        const auto selectedId=selected?selected->id:QString();
        const QSignalBlocker blocker(startupTable_);
        startupTable_->setSortingEnabled(false); startupTable_->setRowCount(0);
        startupEntries_=tux::autostart::discover(tux::autostart::Paths::environment());
        int eligible=0;
        for (auto &entry:startupEntries_) {
            entry.contents.clear(); // UI metadata does not need retained file contents.
            if ((filter==1 && !entry.userOverride) || (filter==2 && !entry.hasSystemDefault)) continue;
            const int row=startupTable_->rowCount(); startupTable_->insertRow(row);
            const auto source=entry.userOverride
                ? (entry.hasSystemDefault?tr("User override of system default"):tr("User entry")) : tr("System default");
            const QStringList values{entry.name,entry.reason,source,entry.command.left(240)};
            for (int column=0;column<4;++column) {
                auto *item=new QTableWidgetItem(values[column]);
                item->setData(Qt::UserRole,entry.id); startupTable_->setItem(row,column,item);
            }
            startupTable_->item(row,0)->setToolTip(entry.id);
            startupTable_->item(row,3)->setToolTip(entry.command);
            if (entry.eligible) ++eligible;
        }
        startupTable_->setSortingEnabled(true);
        for (int row=0;row<startupTable_->rowCount();++row)
            if (startupTable_->item(row,0)->data(Qt::UserRole).toString()==selectedId) startupTable_->selectRow(row);
        startupSummary_->setText(tr("%1 effective entries shown · %2 eligible for this desktop · eligibility is not proof an app will start successfully")
            .arg(startupTable_->rowCount()).arg(eligible));
        filterStartupEntries();
    }

    bool confirmStartup(const QString &title,const QString &text)
    {
        QMessageBox dialog(QMessageBox::Question,title,text,QMessageBox::Yes|QMessageBox::Cancel,tabs_);
        dialog.setTextFormat(Qt::PlainText); dialog.setDefaultButton(QMessageBox::Cancel);
        return dialog.exec()==QMessageBox::Yes;
    }

    void setStartupEnabled(bool enabled)
    {
        const auto *selected=selectedStartupEntry();
        if (selected==nullptr) return;
        const auto entry=*selected;
        if (!confirmStartup(enabled?tr("Enable startup application"):tr("Disable startup application"),
            tr("%1 %2 at sign-in for your account? This does not start or stop it now. Original system files remain unchanged. Undo last change will be available.")
                .arg(enabled?tr("Enable"):tr("Disable"),entry.name))) return;
        const auto result=tux::autostart::setEnabled(tux::autostart::Paths::environment(),entry.id,enabled);
        statusMessage_=result.message;
        refreshCurrent();
    }

    void undoStartupChange()
    {
        const auto *selected=selectedStartupEntry(); if (selected==nullptr) return;
        const auto id=selected->id;
        if (!confirmStartup(tr("Undo startup change"),tr("Restore the saved previous user state for %1? An entry edited outside Tux Steward will not be overwritten.").arg(selected->name))) return;
        statusMessage_=tux::autostart::undo(tux::autostart::Paths::environment(),id).message;
        refreshCurrent();
    }

    void showStartupDetails()
    {
        const auto *entry=selectedStartupEntry(); if (entry==nullptr) return;
        QMessageBox dialog(QMessageBox::Information,tr("Startup entry details"),
            tr("Application: %1\nFile: %2\nEffective source: %3\nCommand (display only): %4\nStatus: %5")
                .arg(entry->name,entry->id,entry->path,entry->command,entry->reason),QMessageBox::Close,tabs_);
        dialog.setTextFormat(Qt::PlainText); dialog.exec();
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
        const int index = currentView();
        if (index == 0 && !performancePaused_) refreshPerformance();
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
            refreshStartupEntries(filter);
        } else if (index == 8) {
            if (!serviceRefresh_.isValid() || serviceRefresh_.elapsed() >= 10000) {
                const int filter = filters_.value(tr("Services"))->currentIndex();
                refreshServices(filter);
            }
        } else if (index == 9) gpu_->setPlainText(gpuReport());
        if (index == 7 && actionStatusByTab_.value(tr("Startup apps")) != nullptr)
            actionStatusByTab_.value(tr("Startup apps"))->setText(statusMessage_);
        if (index == 8 && actionStatusByTab_.value(tr("Services")) != nullptr)
            actionStatusByTab_.value(tr("Services"))->setText(statusMessage_);
    }

    void setServiceActionsEnabled(bool enabled)
    {
        enabled = enabled && !serviceMutationBusy_ && !serviceLoading_;
        if(serviceTable_)serviceTable_->setEnabled(!serviceMutationBusy_&&!serviceLoading_);
        if (serviceSelection_ != nullptr) serviceSelection_->setEnabled(enabled);
        for (auto *button : serviceActionButtons_) button->setEnabled(enabled);
    }

    void cancelServiceQueries()
    {
        if (!serviceLoading_) return;
        ++serviceGeneration_;
        serviceLoading_ = false;
        serviceRefresh_.invalidate();
        const auto queries = serviceQueries_;
        serviceQueries_.clear();
        for (const auto &query : queries) if (query != nullptr) query->cancel();
        setServiceActionsEnabled(false);
    }

    void refreshServices(int filter)
    {
        if (!tabs_->isVisible()) return;
        if (serviceLoading_ && serviceActiveFilter_ == filter) return;
        cancelServiceQueries();
        const auto previousSelection = serviceSelection_->currentData().toMap();
        serviceSelection_->clear();
        serviceTable_->setRowCount(0);
        setServiceActionsEnabled(false);
        const auto executable = QStandardPaths::findExecutable(QStringLiteral("systemctl"));
        if (executable.isEmpty()) {
            services_->setPlainText(tr("systemctl is unavailable on this system."));
            serviceRefresh_.restart();
            return;
        }
        const int generation = ++serviceGeneration_;
        serviceActiveFilter_ = filter;
        serviceLoading_ = true;
        serviceReports_.clear();
        serviceChoices_.clear();
        serviceRuntime_.clear();
        servicePending_ = filter == 0 ? 4 : 2;
        services_->setPlainText(tr("Loading services… You can keep using the other tabs."));
        serviceRefresh_.restart();
        for (bool user : {false, true}) {
            if ((filter == 1 && user) || (filter == 2 && !user)) continue;
            QStringList runtimeArgs;if(user)runtimeArgs<<"--user";
            runtimeArgs<<"list-units"<<"--all"<<"--type=service"<<"--plain"<<"--no-legend"<<"--no-pager";
            auto* runtime=AsyncCommand::run(this,executable,runtimeArgs,3000,512*1024,[this,generation,user,filter,previousSelection](AsyncCommandResult result){
                if(generation!=serviceGeneration_)return;
                if(result.ok())for(const auto& line:QString::fromLocal8Bit(result.output).split('\n')) {
                    const auto fields=line.simplified().split(' ');if(fields.size()>=4&&fields[0].endsWith(".service"))serviceRuntime_.insert((user?"user:":"system:")+fields[0],fields[2]+"|"+fields[3]);
                }
                else serviceRuntime_.insert(user?"user:ERROR":"system:ERROR","Unavailable");
                if(--servicePending_==0)finishServices(filter,previousSelection);
            });serviceQueries_.append(runtime);
            QStringList args;
            if (user) args << QStringLiteral("--user");
            args << QStringLiteral("list-unit-files") << QStringLiteral("--type=service")
                 << QStringLiteral("--no-legend") << QStringLiteral("--no-pager");
            auto *query = AsyncCommand::run(this, executable, args, 3000, 512 * 1024,
                [this, generation, user, filter, previousSelection](AsyncCommandResult result) {
                    if (generation != serviceGeneration_) return;
                    const auto scope = user ? tr("User") : tr("System");
                    QString report = scope + tr(" services\n\n");
                    if (result.ok()) {
                        report += QString::fromLocal8Bit(result.output);
                        const auto lines = QString::fromLocal8Bit(result.output).split(QLatin1Char('\n'));
                        for (const auto &line : lines) {
                            const auto unit = line.simplified().section(QLatin1Char(' '), 0, 0);
                            if (!unit.endsWith(QStringLiteral(".service"))) continue;
                            QVariantMap data;
                            data.insert(QStringLiteral("unit"), unit);
                            data.insert(QStringLiteral("user"), user);
                            data.insert(QStringLiteral("startup"),line.simplified().section(QLatin1Char(' '),1,1));
                            serviceChoices_.append(qMakePair(unit, data));
                        }
                    } else if (result.timedOut) report += tr("Query timed out after 3 seconds.");
                    else if (result.truncated) report += tr("Service output exceeded 512 KiB; this scope's actions are unavailable.");
                    else report += tr("Query failed: %1\n%2").arg(result.error.isEmpty()
                        ? tr("exit code %1").arg(result.exitCode) : result.error,
                        QString::fromLocal8Bit(result.output));
                    serviceReports_.insert(user ? 1 : 0, report);
                    if(--servicePending_==0)finishServices(filter,previousSelection);
                });
            serviceQueries_.append(query);
        }
    }

    void finishServices(int filter,const QVariantMap& previousSelection) {
                    serviceLoading_ = false;
                    serviceQueries_.clear();
                    QStringList reports;
                    for (int scopeIndex : {0, 1}) if (serviceReports_.contains(scopeIndex))
                        reports.append(serviceReports_.value(scopeIndex));
                    services_->setPlainText(reports.join(QStringLiteral("\n\n")));
                    std::sort(serviceChoices_.begin(), serviceChoices_.end(),
                        [](const QPair<QString, QVariantMap> &left, const QPair<QString, QVariantMap> &right) {
                            if (left.first == right.first)
                                return left.second.value(QStringLiteral("user")).toBool()
                                    < right.second.value(QStringLiteral("user")).toBool();
                            return left.first < right.first;
                        });
                    for (const auto &choice : serviceChoices_) {
                        const bool isUser = choice.second.value(QStringLiteral("user")).toBool();
                        serviceSelection_->addItem(filter == 0
                            ? QStringLiteral("%1  ·  %2").arg(choice.first, isUser ? tr("User") : tr("System"))
                            : choice.first, choice.second);
                    }
                    for (int index = 0; index < serviceSelection_->count(); ++index)
                        if (serviceSelection_->itemData(index).toMap().value("unit")==previousSelection.value("unit") && serviceSelection_->itemData(index).toMap().value("user")==previousSelection.value("user"))
                            serviceSelection_->setCurrentIndex(index);
                    {const QSignalBlocker block(serviceTable_);serviceTable_->setRowCount(serviceSelection_->count());
                    for(int row=0;row<serviceSelection_->count();++row) {
                        const auto data=serviceSelection_->itemData(row).toMap();const bool user=data.value("user").toBool();const auto unit=data.value("unit").toString();const auto key=(user?QString("user:"):QString("system:"));
                        const auto runtime=serviceRuntime_.value(key+unit,serviceRuntime_.contains(key+"ERROR")?"Unavailable":"Not loaded");
                        const QStringList fields{unit,user?"User":"System",data.value("startup").toString(),runtime.section('|',0,0),runtime.section('|',1,1)};
                        for(int col=0;col<fields.size();++col)serviceTable_->setItem(row,col,new QTableWidgetItem(fields[col]));
                    }}
                    filterServices();
    }
    void filterServices() {
        int first=-1;for(int row=0;row<serviceTable_->rowCount();++row) {QStringList fields;for(int col=0;col<serviceTable_->columnCount();++col)fields.append(serviceTable_->item(row,col)->text());const bool matches=fields.join(' ').contains(serviceSearch_->text().trimmed(),Qt::CaseInsensitive);serviceTable_->setRowHidden(row,!matches);if(matches&&first<0)first=row;}
        int row=serviceSelection_->currentIndex();if(row<0||serviceTable_->isRowHidden(row))row=first;
        if(row>=0) {serviceTable_->selectRow(row);serviceSelection_->setCurrentIndex(row);}else serviceTable_->clearSelection();
        setServiceActionsEnabled(row>=0&&!serviceLoading_);
    }

    void changeService(const QString &action)
    {
        if (serviceMutationBusy_ || serviceLoading_ || serviceSelection_ == nullptr || serviceSelection_->currentIndex() < 0 || serviceTable_->isRowHidden(serviceSelection_->currentIndex())) return;
        const QVariantMap data = serviceSelection_->currentData().toMap();
        const QString unit = data.value(QStringLiteral("unit")).toString();
        const bool user = data.value(QStringLiteral("user")).toBool();
        if (unit.isEmpty() || !unit.endsWith(QStringLiteral(".service"))
            || !QStringList{QStringLiteral("start"),QStringLiteral("stop"),QStringLiteral("restart"),QStringLiteral("enable"),QStringLiteral("disable")}.contains(action)) return;
        const QString scope = user ? tr("your user session") : tr("the whole system");
        const auto answer = QMessageBox::warning(tabs_, tr("Change service"),
            tr("Run ‘%1’ for %2 in %3? Changing system services can affect networking, login, or other apps.")
                .arg(action, unit, scope), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes || serviceMutationBusy_ || serviceLoading_) return;

        auto command = serviceActionCommand(user, action, unit);
        if (command.isEmpty()) {
            statusMessage_ = user ? tr("systemctl is unavailable; no service action was started.")
                : tr("Trusted systemctl or administrator authorization is unavailable; no service action was started.");
            if (actionStatusByTab_.value(tr("Services")) != nullptr)
                actionStatusByTab_.value(tr("Services"))->setText(statusMessage_);
            return;
        }
        const QString program = command.takeFirst();
        const QStringList args = command;
        serviceMutationBusy_ = true;
        setServiceActionsEnabled(false);
        auto *process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);
        process->setStandardInputFile(QProcess::nullDevice());
        auto output = std::make_shared<QByteArray>();
        auto completed = std::make_shared<bool>(false);
        const auto drain = [process,output]() {
            output->append(process->readAllStandardOutput());
            if (output->size()>65536) *output=output->right(65536);
        };
        const auto complete = [this,process,output,completed,drain,action,unit](bool success,const QString &error) {
            if (*completed) return;
            *completed=true; drain();
            statusMessage_=success?tr("‘%1’ completed for %2.").arg(action,unit)
                :tr("Could not %1 %2: %3").arg(action,unit,error.isEmpty()?QString::fromLocal8Bit(*output).trimmed():error);
            serviceMutationBusy_=false;
            process->deleteLater();
            serviceRefresh_.invalidate();
            setServiceActionsEnabled(!serviceLoading_ && serviceSelection_->count()>0);
            if (actionStatusByTab_.value(tr("Services")) != nullptr)
                actionStatusByTab_.value(tr("Services"))->setText(statusMessage_);
            refreshCurrent();
        };
        connect(process,&QProcess::readyReadStandardOutput,this,drain);
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,
            [complete](int code,QProcess::ExitStatus status) {
                complete(status==QProcess::NormalExit && code==0,
                    status==QProcess::NormalExit?QString():QObject::tr("The service command terminated unexpectedly."));
            });
        connect(process,&QProcess::errorOccurred,this,[process,complete](QProcess::ProcessError error) {
            if (error==QProcess::FailedToStart) complete(false,process->errorString());
        });
        statusMessage_ = tr("Waiting for service manager…");
        if (actionStatusByTab_.value(tr("Services")) != nullptr)
            actionStatusByTab_.value(tr("Services"))->setText(statusMessage_);
        process->start(program, args);
    }

    QTabWidget *tabs_ = nullptr;
    QPointer<QWidget> closeWindow_;
    QTabWidget *experimentalTabs_ = nullptr;
    QVector<QWidget *> resourcePages_;
    int firstIndex_ = 0;
    QHash<QString, QComboBox *> filters_;
    QHash<QString, QLabel *> actionStatusByTab_;
    QTableWidget *startupTable_ = nullptr;
    QLineEdit *startupSearch_ = nullptr;
    QLabel *startupSummary_ = nullptr;
    QPushButton *startupEnable_ = nullptr;
    QPushButton *startupDisable_ = nullptr;
    QPushButton *startupUndo_ = nullptr;
    QPushButton *startupDetails_ = nullptr;
    QVector<tux::autostart::Entry> startupEntries_;
    QTableWidget* serviceTable_=nullptr;
    QLineEdit* serviceSearch_=nullptr;
    QMap<QString,QString> serviceRuntime_;
    QComboBox *serviceSelection_ = nullptr;
    QVector<QPushButton *> serviceActionButtons_;
    QVector<QPointer<AsyncCommand>> serviceQueries_;
    QMap<int, QString> serviceReports_;
    QVector<QPair<QString, QVariantMap>> serviceChoices_;
    int serviceGeneration_ = 0;
    int servicePending_ = 0;
    int serviceActiveFilter_ = -1;
    bool serviceLoading_ = false;
    bool serviceMutationBusy_ = false;
    QString statusMessage_;
    ResourceDetail* resourceDetail_=nullptr;
    bool performancePaused_=false;
    int performanceInterval_=2000;
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
    QPlainTextEdit *services_ = nullptr;
    QPlainTextEdit *gpu_ = nullptr;
    QTimer *autoRefresh_ = nullptr;
    QElapsedTimer serviceRefresh_;
};

