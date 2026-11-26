#include "tux/core.hpp"
#include "tux/trusted_command.hpp"
#include <QJsonArray>
#include <QRegularExpression>
#include <QStandardPaths>
#include <stdexcept>

namespace tux {
const QVector<Action>& actions() {
    static const QVector<Action> catalog = {
        {"storage", "Storage", "Storage usage", "Review mounted filesystems and free space.", {"df", "-hT"}, false, false, false},
        {"devices", "Storage", "Disks & filesystems", "Inspect devices, rotation type, mount points and filesystem types.", {"lsblk", "-o", "NAME,SIZE,ROTA,FSTYPE,MOUNTPOINTS"}, false, false, false},
        {"mounts", "Storage", "Mount map", "Inspect loop devices and filesystem mounts before planning repairs.", {"findmnt"}, false, false, false},
        {"trim-check", "Storage", "SSD trim schedule", "Inspect the system SSD trim timer.", {"systemctl", "status", "fstrim.timer", "--no-pager"}, false, false, false},
        {"trim", "Storage", "Trim SSDs", "Reclaim unused blocks on supported mounted SSD filesystems. Requires administrator authentication.", {"fstrim", "--all", "--verbose"}, true, true, false},
        {"failed", "Diagnostics", "Failed services", "Inspect failed system services.", {"systemctl", "--failed", "--no-pager"}, false, false, false},
        {"kernel", "Diagnostics", "Kernel warnings", "Read the latest 150 kernel warnings from this boot. Access depends on local journal permissions.", {"journalctl", "-k", "-b", "-p", "warning", "-n", "150", "--no-pager"}, false, false, false},
        {"journal", "Diagnostics", "System warnings", "Read the latest 200 warnings and errors.", {"journalctl", "-b", "-p", "warning", "-n", "200", "--no-pager"}, false, false, false},
        {"journal-size", "Maintenance", "Journal disk usage", "Measure journal storage before deciding whether to prune.", {"journalctl", "--disk-usage"}, false, false, false},
        {"apt-preview", "Maintenance", "Preview unused packages", "Simulate APT autoremove. No packages are removed.", {"apt-get", "--simulate", "autoremove"}, false, false, false},
        {"apt-clean", "Maintenance", "Clean downloaded packages", "Delete cached APT package downloads. Installed applications remain available.", {"apt-get", "clean"}, true, true, false},
        {"journal-prune", "Maintenance", "Retain seven days of logs", "Permanently remove archived journal files older than seven days. Export needed diagnostics first.", {"journalctl", "--vacuum-time=7d"}, true, true, false},
        {"timers", "Schedules", "Scheduled system tasks", "Review timers including trim and package maintenance.", {"systemctl", "list-timers", "--all", "--no-pager"}, false, false, false},
        {"user-timers", "Schedules", "Your scheduled tasks", "Inspect timers in the current user session.", {"systemctl", "--user", "list-timers", "--all", "--no-pager"}, false, false, false},
        {"trim-enable", "Schedules", "Enable weekly SSD trim", "Enable and start the distribution-provided fstrim timer. Undo: sudo systemctl disable --now fstrim.timer", {"systemctl", "enable", "--now", "fstrim.timer"}, true, true, false},
        {"memory", "Performance", "Memory & swap", "Inspect RAM and swap usage before tuning.", {"free", "-h"}, false, false, false},
        {"zram", "Performance", "Compressed swap status", "Inspect active zRAM devices.", {"zramctl"}, false, false, false},
        {"power", "Performance", "Power management status", "Inspect TLP if installed. No packages are installed automatically.", {"tlp-stat", "-s"}, false, false, false},
        {"compositor-on", "Performance", "Enable XFCE compositor", "Enable desktop compositing. Reversible using Disable compositor.", {"xfconf-query", "-c", "xfwm4", "-p", "/general/use_compositing", "-s", "true"}, true, false, false},
        {"compositor-off", "Performance", "Disable XFCE compositor", "May reduce rendering overhead; can cause tearing. Reversible using Enable compositor.", {"xfconf-query", "-c", "xfwm4", "-p", "/general/use_compositing", "-s", "false"}, true, false, false},
        {"input", "Input", "List X11 input devices", "Inspect exact device IDs. X11 only; permanent keyboard disabling is intentionally deferred.", {"xinput", "list"}, false, false, true},
        {"wine", "Gaming", "Wine version", "Check whether Wine is installed.", {"wine", "--version"}, false, false, false},
        {"lutris", "Gaming", "Lutris version", "Check whether Lutris is installed.", {"lutris", "--version"}, false, false, false},
        {"vulkan", "Gaming", "Vulkan capabilities", "Inspect graphics driver support.", {"vulkaninfo", "--summary"}, false, false, false},
        {"arch", "Gaming", "32-bit architectures", "Inspect enabled foreign package architectures.", {"dpkg", "--print-foreign-architectures"}, false, false, false},
        {"network-links", "Network", "Network links", "Inspect link state and capabilities. Details stay local unless explicitly exported.", {"ip", "-brief", "link"}, false, false, false},
        {"network-addresses", "Network", "Network addresses", "Inspect local interface addresses. Review private details before sharing.", {"ip", "-brief", "address"}, false, false, false},
        {"network-routes", "Network", "Network routes", "Inspect the local route table without changing network configuration.", {"ip", "route", "show"}, false, false, false},
        {"firewall-status", "Security", "Firewall status", "Read UFW status and rules with administrator authorization. No rules are changed.", {"ufw", "status", "verbose"}, false, true, false},
        {"available-updates", "Security", "Available package updates", "List updates from the existing APT cache. Does not contact repositories or install packages.", {"apt", "list", "--upgradable"}, false, false, false},
        {"package-audit", "Maintenance", "Package health", "Inspect incomplete package installations without modifying packages.", {"dpkg", "--audit"}, false, false, false},
        {"desktop-channels", "Desktop", "XFCE configuration channels", "List available XFCE configuration channels without changing settings.", {"xfconf-query", "--list"}, false, false, false},
        {"filesystem-config", "Storage", "Filesystem configuration check", "Read and verify the mount configuration. Does not repair or remount filesystems.", {"findmnt", "--verify", "--verbose"}, false, false, false},
        {"session-state", "Desktop", "Desktop session state", "Inspect the current login session. Output may contain private local details.", {"loginctl", "session-status", "--no-pager"}, false, false, false},
        {"boot-time", "Diagnostics", "Boot timing", "Inspect measured system boot timing when systemd provides it.", {"systemd-analyze", "time"}, false, false, false},
    };
    return catalog;
}
const Action& action(const QString& id) {
    for (const auto& tool : actions()) if (tool.id==id) return tool;
    throw std::invalid_argument("Unknown tool identifier");
}
QString unavailable(const Action& tool) {
    if (tool.x11 && qEnvironmentVariable("XDG_SESSION_TYPE")=="wayland")
        return "Requires X11; XWayland cannot control native Wayland devices.";
    if ((tool.admin?trustedSystemExecutable(tool.command.first()):QStandardPaths::findExecutable(tool.command.first())).isEmpty())
        return "Missing dependency: "+tool.command.first();
    if (tool.admin && trustedSystemExecutable("pkexec").isEmpty())
        return "Missing dependency: pkexec (PolicyKit authentication agent required)";
    return {};
}
bool matches(const Action& tool, const QString& query) {
    const auto text=tool.title+" "+tool.group+" "+tool.description;
    for (const auto& word:query.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts))
        if (!text.contains(word,Qt::CaseInsensitive)) return false;
    return true;
}
QString preview(const QStringList& command) {
    QStringList quoted;
    for (auto arg:command) {
        arg.replace("'", "'\\''");
        quoted.append("'"+arg+"'");
    }
    return quoted.join(' '); // Display only. Never evaluated by a shell.
}
Result execute(const QString& id, bool confirmed, int timeoutMs, Output output, const std::atomic_bool* cancel) {
    const auto& tool=action(id);
    if ((tool.changes || tool.admin) && !confirmed) throw std::runtime_error("Review and confirm this system action first.");
    const auto problem=unavailable(tool);
    if (!problem.isEmpty()) throw std::runtime_error(problem.toStdString());
    auto command=tool.command;
    if (tool.admin) {
        const auto executable=trustedSystemExecutable(command.first());
        const auto authorization=trustedSystemExecutable("pkexec");
        if(executable.isEmpty()||authorization.isEmpty())throw std::runtime_error("A trusted system executable is no longer available.");
        command[0]=executable;
        command.prepend(authorization);
    }
    // Cancellation is deliberately unavailable to settings-changing/admin tasks.
    auto result=runCommand(command,timeoutMs,std::move(output),(!tool.changes && !tool.admin)?cancel:nullptr);
    result.action=tool.title;
    return result;
}
QJsonObject Result::json() const {
    return {{"action",action},{"command",QJsonArray::fromStringList(command)},
            {"code",code},{"output",QString::fromUtf8(output)},
            {"truncated",truncated},{"seconds",seconds},{"cancelled",cancelled}};
}
QJsonObject sessionReport(const QVector<Result>& results, bool includeDetails) {
    QJsonArray records;
    for (const auto& result:results) {
        if (includeDetails) { records.append(result.json()); continue; }
        QString title="Unrecognized task";
        for (const auto& tool:actions()) if (tool.title==result.action) { title=tool.title; break; }
        records.append(QJsonObject{{"action",title},{"code",result.code},
            {"seconds",result.seconds},{"truncated",result.truncated},{"cancelled",result.cancelled}});
    }
    return {{"version",Version},{"schema",1},{"mode",includeDetails?"detailed":"summary"},
        {"results",records}};
}
}
