#include "tux/window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <thread>
#include <chrono>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QTableWidget>
#include <iostream>
int main(int argc,char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    tux::Window window; window.show(); app.processEvents();
    auto* tabs=window.findChild<QTabWidget*>("toolTabs");
    auto* search=window.findChild<QLineEdit*>("toolSearch");
    if (window.toolCount()!=tux::actions().size() || !tabs || tabs->count()!=8 || !search) return 1;
    int experimental=-1;
    for (int i=0;i<tabs->count();++i) if (tabs->tabText(i)=="Experimental") experimental=i;
    if (experimental<0 || !tabs->widget(experimental)->isAncestorOf(window.findChild<QPushButton *>("tool_apt-clean"))) return 20;
    if (tabs->widget(experimental)->isAncestorOf(window.findChild<QPushButton *>("tool_storage"))) return 21;
    search->setText("Vulkan"); app.processEvents();
    int visible=0;
    for (auto* button:window.findChildren<QPushButton*>())
        if (button->objectName().startsWith("tool_") && !button->parentWidget()->isHidden()) ++visible;
    if (visible!=1) return 2;
    // Search must reveal results in another tab, rather than leaving an empty page.
    if (!tabs->tabText(tabs->currentIndex()).startsWith("Gaming (1)") ||
        !window.findChild<QLabel*>("searchStatus")->text().startsWith("1 matching")) return 22;
    search->setText("no-tool-has-this-name"); app.processEvents();
    if (!window.findChild<QLabel*>("searchStatus")->text().startsWith("No matching")) return 23;
    search->clear(); app.processEvents();
    if (tabs->tabText(experimental)!="Experimental") return 24;
    tabs->setCurrentIndex(0);
    bool cancelled=false;
    QTimer::singleShot(0,[&]{
        for (auto* widget:QApplication::topLevelWidgets()) {
            if (auto* box=qobject_cast<QMessageBox*>(widget)) {
                cancelled=box->defaultButton()==box->button(QMessageBox::Cancel);
                box->reject();
            }
        }
    });
    window.findChild<QPushButton*>("tool_storage")->click();
    if (!cancelled || !window.findChild<QLabel*>("taskStatus")->text().startsWith("Ready")) return 3;
    // done(Ok) represents the button result, rather than QDialog::Accepted.
    QTimer::singleShot(0,[&]{
        for (auto* widget:QApplication::topLevelWidgets())
            if (auto* box=qobject_cast<QMessageBox*>(widget)) box->done(QMessageBox::Ok);
    });
    window.findChild<QPushButton*>("tool_storage")->click();
    auto* status=window.findChild<QLabel*>("taskStatus");
    QElapsedTimer deadline; deadline.start();
    while (status->text().startsWith("Running") && deadline.elapsed()<5000) {
        app.processEvents(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto* output=window.findChild<QPlainTextEdit*>("sessionOutput");
    if (!status->text().startsWith("Completed") || !output->toPlainText().contains("Filesystem")) return 5;
    bool reviewedAgain=false;
    if(window.findChild<QPushButton*>("sessionHistory")->text()!="History (1)") return 26;
    QTimer::singleShot(0,[&]{
        for(auto* widget:QApplication::topLevelWidgets()) if(widget->objectName()=="taskHistory") {
            QTimer::singleShot(0,[&]{
                for(auto* candidate:QApplication::topLevelWidgets()) if(auto* box=qobject_cast<QMessageBox*>(candidate)) {
                    reviewedAgain=box->defaultButton()==box->button(QMessageBox::Cancel); box->reject();
                }
            });
            widget->findChild<QPushButton*>("historyRerun")->click(); break;
        }
    });
    window.findChild<QPushButton*>("sessionHistory")->click();
    if(!reviewedAgain || status->text().startsWith("Running") || window.findChild<QPushButton*>("sessionHistory")->text()!="History (1)") return 27;
    bool healthReviewed=false;
    QTimer::singleShot(0,[&]{for(auto* widget:QApplication::topLevelWidgets()) if(widget->objectName()=="healthCheckDialog") {
        healthReviewed=widget->findChild<QPlainTextEdit*>("healthCommands")->toPlainText().contains("'df'");
        qobject_cast<QDialog*>(widget)->reject();
    }});
    window.findChild<QPushButton*>("healthCheck")->click();
    if(!healthReviewed || window.findChild<QPushButton*>("sessionHistory")->text()!="History (1)") return 28;
    QTemporaryDir temp;
    const auto report=temp.filePath("report.json");
    bool reviewed=false;
    QTimer::singleShot(0,[&]{
        for (auto* widget:QApplication::topLevelWidgets()) {
            if (widget->objectName()!="reportReview") continue;
            auto* details=widget->findChild<QCheckBox*>("reportDetails");
            auto* preview=widget->findChild<QPlainTextEdit*>("reportPreview");
            const auto summary=preview->toPlainText();
            reviewed=!details->isChecked() && summary.contains("summary") && !summary.contains("Filesystem");
            details->setChecked(true);
            reviewed=reviewed && preview->toPlainText().contains("Filesystem");
            details->setChecked(false);
            reviewed=reviewed && preview->toPlainText()==summary;
            if (argc>1) reviewed=reviewed && widget->grab().save(QString::fromLocal8Bit(argv[1])+".report.png");
            QTimer::singleShot(0,[&]{
                for (auto* candidate:QApplication::topLevelWidgets())
                    if (auto* dialog=qobject_cast<QFileDialog*>(candidate)) {
                        dialog->selectFile(report); QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);
                    }
            });
            widget->findChild<QPushButton*>("reportSave")->click();
            break;
        }
    });
    for (auto* item:window.findChildren<QAction*>())
        if (item->text().startsWith("Export session")) { item->trigger(); break; }
    QFile saved(report);
    if (!reviewed || !saved.open(QIODevice::ReadOnly)) return 6;
    const auto records=QJsonDocument::fromJson(saved.readAll()).object().value("results").toArray();
    if (records.size()!=1 || records.first().toObject().value("code").toInt()!=0 ||
        records.first().toObject().contains("output") || records.first().toObject().contains("command")) return 7;
    // Cancelling a detailed preview must not remember consent or overwrite the saved summary.
    QTimer::singleShot(0,[&]{
        for (auto* widget:QApplication::topLevelWidgets()) if (widget->objectName()=="reportReview") {
            widget->findChild<QCheckBox*>("reportDetails")->setChecked(true);
            qobject_cast<QDialog*>(widget)->reject(); break;
        }
    });
    for (auto* item:window.findChildren<QAction*>())
        if (item->text().startsWith("Export session")) { item->trigger(); break; }
    bool reset=false;
    QTimer::singleShot(0,[&]{
        for (auto* widget:QApplication::topLevelWidgets()) if (widget->objectName()=="reportReview") {
            reset=!widget->findChild<QCheckBox*>("reportDetails")->isChecked();
            qobject_cast<QDialog*>(widget)->reject(); break;
        }
    });
    for (auto* item:window.findChildren<QAction*>())
        if (item->text().startsWith("Export session")) { item->trigger(); break; }
    if (!reset) return 25;
    output->setPlainText("Report exported locally. Review diagnostics before sharing.\n");
    for (int i=0;i<tabs->count();++i) { tabs->setCurrentIndex(i); app.processEvents(); }
    tabs->setCurrentIndex(0); app.processEvents();
    if (argc>1 && !window.grab().save(QString::fromLocal8Bit(argv[1]))) return 4;
    window.close();
    std::cout<<"GUI: complete inherited and SysPulse tool catalog, eight legacy tabs, cross-tab search, default-cancel review, real diagnostic completion, JSON export and render passed\n";
    return 0;
}

