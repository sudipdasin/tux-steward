// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/performance.hpp"
#include "syspulse/metrics.hpp"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <utility>

namespace syspulse {
namespace {

const QColor CpuColor(QStringLiteral("#16813D"));
const QColor MemoryColor(QStringLiteral("#C8102E"));
const QColor DiskColor(QStringLiteral("#246AD3"));
const QColor NetworkColor(QStringLiteral("#8B40C6"));
const QColor SecondaryColor(QStringLiteral("#667085"));

QString readBounded(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto bytes = file.read(1024 * 1024 + 1);
    if (bytes.size() > 1024 * 1024) return {};
    return QString::fromUtf8(bytes);
}

QString decimal(double value, int digits = 1) { return QString::number(value, 'f', digits); }

class HistoryChart final : public QWidget {
public:
    HistoryChart(QString title, QString unit, QColor color, double maximum,
                 bool compact = false, QWidget* parent = nullptr)
        : QWidget(parent), title_(std::move(title)), unit_(std::move(unit)),
          color_(std::move(color)), fixedMaximum_(maximum), compact_(compact)
    {
        setMinimumSize(compact ? 110 : 230, compact ? 78 : 140);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAccessibleName(title_ + QStringLiteral(" history chart"));
    }

    void setMaximum(double maximum) { fixedMaximum_ = maximum; }
    void setLegend(const QString& legend) { legend_ = legend; update(); }
    void setStatus(const QString& status) { status_ = status; update(); }
    void clear()
    {
        samples_.clear();
        status_ = QStringLiteral("Collecting baseline");
        update();
    }

    void sample(double seconds, std::optional<double> first,
                std::optional<double> second = {}, const QString& missing = QStringLiteral("Unavailable"))
    {
        if (first && (!std::isfinite(*first) || *first < 0)) first.reset();
        if (second && (!std::isfinite(*second) || *second < 0)) second.reset();
        samples_.append({seconds, first, second});
        while (samples_.size() > 128 || (samples_.size() > 1 && samples_[1].seconds < seconds - 60.0))
            samples_.removeFirst();
        status_ = first ? decimal(*first, unit_ == QStringLiteral("%") ? 0 : 1) + QLatin1Char(' ') + unit_ : missing;
        setAccessibleDescription(title_ + QStringLiteral(": ") + status_ + QStringLiteral(". ") + legend_);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), Qt::white);
        const int left = compact_ ? 8 : 50;
        const int top = compact_ ? 30 : 44;
        const int bottom = compact_ ? 6 : 24;
        const QRectF plot(left, top, qMax(1, width() - left - 14), qMax(1, height() - top - bottom));
        QFont titleFont = font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(QColor(QStringLiteral("#202124")));
        painter.drawText(QRect(8, 2, width() - 16, 22), Qt::AlignLeft | Qt::AlignVCenter, title_);
        painter.setPen(color_);
        const auto statusWidth = qMax(80, width() / 2);
        painter.drawText(QRect(width() - statusWidth - 8, 2, statusWidth, 22), Qt::AlignRight | Qt::AlignVCenter, status_);
        auto axisFont = font();
        axisFont.setPointSizeF(qMax(8.0, font().pointSizeF() - 1.0));
        painter.setFont(axisFont);
        if (!compact_ && !legend_.isEmpty()) {
            painter.setPen(SecondaryColor);
            painter.drawText(QRect(8, 23, width() - 16, 18), Qt::AlignLeft | Qt::AlignVCenter, legend_);
        }
        double maximum = fixedMaximum_;
        if (!(maximum > 0)) {
            maximum = 1.0;
            for (const auto& item : samples_) {
                if (item.first) maximum = qMax(maximum, *item.first * 1.2);
                if (item.second) maximum = qMax(maximum, *item.second * 1.2);
            }
        }
        painter.setPen(QPen(QColor(QStringLiteral("#E3E7EE")), 1));
        for (int i = 0; i <= 4; ++i) {
            const auto y = plot.top() + plot.height() * i / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            if (!compact_) {
                painter.setPen(SecondaryColor);
                painter.drawText(QRectF(0, y - 9, left - 7, 18), Qt::AlignRight | Qt::AlignVCenter,
                                 decimal(maximum * (4 - i) / 4.0, maximum >= 4 ? 0 : 1));
                painter.setPen(QPen(QColor(QStringLiteral("#E3E7EE")), 1));
            }
        }
        for (int i = 0; i <= 6; ++i) {
            const auto x = plot.left() + plot.width() * i / 6.0;
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        if (!compact_) {
            painter.setPen(SecondaryColor);
            painter.drawText(QRectF(plot.left(), plot.bottom() + 4, 50, 18), Qt::AlignLeft, QStringLiteral("−60 s"));
            painter.drawText(QRectF(plot.right() - 40, plot.bottom() + 4, 40, 18), Qt::AlignRight, QStringLiteral("0 s"));
        }
        if (samples_.isEmpty()) return;
        const auto newest = samples_.last().seconds;
        auto drawSeries = [&](bool secondary) {
            QPainterPath line;
            QPainterPath area;
            QPointF lastPoint;
            bool continuing = false;
            for (const auto& item : samples_) {
                const auto value = secondary ? item.second : item.first;
                if (!value || item.seconds < newest - 60.0) {
                    if (continuing) {
                        area.lineTo(lastPoint.x(), plot.bottom());
                        area.closeSubpath();
                    }
                    continuing = false;
                    continue;
                }
                const auto x = plot.right() - (newest - item.seconds) * plot.width() / 60.0;
                const auto y = plot.bottom() - qBound(0.0, *value / maximum, 1.0) * plot.height();
                if (!continuing) {
                    line.moveTo(x, y);
                    area.moveTo(x, plot.bottom());
                } else line.lineTo(x, y);
                area.lineTo(x, y);
                lastPoint = QPointF(x, y);
                continuing = true;
            }
            if (continuing) {
                area.lineTo(lastPoint.x(), plot.bottom());
                area.closeSubpath();
            }
            painter.save();
            painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
            auto fill = secondary ? SecondaryColor : color_;
            fill.setAlpha(secondary ? 12 : 24);
            painter.fillPath(area, fill);
            painter.setPen(QPen(secondary ? SecondaryColor : color_, compact_ ? 1.6 : 2.0));
            painter.drawPath(line);
            painter.restore();
        };
        drawSeries(true);
        drawSeries(false);
    }

private:
    struct Sample { double seconds; std::optional<double> first, second; };
    QString title_, unit_, legend_, status_ = QStringLiteral("Collecting baseline");
    QColor color_;
    double fixedMaximum_;
    bool compact_;
    QVector<Sample> samples_;
};

QWidget* scrollPage(QTabWidget* tabs, const QString& title, QVBoxLayout** layout)
{
    auto* area = new QScrollArea(tabs);
    area->setFrameShape(QFrame::NoFrame);
    area->setWidgetResizable(true);
    auto* content = new QWidget;
    *layout = new QVBoxLayout(content);
    (*layout)->setContentsMargins(8, 12, 8, 12);
    (*layout)->setSpacing(12);
    area->setWidget(content);
    tabs->addTab(area, title);
    return content;
}

QFrame* card(QVBoxLayout** layout)
{
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("performanceCard"));
    frame->setStyleSheet(QStringLiteral("QFrame#performanceCard{background:#FFFFFF;border:1px solid #D5DAE3;border-radius:10px;}"));
    *layout = new QVBoxLayout(frame);
    (*layout)->setContentsMargins(12, 10, 12, 10);
    (*layout)->setSpacing(6);
    return frame;
}

