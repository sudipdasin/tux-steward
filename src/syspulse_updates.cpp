// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/updates.hpp"
#include "tux/core.hpp"
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>
#include <optional>
#include <tuple>

namespace syspulse {
namespace {
struct Version {
    int major, minor, patch, stage, sequence;
    auto key() const { return std::make_tuple(major, minor, patch, stage, sequence); }
};
std::optional<Version> version(const QString& text) {
    static const QRegularExpression pattern(QStringLiteral(
        "\\Av?(0|[1-9][0-9]{0,8})\\.(0|[1-9][0-9]{0,8})\\.(0|[1-9][0-9]{0,8})(?:-(alpha|beta|rc)\\.(0|[1-9][0-9]{0,8}))?\\z"));
    const auto match = pattern.match(text);
    if (!match.hasMatch()) return {};
    const auto stage = match.captured(4);
    return Version{match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt(),
                   stage.isEmpty() ? 3 : stage == "rc" ? 2 : stage == "beta" ? 1 : 0,
                   match.captured(5).toInt()};
}
}
UpdateOffer evaluateReleaseFeed(const QByteArray& body, const QString& current) {
    UpdateOffer offer;
    const auto installed = version(current);
    if (!installed || body.size() > ReleaseFeedLimit) {
        offer.error = "The installed version or response size is invalid."; return offer;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray() || document.array().size() > 20) {
        offer.error = "The release service did not return the expected bounded release list."; return offer;
    }
    std::optional<Version> best;
    for (const auto& value : document.array()) {
        if (!value.isObject()) continue;
        const auto object = value.toObject();
        const auto tag = object.value("tag_name").toString();
        const auto candidate = version(tag);
        if (!tag.startsWith('v') || !candidate || !object.value("draft").isBool() || object.value("draft").toBool()
            || !object.value("prerelease").isBool()
            || object.value("prerelease").toBool() != (candidate->stage != 3)
            || !QDateTime::fromString(object.value("published_at").toString(), Qt::ISODate).isValid()) continue;
        const auto expected = QStringLiteral("https://github.com/sudipdasin/tux-steward/releases/tag/") + tag;
        if (object.value("html_url").toString() != expected) continue;
        ++offer.considered;
        // A stable installation never offers a testing build by default.
        if (installed->stage == 3 && candidate->stage != 3) continue;
        if (!best || candidate->key() > best->key()) {
            best = candidate; offer.tag = tag; offer.url = QUrl(expected);
        }
    }
    if (!document.array().isEmpty() && !offer.considered) {
        offer.error = "No valid published project releases were present."; return offer;
    }
    offer.valid = true;
    offer.newer = best && best->key() > installed->key();
    return offer;
}

UpdatePanel::UpdatePanel(QWidget* parent) : UpdatePanel(nullptr, 10000, parent) {}
UpdatePanel::UpdatePanel(QNetworkAccessManager* transport, int deadlineMs, QWidget* parent) : QWidget(parent) {
    setObjectName("updatePanel");
    transport_ = transport;
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0, 12, 0, 0);
    auto* card = new QFrame; card->setObjectName("panel"); outer->addWidget(card); outer->addStretch(1);
    auto* layout = new QVBoxLayout(card); layout->setContentsMargins(22, 20, 22, 20); layout->setSpacing(12);
    auto* title = new QLabel("Application updates"); title->setObjectName("sectionTitle"); layout->addWidget(title);
    auto* notice = new QLabel("Check manually for newer published SysPulse releases. This contacts the public GitHub release API only when you click Check. No diagnostics are uploaded. Packages are not downloaded or installed automatically.");
    notice->setTextFormat(Qt::PlainText); notice->setWordWrap(true); layout->addWidget(notice);
    status_ = new QLabel(QString("Installed: %1 · Not checked in this session").arg(tux::Version));
    status_->setObjectName("updateStatus"); status_->setTextFormat(Qt::PlainText); status_->setWordWrap(true); layout->addWidget(status_);
    auto* row = new QHBoxLayout;
    check_ = new QPushButton("Check for updates"); check_->setObjectName("checkForUpdates"); check_->setProperty("primary", true); row->addWidget(check_);
    cancel_ = new QPushButton("Cancel check"); cancel_->setObjectName("cancelUpdateCheck"); cancel_->setEnabled(false); row->addWidget(cancel_);
    release_ = new QPushButton("Review release notes…"); release_->setObjectName("reviewUpdateRelease"); release_->setEnabled(false); row->addWidget(release_); row->addStretch(); layout->addLayout(row);
    deadline_ = new QTimer(this); deadline_->setSingleShot(true); deadline_->setInterval(qBound(1, deadlineMs, 30000));
    connect(check_, &QPushButton::clicked, this, [this] { check(); });
    connect(cancel_, &QPushButton::clicked, this, [this] { stop("Update check cancelled. No package was changed."); });
    connect(deadline_, &QTimer::timeout, this, [this] { stop("Update check timed out. Check your connection and try again."); });
    connect(release_, &QPushButton::clicked, this, [this] {
        if (!releaseUrl_.isEmpty() && !QDesktopServices::openUrl(releaseUrl_))
            status_->setText("The browser could not open the release page. Retry from the project homepage.");
    });
}
UpdatePanel::~UpdatePanel() { if (reply_) { disconnect(reply_, nullptr, this, nullptr); reply_->abort(); reply_->deleteLater(); } }
bool UpdatePanel::isChecking() const { return !reply_.isNull(); }
void UpdatePanel::stop(const QString& message) {
    auto* old = reply_.data(); reply_.clear(); deadline_->stop();
    if (old) { old->abort(); old->deleteLater(); }
    body_.clear(); releaseUrl_.clear(); release_->setEnabled(false);
    check_->setEnabled(true); cancel_->setEnabled(false); status_->setText(message);
}
bool UpdatePanel::consume(QNetworkReply* reply) {
    if (reply_.data() != reply) return false;
    if (reply->bytesAvailable() > ReleaseFeedLimit - body_.size()) {
        stop("Update response exceeded the size limit. No package was changed."); return false;
    }
    const auto chunk = reply->read(ReleaseFeedLimit - body_.size() + 1);
    if (chunk.size() > ReleaseFeedLimit - body_.size()) {
        stop("Update response exceeded the size limit. No package was changed."); return false;
    }
    body_ += chunk; return true;
}
void UpdatePanel::check() {
    if (reply_) return;
    body_.clear(); releaseUrl_.clear(); release_->setEnabled(false);
    check_->setEnabled(false); cancel_->setEnabled(true); status_->setText("Checking up to 20 recent published project releases…");
    QNetworkRequest request(QUrl("https://api.github.com/repos/sudipdasin/tux-steward/releases?per_page=20"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "SysPulse-Manual-Release-Check");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!transport_) transport_ = new QNetworkAccessManager(this);
    auto* reply = transport_->get(request); reply_ = reply; reply->setReadBufferSize(64 * 1024);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] { consume(reply); });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply_.data() != reply) { reply->deleteLater(); return; }
        if (!consume(reply)) return;
        deadline_->stop();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            stop(QString("Update check failed (HTTP %1). No package was changed. Try again later.").arg(status)); return;
        }
        const auto offer = evaluateReleaseFeed(body_, tux::Version);
        reply_.clear(); reply->deleteLater(); body_.clear(); check_->setEnabled(true); cancel_->setEnabled(false);
        if (!offer.valid) { status_->setText(offer.error); return; }
        if (offer.newer) {
            releaseUrl_ = offer.url; release_->setEnabled(true);
            status_->setText(QString("Newer release: %1 · Installed: %2. Review its compatibility, checksums and release notes before installing.").arg(offer.tag, tux::Version));
        } else status_->setText(QString("No newer eligible release found among %1 published project releases. Installed: %2.").arg(offer.considered).arg(tux::Version));
    });
    deadline_->start();
}
}
