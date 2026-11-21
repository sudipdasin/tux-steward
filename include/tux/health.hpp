// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
namespace tux {
// Fixed read-only catalog subset; callers cannot supply arbitrary commands.
QStringList healthActions();
QStringList healthPreset(const QString& id);
QStringList incompleteHealthActions(const QStringList& plan,const QVector<Result>& results);
struct HealthRun {
    QVector<Result> results;
    int total=0;
    bool stopped=false;
    bool stoppedOnFailure=false;
};
using HealthProgress=std::function<void(const QString&,const Result*)>;
HealthRun runHealthCheck(const QStringList& ids,bool stopOnFailure=true,int timeoutMs=30000,
                        const std::atomic_bool* cancel=nullptr,HealthProgress progress={});
}