QLabel* info(QVBoxLayout* layout, const QString& text = {})
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(label);
    return label;
}

HistoryChart* chart(QVBoxLayout* layout, const QString& title, const QString& unit,
                    const QColor& color, double maximum, const QString& objectName)
{
    auto* result = new HistoryChart(title, unit, color, maximum);
    result->setObjectName(objectName);
    layout->addWidget(result, 1);
    return result;
}

QStringList cpuNames(const QMap<QString, metrics::CpuTicks>& cpus)
{
    auto names = cpus.keys();
    names.removeAll(QStringLiteral("cpu"));
    std::sort(names.begin(), names.end(), [](const QString& first, const QString& second) {
        return first.mid(3).toULongLong() < second.mid(3).toULongLong();
    });
    return names;
}

bool updateChoices(QComboBox* box, const QStringList& names)
{
    QStringList previous;
    for (int i = 0; i < box->count(); ++i) previous.append(box->itemText(i));
    if (previous == names) return false;
    const auto selected = box->currentText();
    const QSignalBlocker blocker(box);
    box->clear();
    box->addItems(names);
    if (names.contains(selected)) box->setCurrentText(selected);
    else if (names.contains(QStringLiteral("eth0"))) box->setCurrentText(QStringLiteral("eth0"));
    else if (names.size() > 1 && box->currentText() == QStringLiteral("lo"))
        box->setCurrentIndex(1);
    return selected != box->currentText();
}

} // namespace

