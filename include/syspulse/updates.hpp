// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QPointer>
#include <QUrl>
#include <QWidget>
class QLabel;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
namespace syspulse {
inline constexpr int ReleaseFeedLimit = 512 * 1024;
struct UpdateOffer {
    bool valid = false;
    bool newer = false;
    QString tag, error;
    QUrl url;
    int considered = 0;
};
// Parse public metadata only. Never downloads or executes a package.
UpdateOffer evaluateReleaseFeed(const QByteArray& body, const QString& current);
class UpdatePanel final : public QWidget {
public:
    explicit UpdatePanel(QWidget* parent = nullptr);
    // Explicit transport/deadline injection for deterministic isolated-VM tests.
    UpdatePanel(QNetworkAccessManager* transport, int deadlineMs, QWidget* parent = nullptr);
    ~UpdatePanel() override;
    bool isChecking() const;
private:
    QNetworkAccessManager* transport_ = nullptr;
    QPointer<QNetworkReply> reply_;
    QTimer* deadline_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* check_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QPushButton* release_ = nullptr;
    QByteArray body_;
    QUrl releaseUrl_;
    void check();
    void stop(const QString& message);
    bool consume(QNetworkReply* reply);
};
}
