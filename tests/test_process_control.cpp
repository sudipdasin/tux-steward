// SPDX-License-Identifier: GPL-3.0-or-later
#include "tux/process_control.hpp"
#include <QCoreApplication>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
namespace {
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Child {
    pid_t pid=-1;
    Child() {
        pid=fork();
        if (pid==0) { prctl(PR_SET_NAME,"fixture ) child",0,0,0); for (;;) pause(); }
        if (pid<0) throw std::runtime_error("fork failed");
    }
    ~Child() { if (pid>0) { kill(pid,SIGKILL); waitpid(pid,nullptr,0); } }
};
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        tux::ProcessIdentity self;
        require(tux::readProcessIdentity(getpid(),self),"read process identity");
        require(!tux::signalVerifiedProcess(getpid(),self.startTicks,geteuid(),SIGTERM).sent,"self protected");
        require(!tux::signalVerifiedProcess(1,0,geteuid(),SIGTERM).sent,"PID 1 protected");
        require(!tux::signalVerifiedProcess(-1,0,geteuid(),SIGKILL).sent,"process group signaling refused");
        Child child;
        tux::ProcessIdentity identity;
        require(tux::readProcessIdentity(child.pid,identity),"read child identity");
        require(!tux::signalVerifiedProcess(child.pid,identity.startTicks,geteuid(),SIGSTOP).sent,"unsupported signal protected");
        require(!tux::signalVerifiedProcess(child.pid,identity.startTicks,geteuid()+1,SIGTERM).sent,"wrong owner refused");
        auto stale=tux::signalVerifiedProcess(child.pid,identity.startTicks+1,geteuid(),SIGTERM);
        require(!stale.sent && kill(child.pid,0)==0,"stale start identity never signals child");
        const auto result=tux::signalVerifiedProcess(child.pid,identity.startTicks,geteuid(),SIGTERM);
        if (result.unavailable) {
            require(!result.sent && kill(child.pid,0)==0,"unavailable pidfd path fails closed");
            std::cout<<"Stable signaling unavailable in this environment; protected refusal verified. Actual delivery skipped.\n";
            return 0;
        }
        require(result.sent,"send TERM via stable process descriptor");
        int status=0;
        require(waitpid(child.pid,&status,0)==child.pid && WIFSIGNALED(status) && WTERMSIG(status)==SIGTERM,"owned child received TERM");
        const auto exited=child.pid; child.pid=-1;
        require(!tux::signalVerifiedProcess(exited,identity.startTicks,geteuid(),SIGTERM).sent,"exited target refused");
        Child forced; require(tux::readProcessIdentity(forced.pid,identity),"read force-stop child identity");
        require(tux::signalVerifiedProcess(forced.pid,identity.startTicks,geteuid(),SIGKILL).sent,"send KILL via stable descriptor");
        require(waitpid(forced.pid,&status,0)==forced.pid && WIFSIGNALED(status) && WTERMSIG(status)==SIGKILL,"owned child received KILL");
        forced.pid=-1;
        std::cout<<"Process identity, protected targets, stale PID refusal and real owned-child pidfd signaling passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<"FAILED: "<<error.what()<<'\n'; return 1; }
}