struct PerformancePage::Impl {
    explicit Impl(PerformancePage* page) : owner(page)
    {
        auto* root = new QVBoxLayout(page);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(8);
        auto* toolbar = new QHBoxLayout;
        sampling = new QLabel(QStringLiteral("Collecting the first sample…"));
        sampling->setObjectName(QStringLiteral("syspulseSamplingStatus"));
        sampling->setWordWrap(true);
        toolbar->addWidget(sampling, 1);
        pause = new QPushButton(QStringLiteral("Pause"));
        pause->setObjectName(QStringLiteral("syspulsePerformancePause"));
        pause->setAccessibleName(QStringLiteral("Pause or resume performance sampling"));
        toolbar->addWidget(pause);
        speed = new QComboBox;
        speed->setObjectName(QStringLiteral("syspulseSamplingInterval"));
        speed->setAccessibleName(QStringLiteral("Performance sampling interval"));
        speed->addItem(QStringLiteral("1 second"), 1000);
        speed->addItem(QStringLiteral("2 seconds"), 2000);
        speed->addItem(QStringLiteral("5 seconds"), 5000);
        toolbar->addWidget(speed);
        root->addLayout(toolbar);
        tabs = new QTabWidget;
        tabs->setObjectName(QStringLiteral("syspulsePerformanceTabs"));
        root->addWidget(tabs, 1);

        QVBoxLayout* summary;
        scrollPage(tabs, QStringLiteral("Summary"), &summary);
        auto* summaryColumns = new QHBoxLayout;
        summary->addLayout(summaryColumns, 1);
        auto* main = new QVBoxLayout;
        main->setSpacing(10);
        summaryColumns->addLayout(main, 4);
        auto* side = new QVBoxLayout;
        side->setSpacing(10);
        summaryColumns->addLayout(side, 1);

        QVBoxLayout* cpuLayout;
        auto* cpuPanel = card(&cpuLayout);
        main->addWidget(cpuPanel, 2);
        cpuSummary = chart(cpuLayout, QStringLiteral("CPU"), QStringLiteral("%"), CpuColor, 100, QStringLiteral("syspulseCpuSummaryChart"));
        cpuSummary->setLegend(QStringLiteral("Total processor usage · 60-second history"));
        summaryCoreLayout = new QHBoxLayout;
        summaryCoreLayout->setSpacing(8);
        cpuLayout->addLayout(summaryCoreLayout);
        auto* allCores = new QPushButton(QStringLiteral("View all logical cores →"));
        allCores->setFlat(true);
        cpuLayout->addWidget(allCores);
        QObject::connect(allCores, &QPushButton::clicked, owner, [this] { tabs->setCurrentIndex(1); });

        auto* resources = new QHBoxLayout;
        resources->setSpacing(10);
        main->addLayout(resources);
        QVBoxLayout* memLayout;
        resources->addWidget(card(&memLayout), 1);
        memorySummary = chart(memLayout, QStringLiteral("Memory"), QStringLiteral("GiB"), MemoryColor, 0, QStringLiteral("syspulseMemorySummaryChart"));
        memorySummaryInfo = info(memLayout, QStringLiteral("Waiting for memory counters"));
        QVBoxLayout* diskLayout;
        resources->addWidget(card(&diskLayout), 1);
        diskSummary = chart(diskLayout, QStringLiteral("Disk activity"), QStringLiteral("MiB/s"), DiskColor, 0, QStringLiteral("syspulseDiskSummaryChart"));
        diskSummary->setLegend(QStringLiteral("Read: blue · Write: slate · Base devices only"));
        diskSummaryInfo = info(diskLayout, QStringLiteral("Collecting baseline"));
        QVBoxLayout* netLayout;
        main->addWidget(card(&netLayout), 1);
        networkSummary = chart(netLayout, QStringLiteral("Network"), QStringLiteral("KiB/s"), NetworkColor, 0, QStringLiteral("syspulseNetworkSummaryChart"));
        networkSummary->setLegend(QStringLiteral("Receive: purple · Transmit: slate · Non-loopback interfaces"));
        networkSummaryInfo = info(netLayout, QStringLiteral("Collecting baseline"));

        QVBoxLayout* watch;
        auto* watchPanel = card(&watch);
        watchPanel->setMinimumWidth(180);
        watchPanel->setMaximumWidth(270);
        side->addWidget(watchPanel);
        auto* watchHeading = info(watch, QStringLiteral("SYSTEM WATCH"));
        auto headingFont = watchHeading->font(); headingFont.setBold(true); watchHeading->setFont(headingFont);
        cpuInfoSummary = info(watch, QStringLiteral("CPU baseline pending"));
        auto* services = new QPushButton(QStringLiteral("Manage services →"));
        auto* startup = new QPushButton(QStringLiteral("Review startup apps →"));
        watch->addWidget(startup);
        watch->addWidget(services);
        QObject::connect(startup, &QPushButton::clicked, owner, [this] { if (owner->onOpenStartup) owner->onOpenStartup(); });
        QObject::connect(services, &QPushButton::clicked, owner, [this] { if (owner->onOpenServices) owner->onOpenServices(); });
        QVBoxLayout* semantics;
        auto* notes = card(&semantics);
        notes->setMaximumWidth(270);
        side->addWidget(notes);
        info(semantics, QStringLiteral("READINGS"))->setFont(headingFont);
        info(semantics, QStringLiteral("Memory in use = total − available. Reclaimable cache is included in available memory.\n\nDisk rates count base block devices, excluding their partitions and stacked logical devices.\n\nNetwork totals exclude loopback; bridged and virtual interfaces can represent the same traffic."));
        side->addStretch();

        QVBoxLayout* cpu;
        scrollPage(tabs, QStringLiteral("CPU"), &cpu);
        cpuInfo = info(cpu, QStringLiteral("CPU counters pending"));
        cpuTotal = chart(cpu, QStringLiteral("Total CPU"), QStringLiteral("%"), CpuColor, 100, QStringLiteral("syspulseCpuDetailChart"));
        cpuTotal->setLegend(QStringLiteral("All logical processors · 60-second history"));
        auto* cores = new QWidget;
        coreGrid = new QGridLayout(cores);
        coreGrid->setContentsMargins(0, 8, 0, 0);
        coreGrid->setSpacing(10);
        cpu->addWidget(cores);
        cpu->addStretch();

        QVBoxLayout* memory;
        scrollPage(tabs, QStringLiteral("Memory"), &memory);
        memoryInfo = info(memory, QStringLiteral("Memory counters pending"));
        memoryDetail = chart(memory, QStringLiteral("Physical memory in use"), QStringLiteral("GiB"), MemoryColor, 0, QStringLiteral("syspulseMemoryDetailChart"));
        memoryDetail->setLegend(QStringLiteral("Total − MemAvailable · 60-second history"));
        swapDetail = chart(memory, QStringLiteral("Swap in use"), QStringLiteral("GiB"), SecondaryColor, 0, QStringLiteral("syspulseSwapDetailChart"));
        memory->addStretch();

        QVBoxLayout* disk;
        scrollPage(tabs, QStringLiteral("Disks"), &disk);
        disks = new QComboBox;
        disks->setObjectName(QStringLiteral("syspulseDiskDevice"));
        disks->setAccessibleName(QStringLiteral("Select block device"));
        disk->addWidget(disks);
        diskInfo = info(disk, QStringLiteral("Looking for block devices"));
        diskDetail = chart(disk, QStringLiteral("Selected disk throughput"), QStringLiteral("MiB/s"), DiskColor, 0, QStringLiteral("syspulseDiskDetailChart"));
        diskDetail->setLegend(QStringLiteral("Read: blue · Write: slate · 512-byte diskstats sectors"));
        diskBusy = chart(disk, QStringLiteral("Device busy time"), QStringLiteral("%"), DiskColor, 100, QStringLiteral("syspulseDiskBusyChart"));
        disk->addStretch();

        QVBoxLayout* network;
        scrollPage(tabs, QStringLiteral("Network"), &network);
        networks = new QComboBox;
        networks->setObjectName(QStringLiteral("syspulseNetworkInterface"));
        networks->setAccessibleName(QStringLiteral("Select network interface"));
        network->addWidget(networks);
        networkInfo = info(network, QStringLiteral("Looking for network interfaces"));
        networkDetail = chart(network, QStringLiteral("Selected interface throughput"), QStringLiteral("KiB/s"), NetworkColor, 0, QStringLiteral("syspulseNetworkDetailChart"));
        networkDetail->setLegend(QStringLiteral("Receive: purple · Transmit: slate · 60-second history"));
        network->addStretch();

        QObject::connect(disks, QOverload<int>::of(&QComboBox::currentIndexChanged), owner, [this] {
            diskBaseline = false; diskDetail->clear(); diskBusy->clear();
            diskInfo->setText(QStringLiteral("Collecting baseline for selected block device…"));
        });
        QObject::connect(networks, QOverload<int>::of(&QComboBox::currentIndexChanged), owner, [this] {
            networkBaseline = false; networkDetail->clear();
            networkInfo->setText(QStringLiteral("Collecting baseline for selected interface…"));
        });
        timer = new QTimer(owner);
        timer->setObjectName(QStringLiteral("syspulseSamplingTimer"));
        timer->setInterval(1000);
        QObject::connect(timer, &QTimer::timeout, owner, [this] { sample(); });
        QObject::connect(pause, &QPushButton::clicked, owner, [this] { owner->setPaused(!paused); });
        QObject::connect(speed, QOverload<int>::of(&QComboBox::currentIndexChanged), owner, [this] {
            timer->setInterval(speed->currentData().toInt());
            sampleClock.invalidate();
            gap();
            if (enabled && !paused) sample();
        });
        historyClock.start();
        sample();
        timer->start();
    }

