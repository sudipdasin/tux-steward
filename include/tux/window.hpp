#pragma once
#include "tux/core.hpp"
#include <QMainWindow>
#include <QVector>
#include <mutex>
#include <memory>
#include <optional>
#include <thread>
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTimer;
class QTextDecoder;
namespace tux {
class Window : public QMainWindow {
public:
    Window();
    ~Window() override;
    int toolCount() const;
protected:
    void closeEvent(QCloseEvent*) override;
private:
    struct Row { int index; QWidget* widget; QPushButton* button; QLabel* state; };
    QVector<Row> rows_;
    QTabWidget* tabs_;
    QLineEdit* search_;
    QLabel* status_;
    QPushButton* historyButton_;
    QPushButton* healthButton_;
    QPushButton* stopButton_;
    std::atomic_bool cancelRequested_{false};
    bool cancellable_=false;
    QLabel* searchStatus_;
    QPlainTextEdit* output_;
    QTimer* timer_;
    bool busy_=false;
    std::thread worker_;
    std::mutex mutex_;
    QByteArray pending_;
    bool skipped_=false;
    std::optional<Result> completed_;
    QVector<Result> history_;
    std::unique_ptr<QTextDecoder> decoder_;
    void filter();
    void review(int index);
    void poll();
    void write(const QString& text);
    void exportReport();
    void showHistory();
    void showHealth();
    void updateButtons();
};
}
