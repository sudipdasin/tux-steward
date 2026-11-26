// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/performance.hpp"
#include "syspulse/window.hpp"
#include "tux/core.hpp"
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>
#include <QTimer>
#include <chrono>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <unistd.h>

namespace {
using Clock = std::chrono::steady_clock;
constexpr int WarmupMs = 2000;
constexpr int DefaultDurationMs = 12000;

QByteArray readProcFile(const char* path) {
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Required process counters are unavailable.");
    const auto contents = file.read(65536);
    if (contents.isEmpty() || file.error() != QFileDevice::NoError)
        throw std::runtime_error("Required process counters could not be read.");
    return contents;
}

quint64 processCpuTicks() {
    const auto contents = readProcFile("/proc/self/stat");
    // comm is parenthesized and can itself contain spaces and parentheses.
    // The last ") " ends comm; the remaining fields begin with state (field 3).
    const int boundary = contents.lastIndexOf(") ");
    if (boundary < 0) throw std::runtime_error("Process CPU counter format is invalid.");
    const auto fields = contents.mid(boundary + 2).simplified().split(' ');
    if (fields.size() < 13) throw std::runtime_error("Process CPU counters are incomplete.");
    bool userOk = false, systemOk = false;
    const auto user = fields[11].toULongLong(&userOk);  // stat field 14
    const auto system = fields[12].toULongLong(&systemOk); // stat field 15
    if (!userOk || !systemOk || std::numeric_limits<quint64>::max() - user < system)
        throw std::runtime_error("Process CPU counters are invalid.");
    // Include all threads in this process, but not child processes or helpers.
    return user + system;
}

quint64 residentKiB() {
    const auto contents = readProcFile("/proc/self/status");
    for (const auto& line : contents.split('\n')) {
        if (!line.startsWith("VmRSS:")) continue;
        const auto fields = line.simplified().split(' ');
        bool ok = false;
        const auto value = fields.value(1).toULongLong(&ok);
        if (ok && fields.size() == 3 && fields[2] == "kB") return value;
        throw std::runtime_error("Process resident-memory counter format is invalid.");
    }
    throw std::runtime_error("Process resident-memory counter is unavailable.");
}

class TimerCounter final : public QObject {
public:
    bool measuring = false;
    QObject* stopTimer = nullptr;
    quint64 timerEvents = 0;
    quint64 stopTimerEvents = 0;
protected:
    bool eventFilter(QObject* receiver, QEvent* event) override {
        if (measuring && event->type() == QEvent::Timer) {
            ++timerEvents;
            if (receiver == stopTimer) ++stopTimerEvents;
        }
        return false;
    }
};

void printUsage() {
    std::fputs(
        "Usage: syspulse-idle-probe [--duration-ms N] [--paused]\n"
        "                            [--source-label LABEL] [--output FILE]\n\n"
        "Shows the real Overview, warms up for 2000 ms, then measures a real\n"
        "QApplication event loop (default 12000 ms; allowed 1000..120000 ms).\n"
        "--paused pauses the embedded resource sampler; it is live otherwise.\n"
        "Use QT_QPA_PLATFORM to choose the same Qt renderer for compared runs.\n"
        "JSON is written to stdout and optionally to an owner-only output file.\n"
        "Run benchmarks only in the project's isolated validation VM.\n",
        stdout);
}
}