    void gap()
    {
        const auto now = historyClock.nsecsElapsed() / 1e9;
        for (auto* item : allCharts()) item->sample(now, {}, {}, QStringLiteral("Collecting baseline"));
        sampleClock.invalidate();
        diskBaseline = networkBaseline = false;
    }

    QVector<HistoryChart*> allCharts() const
    {
        QVector<HistoryChart*> charts{cpuSummary, memorySummary, diskSummary, networkSummary,
                                     cpuTotal, memoryDetail, swapDetail, diskDetail, diskBusy, networkDetail};
        for (auto* item : cores) charts.append(item);
        for (auto* item : summaryCores) charts.append(item);
        return charts;
    }

    void rebuildCores(const QStringList& names)
    {
        if (names == knownCores) return;
        knownCores = names;
        auto trim = [&](QMap<QString, HistoryChart*>& widgets, const QStringList& wanted) {
            for (auto it = widgets.begin(); it != widgets.end();) {
                if (!wanted.contains(it.key())) { delete it.value(); it = widgets.erase(it); }
                else ++it;
            }
        };
        const auto shown = names.mid(0, 256);
        const auto summary = names.mid(0, 4);
        trim(cores, shown);
        trim(summaryCores, summary);
        for (int i = 0; i < shown.size(); ++i) {
            const auto& name = shown[i];
            if (!cores.contains(name)) {
                auto* item = new HistoryChart(QStringLiteral("Core ") + name.mid(3), QStringLiteral("%"), CpuColor, 100);
                item->setObjectName(QStringLiteral("syspulseCore_") + name);
                item->setLegend(QStringLiteral("60-second history"));
                cores.insert(name, item);
            }
            coreGrid->addWidget(cores[name], i / 2, i % 2);
        }
        for (const auto& name : summary) {
            if (!summaryCores.contains(name)) {
                auto* item = new HistoryChart(QStringLiteral("Core ") + name.mid(3), QStringLiteral("%"), CpuColor, 100, true);
                item->setObjectName(QStringLiteral("syspulseSummaryCore_") + name);
                summaryCores.insert(name, item);
                summaryCoreLayout->addWidget(item, 1);
            }
        }
    }

