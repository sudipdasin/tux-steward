#include "tux/core.hpp"
#include "tux/cli.hpp"
#include "tux/process_manager.hpp"

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

#include <unistd.h>

int main(int argc, char *argv[])
{
    const int information=tux::handleInformationOptions(argc,argv,"Tux Task Manager");
    if (information>=0) return information;
    QApplication application(argc, argv);
    application.setApplicationName("Tux Task Manager");
    application.setApplicationVersion(tux::Version);
    application.setOrganizationName("Tux Steward");
    if (::geteuid() == 0) {
        QMessageBox::critical(nullptr, QObject::tr("Run as a desktop user"),
                              QObject::tr("Tux Task Manager should run as your normal desktop user, not as root."));
        return 1;
    }

    QMainWindow window;
    window.setWindowTitle("Tux Task Manager — CPU · RAM · Disks · Network");
    window.setMinimumSize(620, 420);
    window.resize(1160, 740);
    window.setCentralWidget(new ProcessManagerWidget(&window));
    auto *help=window.menuBar()->addMenu("Help");
    QObject::connect(help->addAction("Send feedback…"), &QAction::triggered, &window, [] {
        QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/tux-steward/issues/new/choose"));
    });
    QObject::connect(help->addAction("Contribute…"), &QAction::triggered, &window, [] {
        QDesktopServices::openUrl(QUrl("https://github.com/sudipdasin/tux-steward/blob/main/CONTRIBUTING.md"));
    });
    window.setStyleSheet(
        "QMainWindow,QWidget{background:#101827;color:#e7eef9;font-family:'Sans Serif';}"
        "#pageTitle{color:#f1f5fb;font-size:27px;font-weight:700;}"
        "#intro{color:#9aacc5;font-size:13px;}"
        "#processTable{background:#192438;border:1px solid #2a3c54;alternate-background-color:#202c3e;}"
        "QHeaderView::section{background:#22334b;color:#dce5f3;border:0;padding:8px;font-weight:600;}"
        "QLineEdit{background:#0b1220;border:1px solid #2a3c54;border-radius:6px;padding:8px 10px;}"
        "QPushButton{background:#2b4359;border:0;border-radius:5px;padding:8px 12px;}"
        "QPushButton:hover{background:#365775;}"
        "#dangerAction{background:#672e35;color:#ffe9e7;font-weight:600;}"
        "#dangerAction:hover{background:#833944;}"
        "#secondaryAction{background:#246b63;color:white;font-weight:600;}"
        "#secondaryAction:hover{background:#2c8278;}"
        "#fileStatus{color:#9aacc5;font-size:12px;}");
    window.show();
    return application.exec();
}

