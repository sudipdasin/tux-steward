// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tux/core.hpp"
#include <cstdio>
#include <cstring>
namespace tux {
// These information-only options work without a display server, including
// in package verification. They do not bypass root restrictions on the GUI.
inline int handleInformationOptions(int argc,char** argv,const char* application) {
    if (argc!=2) return -1;
    if (std::strcmp(argv[1],"--version")==0) {
        std::printf("%s %s\n",application,Version); return 0;
    }
    if (std::strcmp(argv[1],"--help")==0) {
        std::printf("%s %s\nUsage: %s [--help | --version]\nStart without options to open the desktop interface as a normal user.\n",application,Version,argv[0]);
        return 0;
    }
    return -1;
}
}
