// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/health.hpp"
#include <QCoreApplication>
#include <QSet>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    for(const auto& preset:QStringList{"balanced","storage","logs","all"}) {
        const auto ids=tux::healthPreset(preset); QSet<QString> seen;
        if(ids.isEmpty()) return 1;
        for(const auto& id:ids) {if(!tux::healthActions().contains(id) || seen.contains(id) || tux::action(id).admin || tux::action(id).changes) return 2; seen.insert(id);}
    }
    if(tux::healthPreset("storage")!=QStringList{"storage","devices"} || tux::healthPreset("all")!=tux::healthActions()) return 3;
    bool refused=false; try{tux::healthPreset("arbitrary-command");}catch(const std::invalid_argument&){refused=true;} if(!refused) return 4;
    const auto plan=tux::healthPreset("balanced");
    auto result=[](const QString& id,int code){tux::Result r; const auto& tool=tux::action(id);r.action=tool.title;r.command=tool.command;r.code=code;return r;};
    const auto storage=result("storage",0); auto memory=result("memory",42);
    if(tux::incompleteHealthActions(plan,{storage,memory})!=QStringList{"memory","devices","failed"}) return 5;
    if(tux::incompleteHealthActions(plan,{})!=plan || !tux::incompleteHealthActions({},{}).isEmpty()) return 6;
    memory.code=0;memory.cancelled=true;
    if(tux::incompleteHealthActions(plan,{storage,memory}).first()!="memory") return 7;
    memory.cancelled=false;memory.code=124;
    if(tux::incompleteHealthActions(plan,{storage,memory}).first()!="memory") return 8;
    QVector<tux::Result> all; for(const auto& id:plan) all.append(result(id,0));
    if(!tux::incompleteHealthActions(plan,all).isEmpty()) return 9;
    auto altered=storage;altered.command.append("unexpected");
    for(const auto& invalid:QVector<QVector<tux::Result>>{{storage,storage},{altered},{result("journal",0)}}) {
        refused=false;try{tux::incompleteHealthActions(plan,invalid);}catch(const std::invalid_argument&){refused=true;} if(!refused) return 10;
    }
    for(const auto& invalid:QVector<QStringList>{{"storage","storage"},{"storage","apt-clean"},{"unknown"}}) {
        refused=false;try{tux::incompleteHealthActions(invalid,{});}catch(const std::invalid_argument&){refused=true;} if(!refused) return 11;
    }
    std::cout<<"Health recovery: fixed read-only presets, plan-order incomplete selection, cancelled/timeout handling, completed exclusion and mismatched/duplicate result refusal passed\n";
}
