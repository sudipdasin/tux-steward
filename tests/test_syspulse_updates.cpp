// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/updates.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void require(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
QJsonObject release(const QString& tag) {
    return {{"tag_name", tag}, {"html_url", "https://github.com/sudipdasin/tux-steward/releases/tag/" + tag},
            {"draft", false}, {"prerelease", tag.contains('-')}, {"published_at", "2026-11-26T10:00:00Z"}};
}
QByteArray feed(const QJsonArray& releases) { return QJsonDocument(releases).toJson(QJsonDocument::Compact); }
void until(const std::function<bool()>& done) {
    QElapsedTimer timer; timer.start();
    while (!done() && timer.elapsed() < 1500) { qApp->processEvents(); QThread::msleep(1); }
    require(done(), "asynchronous check resolves within test deadline");
}
struct Response { QByteArray body; int status = 200; bool wait = false; };
class Reply final : public QNetworkReply {
public:
    Reply(const QNetworkRequest& request, Response response, QObject* parent)
        : QNetworkReply(parent), response_(std::move(response)) {
        setRequest(request); setUrl(request.url()); setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response_.status); open(QIODevice::ReadOnly);
        if (!response_.wait) QTimer::singleShot(0, this, [this] {
            emit readyRead(); if (!isFinished()) { setFinished(true); emit finished(); }
        });
    }
    void abort() override { if (!isFinished()) { setError(OperationCanceledError, "cancelled"); setFinished(true); emit finished(); } }
    qint64 bytesAvailable() const override { return response_.body.size() - offset_ + QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char* destination, qint64 limit) override {
        const auto count = qMin<qint64>(limit, response_.body.size() - offset_);
        if (!count) return -1;
        std::memcpy(destination, response_.body.constData() + offset_, static_cast<size_t>(count)); offset_ += count; return count;
    }
private:
    Response response_; qint64 offset_ = 0;
};
class Manager final : public QNetworkAccessManager {
public:
    Response next; int requests = 0; QNetworkRequest observed;
protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request, QIODevice*) override {
        require(operation == GetOperation, "update check is a GET without upload body");
        ++requests; observed = request; return new Reply(request, next, this);
    }
};
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        auto offer = syspulse::evaluateReleaseFeed(feed({release("v3.0.0-alpha.10"), release("v3.0.0-alpha.9")}), "3.0.0-alpha.9");
        require(offer.valid && offer.newer && offer.tag == "v3.0.0-alpha.10", "numeric prerelease ordering, independent of feed order");
        offer = syspulse::evaluateReleaseFeed(feed({release("v3.0.0-beta.1"), release("v3.0.0-rc.1"), release("v3.0.0")}), "3.0.0-alpha.99");
        require(offer.newer && offer.tag == "v3.0.0", "stable outranks alpha, beta and release candidate");
        offer = syspulse::evaluateReleaseFeed(feed({release("v4.0.0-alpha.1"), release("v3.0.0")}), "3.0.0");
        require(offer.valid && !offer.newer, "stable installations do not offer a testing channel");
        offer = syspulse::evaluateReleaseFeed(feed({release("v2.9.9")}), "3.0.0-alpha.1");
        require(offer.valid && !offer.newer, "never offer a downgrade");
        for (const auto& url : {"http://github.com/sudipdasin/tux-steward/releases/tag/v9.0.0", "https://github.com.evil.test/sudipdasin/tux-steward/releases/tag/v9.0.0", "https://github.com/other/repo/releases/tag/v9.0.0", "https://github.com/sudipdasin/tux-steward/releases/tag/v9.0.0?token=private"}) {
            auto item = release("v9.0.0"); item["html_url"] = url;
            require(!syspulse::evaluateReleaseFeed(feed({item}), "3.0.0").valid, "reject a foreign, insecure or decorated release destination");
        }
        for (const auto& key : {"draft", "prerelease", "published_at", "tag_name"}) {
            auto item = release("v9.0.0"); item.remove(key);
            require(!syspulse::evaluateReleaseFeed(feed({item}), "3.0.0").valid, "missing publication identity cannot authorize an update offer");
        }
        auto draft = release("v9.0.0"); draft["draft"] = true;
        require(!syspulse::evaluateReleaseFeed(feed({draft}), "3.0.0").valid, "draft is not a published update");
        for (const auto& tag : {"v03.0.0", "v3.0.0-alpha.01", "v99999999999.0.0", "v3.0.0;command", "v3.0.0-unknown.1"})
            require(!syspulse::evaluateReleaseFeed(feed({release(tag)}), "3.0.0").valid, "reject malformed or unsupported version syntax");
        require(!syspulse::evaluateReleaseFeed("{\"message\":\"rate limit\"}", "3.0.0").valid, "API error is not an up-to-date result");
        require(!syspulse::evaluateReleaseFeed(QByteArray(syspulse::ReleaseFeedLimit + 1, ' '), "3.0.0").valid, "oversize metadata rejected");
        syspulse::UpdatePanel neverChecked;
        require(neverChecked.findChildren<QNetworkAccessManager*>().isEmpty(), "unrequested update check does not create a network manager");
        Manager manager;
        syspulse::UpdatePanel panel(&manager, 30);
        require(manager.requests == 0 && !panel.isChecking(), "constructing an update panel never contacts a service");
        auto* check = panel.findChild<QPushButton*>("checkForUpdates");
        auto* cancel = panel.findChild<QPushButton*>("cancelUpdateCheck");
        auto* review = panel.findChild<QPushButton*>("reviewUpdateRelease");
        auto* status = panel.findChild<QLabel*>("updateStatus");
        require(check && cancel && review && status && !review->isEnabled(), "no release link before a valid manual check");
        manager.next = {feed({release("v9.0.0")})}; check->click(); until([&] { return !panel.isChecking(); });
        require(manager.requests == 1 && review->isEnabled() && status->text().contains("v9.0.0"), "manual asynchronous check produces a reviewable offer");
        require(manager.observed.url() == QUrl("https://api.github.com/repos/sudipdasin/tux-steward/releases?per_page=20"), "fixed project endpoint with bounded page size");
        require(manager.observed.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt() == QNetworkRequest::ManualRedirectPolicy, "redirects are not followed");
        require(!manager.observed.hasRawHeader("Authorization"), "public check does not transmit credentials");
        manager.next = {"{}", 403}; check->click(); until([&] { return !panel.isChecking(); });
        require(!review->isEnabled() && status->text().contains("403"), "rate limit/failure clears the stale offer");
        manager.next = {QByteArray(syspulse::ReleaseFeedLimit + 1, 'x')}; check->click(); until([&] { return !panel.isChecking(); });
        require(status->text().contains("size limit") && !review->isEnabled(), "streaming body limit stops transport and clears offer");
        manager.next = {{}, 200, true}; check->click(); require(panel.isChecking(), "pending reply remains cancellable");
        cancel->click(); require(!panel.isChecking() && status->text().contains("cancelled"), "cancel exits pending check normally");
        check->click(); until([&] { return !panel.isChecking(); });
        require(status->text().contains("timed out") && check->isEnabled(), "absolute request deadline and retry admission");
        manager.next = {feed({release("v9.0.0")}), 302}; check->click(); until([&] { return !panel.isChecking(); });
        require(!review->isEnabled() && status->text().contains("302"), "redirect response cannot present an offer");
        for (const auto* timer : panel.findChildren<QTimer*>()) require(!timer->isActive(), "idle update panel has no background timer");
        std::cout << checks << " manual update identity/channel/transport/privacy/lifecycle checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
