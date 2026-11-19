// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/core.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
namespace {
void nap(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
bool gone(pid_t pid) { return pid>0 && kill(pid,0)<0 && errno==ESRCH; }
bool reapDescendant(pid_t pid) {
    if(pid<=0) return false;
    for(int n=0;n<100;++n) {
        const auto state=waitpid(pid,nullptr,WNOHANG);
        if(state==pid || (state<0 && errno==ECHILD)) return gone(pid);
        nap(10);
    }
    kill(pid,SIGKILL); waitpid(pid,nullptr,0); return false;
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(argc>1 && QString(argv[1])=="--fixture") {
        const auto leader=getpid(); const auto descendant=fork();
        if(descendant<0) return 90;
        if(descendant==0) {
            nap(150); std::cout<<"late output\n"<<std::flush;
            if(argc>2 && QString(argv[2])=="normal") _exit(0);
            nap(10000); _exit(0);
        }
        std::cout<<"ids "<<leader<<' '<<descendant<<'\n'<<std::flush; return 7;
    }
    // Adopt test grandchildren so every fixture can be reaped, including failures.
    if(prctl(PR_SET_CHILD_SUBREAPER,1)!=0) return 1;
    for(const QString mode:{"normal","timeout","cancel","exception"}) {
        pid_t leader=-1,descendant=-1; bool anchored=false,thrown=false;
        QByteArray captured; std::atomic_bool cancel{false}; tux::Result result;
        auto callback=[&](const QByteArray& chunk) {
            captured+=chunk;
            if(leader<0 && captured.contains('\n')) {
                const auto ids=captured.left(captured.indexOf('\n')).split(' ');
                if(ids.size()==3 && ids[0]=="ids") { leader=ids[1].toLongLong(); descendant=ids[2].toLongLong(); }
            }
            if(chunk.contains("late output")) {
                siginfo_t state{};
                anchored=waitid(P_PID,static_cast<id_t>(leader),&state,WEXITED|WNOHANG|WNOWAIT)==0 && state.si_pid==leader && state.si_code==CLD_EXITED;
                if(mode=="cancel") cancel.store(true);
                if(mode=="exception") throw std::runtime_error("fixture callback");
            }
        };
        QElapsedTimer clock; clock.start();
        try { result=tux::runCommand({app.applicationFilePath(),"--fixture",mode},mode=="timeout"?400:3000,callback,&cancel); }
        catch(const std::runtime_error&) { thrown=true; }
        const bool leaderReaped=gone(leader);
        const bool descendantReaped=reapDescendant(descendant);
        if(!anchored || !leaderReaped || !descendantReaped || clock.elapsed()>2500) { std::cerr<<"Identity/cleanup failed: "<<mode.toStdString()<<'\n'; return 2; }
        if(mode=="normal" && (thrown || result.code!=7 || result.cancelled || !result.output.contains("late output"))) return 3;
        if(mode=="timeout" && (thrown || result.code!=124 || result.cancelled)) return 4;
        if(mode=="cancel" && (thrown || result.code!=130 || !result.cancelled)) return 5;
        if(mode=="exception" && !thrown) return 6;
    }
    std::cout<<"Leader identity retained through inherited output, normal status, timeout, cancellation and callback failure; owned fixtures reaped\n";
}
