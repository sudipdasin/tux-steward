// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace tux::setup {
struct Outcome { bool ok; QString message; };
QString installRoot(const QString &dataHome);
Outcome install(const QString &sourcePrefix,const QString &dataHome);
Outcome remove(const QString &dataHome);
}
