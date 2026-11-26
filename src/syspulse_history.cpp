// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/history.hpp"
#include "syspulse/private_store.hpp"
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <cmath>
namespace syspulse {
namespace {
QJsonObject clean(const QJsonObject& source) {
    tux::Result result;result.action=source.value("action").toString();result.code=source.value("code").toInt(-1);
    result.seconds=source.value("seconds").toDouble();if(!std::isfinite(result.seconds)||result.seconds<0)result.seconds=0;
    result.cancelled=source.value("cancelled").toBool();result.truncated=source.value("truncated").toBool();
    auto row=tux::sessionReport({result}).value("results").toArray().first().toObject();
    const auto time=QDateTime::fromString(source.value("finished_at").toString(),Qt::ISODate);
    row.insert("finished_at",time.isValid()?time.toUTC().toString(Qt::ISODate):QString("Unknown"));
    return row;
}
QJsonArray decode(const QByteArray& data) {
    if(data.isEmpty())return {};
    QJsonParseError error;auto document=QJsonDocument::fromJson(data,&error);
    if(error.error!=QJsonParseError::NoError||!document.isArray()||document.array().size()>200)throw storage::Error("Saved report history is invalid; it was retained unchanged.");
    QJsonArray rows;for(const auto& row:document.array()){if(!row.isObject())throw storage::Error("Saved history contains an invalid record.");rows.append(clean(row.toObject()));}return rows;
}
}
QString historyDirectory(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/syspulse";}
QJsonArray readHistory(const QString& path) {
    if(!QFileInfo::exists(path)&&!QFileInfo(path).isSymLink())return {};
    auto directory=storage::directory(path,false,true);return decode(storage::read(directory.get(),"history.json"));
}
void appendHistory(const QString& path,const tux::Result& result) {
    auto directory=storage::directory(path,true,true);auto lock=storage::lock(directory.get());
    auto rows=decode(storage::read(directory.get(),"history.json"));
    auto row=result.json();row["finished_at"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    rows.append(clean(row));while(rows.size()>200)rows.removeAt(0);
    storage::write(directory.get(),"history.json",QJsonDocument(rows).toJson(QJsonDocument::Compact));
}
QJsonObject historyReport(const QJsonArray& records) {
    QJsonArray safe;for(const auto& row:records)safe.append(clean(row.toObject()));
    return {{"application","SysPulse"},{"version",tux::Version},{"schema",1},{"mode","saved-summary"},{"records",safe}};
}
}