    void sample()
    {
        if (paused || !enabled) return;
        const auto timestamp = historyClock.nsecsElapsed() / 1e9;
        const auto seconds = sampleClock.isValid() ? sampleClock.nsecsElapsed() / 1e9 : 0.0;
        // Timestamp the start of each snapshot. Restarting after rendering and
        // parsing would omit that work from the rate denominator.
        sampleClock.restart();
        const auto cpu = metrics::parseCpu(readBounded(QStringLiteral("/proc/stat")));
        const auto memory = metrics::parseMemory(readBounded(QStringLiteral("/proc/meminfo")));
        const auto disk = metrics::parseDisks(readBounded(QStringLiteral("/proc/diskstats")));
        const auto network = metrics::parseNetwork(readBounded(QStringLiteral("/proc/net/dev")));
        const bool baseline = seconds > 0;
        const auto names = cpuNames(cpu);
        rebuildCores(names);
        const auto percent = baseline && previousCpu.contains(QStringLiteral("cpu")) && cpu.contains(QStringLiteral("cpu"))
            ? metrics::cpuPercent(previousCpu.value(QStringLiteral("cpu")), cpu.value(QStringLiteral("cpu"))) : std::optional<double>{};
        const auto cpuMissing = cpu.contains(QStringLiteral("cpu")) ? QStringLiteral("Collecting baseline") : QStringLiteral("Unavailable");
        cpuText = percent ? decimal(*percent) + QStringLiteral("%") : cpuMissing;
        cpuSummary->sample(timestamp, percent, {}, cpuMissing);
        cpuTotal->sample(timestamp, percent, {}, cpuMissing);
        const auto cpuDescription = cpu.contains(QStringLiteral("cpu"))
            ? QStringLiteral("%1 logical processors · %2%3").arg(names.size()).arg(cpuText,
                 names.size() > 256 ? QStringLiteral(" · Showing first 256 cores") : QString())
            : QStringLiteral("CPU counters unavailable");
        cpuInfo->setText(cpuDescription);
        cpuInfoSummary->setText(cpuDescription);
        for (const auto& name : names.mid(0, 256)) {
            const auto value = baseline && previousCpu.contains(name) ? metrics::cpuPercent(previousCpu.value(name), cpu.value(name)) : std::optional<double>{};
            cores[name]->sample(timestamp, value, {}, QStringLiteral("Baseline"));
            if (summaryCores.contains(name)) summaryCores[name]->sample(timestamp, value, {}, QStringLiteral("Baseline"));
        }

        if (memory) {
            const auto usedGiB = memory->usedKiB() / 1048576.0;
            const auto totalGiB = memory->totalKiB / 1048576.0;
            memoryText = decimal(usedGiB) + QStringLiteral(" / ") + decimal(totalGiB) + QStringLiteral(" GiB");
            memorySummary->setMaximum(totalGiB);
            memoryDetail->setMaximum(totalGiB);
            memorySummary->sample(timestamp, usedGiB);
            memoryDetail->sample(timestamp, usedGiB);
            const auto percentUsed = 100.0 * memory->usedKiB() / memory->totalKiB;
            memorySummaryInfo->setText(memoryText + QStringLiteral(" · ") + decimal(percentUsed, 0) + QStringLiteral("%"));
            const auto swapText = !memory->swapKnown ? QStringLiteral("Swap counters unavailable")
                : memory->swapTotalKiB == 0 ? QStringLiteral("No swap configured")
                : QStringLiteral("Swap %1 / %2 GiB").arg(decimal(memory->swapUsedKiB / 1048576.0), decimal(memory->swapTotalKiB / 1048576.0));
            memoryInfo->setText(QStringLiteral("In use %1 · Available %2 GiB\nCached %3 GiB · Buffers %4 MiB · Reclaimable slab %5 MiB\n%6")
                .arg(memoryText, decimal(memory->availableKiB / 1048576.0), decimal(memory->cachedKiB / 1048576.0),
                     decimal(memory->buffersKiB / 1024.0), decimal(memory->reclaimableKiB / 1024.0), swapText));
            swapDetail->setMaximum(qMax(0.01, memory->swapTotalKiB / 1048576.0));
            if (memory->swapKnown && memory->swapTotalKiB)
                swapDetail->sample(timestamp, memory->swapUsedKiB / 1048576.0);
            else swapDetail->sample(timestamp, {}, {}, swapText);
        } else {
            memoryText = QStringLiteral("Unavailable");
            memorySummary->sample(timestamp, {}); memoryDetail->sample(timestamp, {}); swapDetail->sample(timestamp, {});
            memoryInfo->setText(QStringLiteral("Memory counters unavailable or incomplete"));
            memorySummaryInfo->setText(QStringLiteral("Memory counters unavailable"));
        }

        QStringList diskNames, baseDisks;
        for (const auto& name : QDir(QStringLiteral("/sys/block")).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (name.startsWith(QStringLiteral("loop")) || name.startsWith(QStringLiteral("ram")) || !disk.contains(name)) continue;
            diskNames.append(name);
            const QDir slaves(QStringLiteral("/sys/block/") + name + QStringLiteral("/slaves"));
            if (slaves.exists() && slaves.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) baseDisks.append(name);
        }
        if (updateChoices(disks, diskNames)) { diskBaseline = false; diskDetail->clear(); diskBusy->clear(); }
        if (updateChoices(networks, network.keys())) { networkBaseline = false; networkDetail->clear(); }

        double totalRead = 0, totalWrite = 0;
        bool disksReady = baseline && !baseDisks.isEmpty();
        for (const auto& name : baseDisks) {
            if (!previousDisks.contains(name)) { disksReady = false; continue; }
            const auto before = previousDisks.value(name), after = disk.value(name);
            const auto read = metrics::rate(before.readSectors, after.readSectors, seconds);
            const auto write = metrics::rate(before.writeSectors, after.writeSectors, seconds);
            if (!read || !write) { disksReady = false; continue; }
            totalRead += metrics::sectorsPerSecondToMiB(*read);
            totalWrite += metrics::sectorsPerSecondToMiB(*write);
        }
        const auto diskMissing = baseDisks.isEmpty() ? QStringLiteral("Unavailable") : QStringLiteral("Collecting baseline");
        diskText = disksReady ? decimal(totalRead + totalWrite) + QStringLiteral(" MiB/s") : diskMissing;
        diskSummary->sample(timestamp, disksReady ? std::optional<double>(totalRead) : std::nullopt,
                            disksReady ? std::optional<double>(totalWrite) : std::nullopt, diskMissing);
        if (disksReady) diskSummary->setStatus(diskText);
        diskSummaryInfo->setText(disksReady ? QStringLiteral("Read %1 · Write %2 MiB/s").arg(decimal(totalRead), decimal(totalWrite))
                                                         : baseDisks.isEmpty() ? QStringLiteral("No base block devices available") : diskMissing);

        const auto selectedDisk = disks->currentText();
        if (!selectedDisk.isEmpty() && disk.contains(selectedDisk)) {
            const auto after = disk.value(selectedDisk);
            const auto before = previousDisks.value(selectedDisk);
            const bool ready = baseline && diskBaseline && previousDisks.contains(selectedDisk);
            const auto read = ready ? metrics::rate(before.readSectors, after.readSectors, seconds) : std::optional<double>{};
            const auto write = ready ? metrics::rate(before.writeSectors, after.writeSectors, seconds) : std::optional<double>{};
            const auto busy = ready ? metrics::rate(before.busyMs, after.busyMs, seconds) : std::optional<double>{};
            const auto readOps = ready ? metrics::rate(before.readOperations, after.readOperations, seconds) : std::optional<double>{};
            const auto writeOps = ready ? metrics::rate(before.writeOperations, after.writeOperations, seconds) : std::optional<double>{};
            const auto readMiB = metrics::sectorsPerSecondToMiB(read.value_or(0.0));
            const auto writeMiB = metrics::sectorsPerSecondToMiB(write.value_or(0.0));
            const auto operations = readOps.value_or(0.0) + writeOps.value_or(0.0);
            const auto busyPercent = qBound(0.0, busy.value_or(0.0) / 10.0, 100.0);
            const auto rotational = readBounded(QStringLiteral("/sys/block/") + selectedDisk + QStringLiteral("/queue/rotational")).trimmed();
            const auto kind = rotational == QStringLiteral("1") ? QStringLiteral("Rotational")
                : rotational == QStringLiteral("0") ? QStringLiteral("Non-rotational / virtual") : QStringLiteral("Type unavailable");
            diskInfo->setText(QStringLiteral("%1 · %2\n%3").arg(selectedDisk, kind,
                read && write ? QStringLiteral("Read %1 MiB/s · Write %2 MiB/s%3")
                    .arg(decimal(readMiB), decimal(writeMiB),
                         readOps && writeOps ? QStringLiteral(" · %1 IOPS").arg(decimal(operations, 0)) : QString())
                    : QStringLiteral("Collecting baseline / counters reset")));
            diskDetail->sample(timestamp, read ? std::optional<double>(readMiB) : std::nullopt,
                               write ? std::optional<double>(writeMiB) : std::nullopt, QStringLiteral("Baseline / reset"));
            diskBusy->sample(timestamp, busy ? std::optional<double>(busyPercent) : std::nullopt, {}, QStringLiteral("Baseline / reset"));
            diskBaseline = true;
        } else {
            diskInfo->setText(QStringLiteral("No block-device counters available"));
            diskDetail->sample(timestamp, {}); diskBusy->sample(timestamp, {}); diskBaseline = false;
        }

        double totalReceive = 0, totalTransmit = 0;
        bool networkReady = baseline;
        int countedInterfaces = 0;
        for (auto it = network.cbegin(); it != network.cend(); ++it) {
            if (it.key() == QStringLiteral("lo")) continue;
            ++countedInterfaces;
            if (!previousNetwork.contains(it.key())) { networkReady = false; continue; }
            const auto before = previousNetwork.value(it.key());
            const auto receive = metrics::rate(before.received, it.value().received, seconds);
            const auto transmit = metrics::rate(before.transmitted, it.value().transmitted, seconds);
            if (!receive || !transmit) { networkReady = false; continue; }
            totalReceive += metrics::bytesPerSecondToKiB(*receive);
            totalTransmit += metrics::bytesPerSecondToKiB(*transmit);
        }
        networkReady = networkReady && countedInterfaces > 0;
        const auto networkMissing = countedInterfaces ? QStringLiteral("Collecting baseline") : QStringLiteral("Unavailable");
        networkText = networkReady ? decimal(totalReceive + totalTransmit) + QStringLiteral(" KiB/s") : networkMissing;
        networkSummary->sample(timestamp, networkReady ? std::optional<double>(totalReceive) : std::nullopt,
                               networkReady ? std::optional<double>(totalTransmit) : std::nullopt, networkMissing);
        if (networkReady) networkSummary->setStatus(networkText);
        networkSummaryInfo->setText(networkReady ? QStringLiteral("Receive %1 · Transmit %2 KiB/s").arg(decimal(totalReceive), decimal(totalTransmit))
                                                               : countedInterfaces ? networkMissing : QStringLiteral("No non-loopback interfaces available"));
        const auto selectedNetwork = networks->currentText();
        if (!selectedNetwork.isEmpty() && network.contains(selectedNetwork)) {
            const auto after = network.value(selectedNetwork);
            const auto before = previousNetwork.value(selectedNetwork);
            const bool ready = baseline && networkBaseline && previousNetwork.contains(selectedNetwork);
            const auto receive = ready ? metrics::rate(before.received, after.received, seconds) : std::optional<double>{};
            const auto transmit = ready ? metrics::rate(before.transmitted, after.transmitted, seconds) : std::optional<double>{};
            const auto receiveKiB = metrics::bytesPerSecondToKiB(receive.value_or(0.0));
            const auto transmitKiB = metrics::bytesPerSecondToKiB(transmit.value_or(0.0));
            const auto state = readBounded(QStringLiteral("/sys/class/net/") + selectedNetwork + QStringLiteral("/operstate")).trimmed();
            networkInfo->setText(QStringLiteral("%1 · %2\n%3\nTotals since interface reset: %4 MiB received · %5 MiB sent")
                .arg(selectedNetwork, state.isEmpty() ? QStringLiteral("State unavailable") : state,
                     receive && transmit ? QStringLiteral("Receive %1 KiB/s · Transmit %2 KiB/s")
                         .arg(decimal(receiveKiB), decimal(transmitKiB))
                         : QStringLiteral("Collecting baseline / counters reset"),
                     decimal(after.received / 1048576.0), decimal(after.transmitted / 1048576.0)));
            networkDetail->sample(timestamp, receive ? std::optional<double>(receiveKiB) : std::nullopt,
                                  transmit ? std::optional<double>(transmitKiB) : std::nullopt, QStringLiteral("Baseline / reset"));
            networkBaseline = true;
        } else {
            networkInfo->setText(QStringLiteral("Network interface counters unavailable"));
            networkDetail->sample(timestamp, {}); networkBaseline = false;
        }

        previousCpu = cpu; previousDisks = disk; previousNetwork = network;
        lastSample = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        sampling->setText(QStringLiteral("Live · Updated %1 · %2-second interval").arg(lastSample).arg(speed->currentData().toInt() / 1000));
        if (owner->onMetricsChanged) owner->onMetricsChanged();
    }

