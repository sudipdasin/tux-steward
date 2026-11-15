#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace tux {
inline constexpr auto Version = "0.2.0";
inline constexpr int OutputLimit = 250000;
struct Action {
    QString id, group, title, description;
    QStringList command;
    bool changes=false, admin=false, x11=false;
};
struct Result {
    QString action;
    QStringList command;
    int code=-1;
    QByteArray output;
    bool truncated=false;
    double seconds=0;
    QJsonObject json() const;
};
const QVector<Action>& actions();
const Action& action(const QString& id);
QString unavailable(const Action& tool);
bool matches(const Action& tool, const QString& query);
QString preview(const QStringList& command);
using Output = std::function<void(const QByteArray&)>;
Result runCommand(const QStringList& command, int timeoutMs=120000, Output output={});
Result execute(const QString& id, bool confirmed=false, int timeoutMs=120000, Output output={});
}
