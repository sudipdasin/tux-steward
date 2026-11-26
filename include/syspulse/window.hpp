// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMainWindow>
#include <memory>
namespace syspulse {
QString precisionStyle();
void applyPrecisionTheme();
class Window final : public QMainWindow {
public:
    explicit Window(bool samplingEnabled=true, QWidget* parent=nullptr);
    ~Window() override;
    bool navigate(const QString& id);
    QString currentPageId() const;
    int navigationCount() const;
protected:
    void closeEvent(QCloseEvent*) override;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
}