    PerformancePage* owner;
    QTabWidget* tabs;
    QTimer* timer;
    QPushButton* pause;
    QComboBox *speed, *disks, *networks;
    QLabel *sampling, *cpuInfo, *cpuInfoSummary, *memoryInfo, *memorySummaryInfo;
    QLabel *diskInfo, *diskSummaryInfo, *networkInfo, *networkSummaryInfo;
    HistoryChart *cpuSummary, *memorySummary, *diskSummary, *networkSummary;
    HistoryChart *cpuTotal, *memoryDetail, *swapDetail, *diskDetail, *diskBusy, *networkDetail;
    QGridLayout* coreGrid;
    QHBoxLayout* summaryCoreLayout;
    QMap<QString, HistoryChart*> cores, summaryCores;
    QStringList knownCores;
    QMap<QString, metrics::CpuTicks> previousCpu;
    QMap<QString, metrics::DiskCounters> previousDisks;
    QMap<QString, metrics::NetworkCounters> previousNetwork;
    QElapsedTimer historyClock, sampleClock;
    QString cpuText = QStringLiteral("Collecting baseline"), memoryText = QStringLiteral("Unavailable");
    QString diskText = QStringLiteral("Collecting baseline"), networkText = QStringLiteral("Collecting baseline"), lastSample;
    bool paused = false, enabled = true, diskBaseline = false, networkBaseline = false;
};

