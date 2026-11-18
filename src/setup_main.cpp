// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/cli.hpp"
#include "tux/setup_wizard.hpp"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <cstdio>
#include <unistd.h>
int main(int argc,char **argv) {
    const int information=tux::handleInformationOptions(argc,argv,"Tux Steward Setup");
    if (information>=0) return information;
    if (geteuid()==0) { std::fputs("Run Setup as your desktop user, without sudo.\n",stderr);return 1; }
    QApplication app(argc,argv);app.setApplicationName("Tux Steward Setup");app.setApplicationVersion(tux::Version);
    tux::SetupWizard wizard(QDir::cleanPath(QDir(QApplication::applicationDirPath()).absoluteFilePath("..")),QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    wizard.show();return app.exec();
}