int main(int argc, char** argv) {
    // Permit usage without loading a GUI plugin or opening a display.
    if (argc == 2) {
        const auto argument = QByteArray(argv[1]);
        if (argument == "--help" || argument == "-h") { printUsage(); return 0; }
    }

    QApplication app(argc, argv);
    app.setApplicationName("SysPulse idle probe");
    app.setApplicationVersion(tux::Version);
    app.setQuitOnLastWindowClosed(false);
    QCommandLineParser parser;
    parser.setApplicationDescription("Native SysPulse idle event-loop measurement; run only in a validation VM.");
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList{"duration-ms"}, "Measured duration after a 2000 ms warmup.", "milliseconds", QString::number(DefaultDurationMs)));
    parser.addOption(QCommandLineOption(QStringList{"paused"}, "Pause the embedded Performance resource sampler."));
    parser.addOption(QCommandLineOption(QStringList{"source-label"}, "Caller-provided commit or comparison label; stored as JSON data.", "label", "unspecified"));
    parser.addOption(QCommandLineOption(QStringList{"output"}, "Also save the JSON result to an owner-only file.", "file"));
    parser.process(app);
    if (!parser.positionalArguments().isEmpty()) {
        std::fputs("Idle probe: unexpected positional arguments; use --help.\n", stderr);
        return 2;
    }
    bool durationOk = false;
    const int durationMs = parser.value("duration-ms").toInt(&durationOk);
    if (!durationOk || durationMs < 1000 || durationMs > 120000) {
        std::fputs("Idle probe: --duration-ms must be an integer from 1000 through 120000.\n", stderr);
        return 2;
    }
    const auto sourceLabel = parser.value("source-label");
    if (sourceLabel.size() > 256) {
        std::fputs("Idle probe: --source-label must be at most 256 characters.\n", stderr);
        return 2;
    }
    if (parser.isSet("output") && parser.value("output").isEmpty()) {
        std::fputs("Idle probe: --output requires a nonempty filename.\n", stderr);
        return 2;
    }
    const long ticksPerSecond = ::sysconf(_SC_CLK_TCK);
    if (ticksPerSecond <= 0) {
        std::fputs("Idle probe: the process CPU tick frequency is unavailable.\n", stderr);
        return 2;
    }

    syspulse::applyPrecisionTheme();
    syspulse::Window window(true);
    window.resize(1366, 768);
    if (!window.navigate("overview")) {
        std::fputs("Idle probe: the Overview workspace is unavailable.\n", stderr);
        return 2;
    }
    auto* performance = dynamic_cast<syspulse::PerformancePage*>(
        window.findChild<QWidget*>("syspulsePerformancePage"));
    if (!performance) {
        std::fputs("Idle probe: the embedded Performance sampler was not found.\n", stderr);
        return 2;
    }
    performance->setPaused(parser.isSet("paused"));
    window.show();

    TimerCounter counter;
    QTimer warmupTimer;
    QTimer stopTimer;
    warmupTimer.setSingleShot(true);
    warmupTimer.setTimerType(Qt::PreciseTimer);
    stopTimer.setSingleShot(true);
    stopTimer.setTimerType(Qt::PreciseTimer);
    counter.stopTimer = &stopTimer;
    app.installEventFilter(&counter);
    quint64 startCpuTicks = 0;
    quint64 startRssKiB = 0;
    Clock::time_point startWall;

    QObject::connect(&warmupTimer, &QTimer::timeout, &app, [&] {
        try {
            if (!window.isVisible() || window.currentPageId() != "overview")
                throw std::runtime_error("The Overview window is not visible at measurement start.");
            startCpuTicks = processCpuTicks();
            startRssKiB = residentKiB();
            startWall = Clock::now();
            counter.measuring = true;
            stopTimer.start(durationMs);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Idle probe: %s\n", error.what());
            app.exit(2);
        }
    });
    QObject::connect(&stopTimer, &QTimer::timeout, &app, [&] {
        counter.measuring = false;
        try {
            const auto endWall = Clock::now();
            const auto endCpuTicks = processCpuTicks();
            const auto endRssKiB = residentKiB();
            const double seconds = std::chrono::duration<double>(endWall - startWall).count();
            if (seconds <= 0.0 || endCpuTicks < startCpuTicks)
                throw std::runtime_error("The elapsed duration or process CPU counters are invalid.");
            if (!window.isVisible() || window.currentPageId() != "overview")
                throw std::runtime_error("The Overview window changed during measurement.");
            if (counter.stopTimerEvents != 1)
                throw std::runtime_error("The probe stop-timer event accounting is invalid.");
            const auto ticks = endCpuTicks - startCpuTicks;
            const double cpuSeconds = static_cast<double>(ticks) / static_cast<double>(ticksPerSecond);
            const auto applicationEvents = counter.timerEvents - counter.stopTimerEvents;
            const QJsonObject result{
                {"schema", 1},
                {"probe", "syspulse-native-idle"},
                {"application_version", tux::Version},
                {"source_label", sourceLabel},
                {"mode", parser.isSet("paused") ? "paused" : "live"},
                {"workspace", "overview"},
                {"renderer", QGuiApplication::platformName()},
                {"qt_version", qVersion()},
                {"warmup_ms", WarmupMs},
                {"requested_duration_ms", durationMs},
                {"elapsed_seconds", seconds},
                {"cpu_tick_hz", static_cast<double>(ticksPerSecond)},
                {"cpu_ticks_delta", static_cast<double>(ticks)},
                {"process_cpu_seconds", cpuSeconds},
                {"cpu_fraction_one_core", cpuSeconds / seconds},
                {"cpu_percent_one_core", 100.0 * cpuSeconds / seconds},
                {"rss_start_kib", static_cast<double>(startRssKiB)},
                {"rss_end_kib", static_cast<double>(endRssKiB)},
                {"rss_endpoint_max_kib", static_cast<double>(qMax(startRssKiB, endRssKiB))},
                {"rss_measurement", "VmRSS endpoints in KiB (1024 bytes), not peak RSS"},
                {"timer_events_total", static_cast<double>(counter.timerEvents)},
                {"probe_stop_timer_events", static_cast<double>(counter.stopTimerEvents)},
                {"application_timer_events", static_cast<double>(applicationEvents)},
                {"application_timer_events_per_second", static_cast<double>(applicationEvents) / seconds},
                {"timer_measurement", "Delivered QEvent::Timer events on the QApplication thread; own stop timer subtracted"}
            };
            const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Indented);
            if (parser.isSet("output")) {
                QSaveFile output(parser.value("output"));
                if (!output.open(QIODevice::WriteOnly) ||
                    !output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
                    output.write(bytes) != bytes.size() || !output.commit())
                    throw std::runtime_error("The JSON output file could not be saved.");
            }
            if (std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout) != static_cast<size_t>(bytes.size()) ||
                std::fflush(stdout) != 0)
                throw std::runtime_error("The JSON result could not be written to standard output.");
            app.exit(0);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Idle probe: %s\n", error.what());
            app.exit(2);
        }
    });
    warmupTimer.start(WarmupMs);
    // One real GUI event loop. No processEvents polling, sleeps, timer-driven
    // sampling loops, child-process probes, or maintenance operations are used.
    return app.exec();
}
