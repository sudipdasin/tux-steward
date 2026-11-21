// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/health.hpp"
#include <QSet>
#include <stdexcept>
namespace tux {
QStringList healthActions() { return {"storage","memory","devices","failed","kernel","journal"}; }
QStringList healthPreset(const QString& id) {
    if(id=="balanced") return {"storage","memory","devices","failed"};
    if(id=="storage") return {"storage","devices"};
    if(id=="logs") return {"failed","kernel","journal"};
    if(id=="all") return healthActions();
    throw std::invalid_argument("Unknown health preset.");
}
QStringList incompleteHealthActions(const QStringList& plan,const QVector<Result>& results) {
    const auto allowed=healthActions(); QSet<QString> selected,recorded; QStringList incomplete;
    for(const auto& id:plan) {
        if(!allowed.contains(id) || selected.contains(id)) throw std::invalid_argument("Invalid health plan.");
        selected.insert(id);
    }
    QSet<QString> completed;
    for(const auto& result:results) {
        QString matched;
        for(const auto& id:plan) {const auto& tool=action(id); if(tool.title==result.action && tool.command==result.command) matched=id;}
        if(matched.isEmpty() || recorded.contains(matched)) throw std::invalid_argument("Results do not match the reviewed plan.");
        recorded.insert(matched); if(result.code==0 && !result.cancelled) completed.insert(matched);
    }
    for(const auto& id:plan) if(!completed.contains(id)) incomplete.append(id);
    return incomplete;
}
HealthRun runHealthCheck(const QStringList& ids,bool stopOnFailure,int timeoutMs,
                        const std::atomic_bool* cancel,HealthProgress progress) {
    const auto allowed=healthActions(); QSet<QString> seen;
    if(ids.isEmpty() || ids.size()>allowed.size() || timeoutMs<=0)
        throw std::invalid_argument("Select at least one supported diagnostic and a positive time limit.");
    // Validate the entire plan before starting anything, including later entries.
    for(const auto& id:ids) {
        if(!allowed.contains(id) || seen.contains(id)) throw std::invalid_argument("Unsupported or duplicate health diagnostic.");
        const auto& tool=action(id);
        if(tool.changes || tool.admin || tool.x11) throw std::invalid_argument("Health checks require read-only diagnostics.");
        seen.insert(id);
    }
    HealthRun run; run.total=ids.size();
    for(const auto& id:ids) {
        if(cancel && cancel->load(std::memory_order_relaxed)) {run.stopped=true; break;}
        const auto& tool=action(id); if(progress) progress(id,nullptr);
        Result result;
        try { result=execute(id,false,timeoutMs,{},cancel); }
        catch(const std::exception& error) {
            result.action=tool.title; result.command=tool.command; result.code=127; result.output=error.what();
        }
        run.results.append(result); if(progress) progress(id,&run.results.last());
        if(result.cancelled || (cancel && cancel->load(std::memory_order_relaxed))) {run.stopped=true; break;}
        if(stopOnFailure && result.code!=0) {run.stoppedOnFailure=true; break;}
    }
    return run;
}
}
