// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace syspulse::schedule {
struct Plan { QString preset="balanced", cadence="daily"; int hour=10, minute=0, weekday=1; bool acOnly=false; };
QString configDirectory();
QString dataDirectory();
QString calendar(const Plan& plan);
QString service(const Plan& plan);
QString timer(const Plan& plan);
bool load(const QString& data,Plan& plan);
void save(const QString& config,const QString& data,const Plan& plan);
}
