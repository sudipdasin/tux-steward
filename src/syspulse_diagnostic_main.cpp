// SPDX-License-Identifier: GPL-3.0-or-later
#include "syspulse/history.hpp"
#include "tux/health.hpp"
#include <QCoreApplication>
#include <iostream>
#include <unistd.h>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);app.setApplicationName("SysPulse");app.setApplicationVersion(tux::Version);
    if(argc==2&&QString::fromLocal8Bit(argv[1])=="--version"){std::cout<<"SysPulse diagnostic "<<tux::Version<<'\n';return 0;}
    if(::geteuid()==0){std::cerr<<"Run scheduled diagnostics as a normal user.\n";return 1;}
    try{if(argc!=3||QString::fromLocal8Bit(argv[1])!="--preset")throw std::runtime_error("Use --preset balanced, storage, logs or all.");
        const auto plan=tux::healthPreset(QString::fromLocal8Bit(argv[2]));const auto run=tux::runHealthCheck(plan,false,30000);
        bool ok=true;for(const auto& result:run.results){syspulse::appendHistory(syspulse::historyDirectory(),result);if(result.code||result.cancelled)ok=false;}
        std::cout<<run.results.size()<<" diagnostic summaries saved locally; "<<(ok?"all checks completed":"some checks need attention")<<".\n";return ok?0:2;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
