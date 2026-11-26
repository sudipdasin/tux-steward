// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <QJsonArray>
namespace syspulse {
QString historyDirectory();
QJsonArray readHistory(const QString& directory);
void appendHistory(const QString& directory,const tux::Result& result);
QJsonObject historyReport(const QJsonArray& records);
}
