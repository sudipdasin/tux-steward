#include "tux/window.hpp"
#include <QApplication>
#include <cstdio>
#include <unistd.h>
int main(int argc,char** argv) {
    if (geteuid()==0) {
        std::fputs("Run Tux Steward as your desktop user, without sudo. Individual tasks use PolicyKit.\n",stderr);
        return 1;
    }
    QApplication app(argc,argv);
    app.setApplicationName("Tux Steward"); app.setApplicationVersion(tux::Version); app.setOrganizationName("Tux Steward");
    tux::Window window; window.show(); return app.exec();
}
