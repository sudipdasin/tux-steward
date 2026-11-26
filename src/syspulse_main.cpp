// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/window.hpp"
#include "tux/cli.hpp"
#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <cstdio>
#include <unistd.h>
int main(int argc,char** argv) {
    const int info=tux::handleInformationOptions(argc,argv,"SysPulse");
    if(info>=0) return info;
    if(::geteuid()==0) {
        std::fputs("Run SysPulse as your desktop user without sudo. Individual system actions request authorization.\n",stderr);
        return 1;
    }
    QApplication app(argc,argv);
    app.setApplicationName("SysPulse");
    app.setApplicationVersion(tux::Version);
    app.setOrganizationName("SysPulse");
    syspulse::applyPrecisionTheme();
    QPixmap mark(64,64);mark.fill(Qt::transparent);
    { QPainter p(&mark);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#C8102E"),5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
      p.drawPolyline(QPolygonF(QVector<QPointF>{{4,34},{17,34},{24,10},{34,55},{42,26},{47,34},{60,34}})); }
    app.setWindowIcon(QIcon(mark));
    syspulse::Window window;
    window.show();
    return app.exec();
}
