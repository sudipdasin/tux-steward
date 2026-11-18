// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtGlobal>
#include <QString>
#include <sys/types.h>
namespace tux {
struct ProcessIdentity {
    quint64 startTicks=0;
    uid_t realUid=0, effectiveUid=0, savedUid=0, filesystemUid=0;
};
struct SignalOutcome { bool sent=false; bool unavailable=false; QString message; };
bool readProcessIdentity(pid_t pid,ProcessIdentity& identity);
SignalOutcome signalVerifiedProcess(pid_t pid,quint64 expectedStart,uid_t expectedUid,int signal);
}
