// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_control.hpp"
#include <QFile>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/syscall.h>
#include <unistd.h>
namespace tux {
bool readProcessIdentity(pid_t pid,ProcessIdentity& identity) {
    if (pid<=0) return false;
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) return false;
    const auto record=stat.read(8193).trimmed();
    const int open=record.indexOf('('), close=record.lastIndexOf(')');
    bool idOk=false, startOk=false;
    if (record.size()>8192 || open<1 || close<=open || record.left(open).trimmed().toLongLong(&idOk)!=pid || !idOk) return false;
    const auto fields=record.mid(close+1).simplified().split(' ');
    if (fields.size()<20) return false;
    identity.startTicks=fields[19].toULongLong(&startOk);
    if (!startOk) return false;
    QFile status(QStringLiteral("/proc/%1/status").arg(pid));
    if (!status.open(QIODevice::ReadOnly)) return false;
    // procfs reports size zero; inspect a bounded prefix instead of relying on atEnd().
    for (const auto &line:status.read(65536).split('\n')) {
        if (!line.startsWith("Uid:")) continue;
        const auto uids=line.mid(4).simplified().split(' ');
        if (uids.size()!=4) return false;
        bool ok[4]={false,false,false,false};
        identity.realUid=uids[0].toUInt(&ok[0]); identity.effectiveUid=uids[1].toUInt(&ok[1]);
        identity.savedUid=uids[2].toUInt(&ok[2]); identity.filesystemUid=uids[3].toUInt(&ok[3]);
        return ok[0] && ok[1] && ok[2] && ok[3];
    }
    return false;
}
SignalOutcome signalVerifiedProcess(pid_t pid,quint64 expectedStart,uid_t expectedUid,int signal) {
    if (pid<=1 || pid==getpid() || expectedUid!=geteuid() || (signal!=SIGTERM && signal!=SIGKILL))
        return {false,false,"This process or signal is protected."};
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
    const int descriptor=static_cast<int>(syscall(SYS_pidfd_open,pid,0));
    if (descriptor<0) {
        const int error=errno;
        return {false,error==ENOSYS || error==EPERM || error==ENODEV,
            error==ESRCH?QStringLiteral("The process has exited."):
            QStringLiteral("A stable process handle is unavailable: %1. No signal was sent.").arg(QString::fromLocal8Bit(std::strerror(error)))};
    }
    struct Handle { int fd; ~Handle(){close(fd);} } handle{descriptor};
    ProcessIdentity current;
    if (!readProcessIdentity(pid,current) || current.startTicks!=expectedStart)
        return {false,false,"Process identity changed or is unavailable. No signal was sent."};
    if (current.realUid!=expectedUid || current.effectiveUid!=expectedUid
        || current.savedUid!=expectedUid || current.filesystemUid!=expectedUid)
        return {false,false,"Only non-elevated processes owned by your account can be stopped."};
    if (syscall(SYS_pidfd_send_signal,descriptor,signal,nullptr,0)!=0) {
        const int error=errno;
        return {false,error==ENOSYS || error==EPERM,
            QStringLiteral("No signal was sent: %1.").arg(QString::fromLocal8Bit(std::strerror(error)))};
    }
    return {true,false,"Signal sent through a verified stable process handle."};
#else
    Q_UNUSED(expectedStart)
    return {false,true,"Safe process signaling is unavailable in this build. No numeric-PID fallback is used."};
#endif
}
}
