// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/core.hpp"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <iostream>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    tux::Result result;
    result.action=tux::action("storage").title;
    result.command=QStringList{"diagnostic", "/home/example/private", "token=fixture-secret"};
    result.output="host=fixture-host user=example /media/private-volume 192.0.2.42 secret=fixture-secret";
    result.code=7; result.seconds=1.25; result.truncated=true;
    const QVector<tux::Result> history={result};
    const auto summary=tux::sessionReport(history);
    const auto records=summary.value("results").toArray();
    const auto row=records.first().toObject();
    if (summary.value("mode").toString()!="summary" || row.size()!=5 || row.contains("output") || row.contains("command") ||
        row.value("action")!=result.action || row.value("code").toInt()!=7 || row.value("seconds").toDouble()!=1.25 || !row.value("truncated").toBool()) return 1;
    if(row.value("cancelled").toBool()) return 8;
    const auto bytes=QJsonDocument(summary).toJson();
    for (const auto &privateValue:{"fixture-host","example","/home/","/media/","192.0.2.42","fixture-secret"})
        if (bytes.contains(privateValue)) return 2;
    const auto detailed=tux::sessionReport(history,true);
    if (detailed.value("mode").toString()!="detailed" || detailed.value("results").toArray().first().toObject()!=result.json()) return 3;
    if (history.first().output!=result.output || history.first().command!=result.command) return 4;
    result.action="/home/example/private label";
    if (tux::sessionReport({result}).value("results").toArray().first().toObject().value("action").toString()!="Unrecognized task") return 5;
    if (!tux::sessionReport({}).value("results").toArray().isEmpty()) return 6;
    if (QJsonDocument::fromJson(bytes).object()!=summary) return 7;
    std::cout<<"Summary allowlist, unknown labels, private fixtures omitted, detailed opt-in and source retention passed\n";
}