PerformancePage::PerformancePage(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>(this))
{
    setObjectName(QStringLiteral("syspulsePerformancePage"));
}
PerformancePage::~PerformancePage()
{
    // QObject children outlive C++ members until QWidget teardown. Stop the
    // sampler while its captured implementation and callbacks are still alive.
    impl_->timer->stop();
    QObject::disconnect(impl_->timer, nullptr, this, nullptr);
    onMetricsChanged = {};
    onOpenStartup = {};
    onOpenServices = {};
}
QString PerformancePage::cpuValue() const { return impl_->paused ? QStringLiteral("Paused") : impl_->cpuText; }
QString PerformancePage::memoryValue() const { return impl_->paused ? QStringLiteral("Paused") : impl_->memoryText; }
QString PerformancePage::diskValue() const { return impl_->paused ? QStringLiteral("Paused") : impl_->diskText; }
QString PerformancePage::networkValue() const { return impl_->paused ? QStringLiteral("Paused") : impl_->networkText; }
bool PerformancePage::isPaused() const { return impl_->paused; }
QTabWidget* PerformancePage::detailTabs() const { return impl_->tabs; }

void PerformancePage::setPaused(bool paused)
{
    if (impl_->paused == paused) return;
    impl_->paused = paused;
    impl_->pause->setText(paused ? QStringLiteral("Resume") : QStringLiteral("Pause"));
    impl_->gap();
    if (paused) {
        impl_->timer->stop();
        impl_->sampling->setText(QStringLiteral("Paused · Last sample %1").arg(impl_->lastSample));
        for (auto* chart : impl_->allCharts()) chart->setStatus(QStringLiteral("Paused"));
    } else if (impl_->enabled) {
        impl_->sample();
        impl_->timer->start();
    }
    if (onMetricsChanged) onMetricsChanged();
}

void PerformancePage::setSamplingEnabled(bool enabled)
{
    if (impl_->enabled == enabled) return;
    impl_->enabled = enabled;
    impl_->gap();
    if (enabled && !impl_->paused) { impl_->sample(); impl_->timer->start(); }
    else {
        impl_->timer->stop();
        if (!impl_->paused) impl_->sampling->setText(QStringLiteral("Sampling inactive · Last sample %1").arg(impl_->lastSample));
    }
}

void PerformancePage::refresh() { impl_->sample(); }

} // namespace syspulse
