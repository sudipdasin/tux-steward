#include "tux/core.hpp"
#include "tux/trusted_command.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
#include <QTemporaryDir>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
namespace {
int checks=0;
void require(bool ok,const char* text) { ++checks; if (!ok) throw std::runtime_error(text); }
void nap(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
int child(const QString& mode) {
    if (mode=="stream") { write(1,"first\n",6); nap(300); write(1,"last\n",5); }
    else if (mode=="error") { write(2,"failed\n",7); return 7; }
    else if (mode=="flood") { const QByteArray b(4096,'x'); for (int i=0;i<256;++i) write(1,b.data(),b.size()); }
    else if (mode=="timeout") { write(1,"before\n",7); nap(10000); }
    else if (mode=="hold") { if (fork()==0) { nap(10000); _exit(0); } write(1,"parent done\n",12); }
    else if (mode=="closed") { close(1); close(2); nap(10000); }
    else if (mode=="utf8") { write(1,"\xe2",1); nap(30); write(1,"\x82\xac",2); }
    else if (mode=="signal") { raise(SIGTERM); }
    return 0;
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if (QFileInfo(QString::fromLocal8Bit(argv[0])).fileName()=="pkexec") {
        for (int i=1;i<argc;++i) std::cout<<argv[i]<<'\n'; return 0;
    }
    if (argc>2 && QString(argv[1])=="--child") return child(argv[2]);
    if (argc>2 && QString(argv[1])=="--literal") { std::cout<<argv[2]; return 0; }
    const auto self=QCoreApplication::applicationFilePath();
    auto run=[&](const QString& mode,int ms=3000,tux::Output cb=tux::Output{}) {
        return tux::runCommand({self,"--child",mode},ms,std::move(cb));
    };
    try {
        require(tux::actions().size()==35,"25 inherited tools plus ten SysPulse diagnostics retained");
        QSet<QString> ids;
        for (const auto& a:tux::actions()) {
            ids.insert(a.id);
            require(!a.admin || a.changes || a.id=="firewall-status","administrator tools are reviewed mutations or the explicit read-only firewall check");
            if (a.changes || a.admin) {
                bool rejected=false; try { tux::execute(a.id); } catch (const std::exception&) { rejected=true; }
                require(rejected,"mutation confirmation required");
            }
        }
        require(ids.size()==tux::actions().size(),"unique identifiers");
        require(tux::trustedSystemExecutable("../sh").isEmpty()&&tux::trustedSystemExecutable("sh\n").isEmpty(),"privileged executable names reject traversal and trailing newline");
        QTemporaryDir untrustedPath;
        require(untrustedPath.isValid(),"create isolated PATH fixture");
        const auto originalPath=qgetenv("PATH");qputenv("PATH",untrustedPath.path().toUtf8());
        const auto trustedShell=tux::trustedSystemExecutable("sh");
        if(originalPath.isNull())qunsetenv("PATH");else qputenv("PATH",originalPath);
        require(!trustedShell.isEmpty()&&!trustedShell.startsWith(untrustedPath.path()),"privileged command lookup is independent of user PATH");
        require(tux::matches(tux::action("storage"),"STORAGE usage"),"search ignores case");
        require(!tux::matches(tux::action("storage"),"wine"),"search excludes nonmatches");
        require(tux::matches(tux::action("storage"),"  "),"empty search");
        const auto session=qgetenv("XDG_SESSION_TYPE"); qputenv("XDG_SESSION_TYPE","wayland");
        require(tux::unavailable(tux::action("input")).contains("X11"),"Wayland gate");
        if (session.isNull()) qunsetenv("XDG_SESSION_TYPE"); else qputenv("XDG_SESSION_TYPE",session);
        auto r=run("error"); require(r.code==7 && r.output.contains("failed"),"stderr and exit code");
        r=run("flood"); require(r.truncated && r.output.size()==tux::OutputLimit,"bounded flood output");
        QVector<qint64> times; QElapsedTimer timer; timer.start();
        r=run("stream",3000,[&](const QByteArray&){times.append(timer.elapsed());});
        require(r.code==0 && times.size()>=2 && times.last()-times.first()>=200,"output streams before exit");
        r=run("timeout",150); require(r.code==124 && r.output.contains("before") && r.seconds<2,"partial output on timeout");
        r=run("hold",150); require(r.code==124 && r.seconds<2,"child holding pipe deadline");
        r=run("closed",150); require(r.code==124,"closed stdout deadline");
        r=run("utf8"); require(QString::fromUtf8(r.output)==QString::fromUtf8("€"),"split UTF8 retained");
        r=run("signal"); require(r.code==128+SIGTERM,"signal exit reported");
        bool caught=false;
        try { run("timeout",3000,[](const QByteArray&){throw std::runtime_error("callback");}); } catch (...) { caught=true; }
        require(caught,"callback exception propagated and child cleaned");
        r=tux::runCommand({self,"--literal","$(touch should-not-exist); literal"});
        require(r.output=="$(touch should-not-exist); literal","no shell expansion");
        caught=false; try { tux::runCommand({self},0); } catch (...) { caught=true; } require(caught,"positive timeout");
        caught=false; try { tux::runCommand({"/nonexistent-tux-command"}); } catch (...) { caught=true; } require(caught,"spawn failure");
        caught=false; try { tux::action("unknown"); } catch (...) { caught=true; } require(caught,"unknown action");
        auto report=r.json(); require(report.contains("code") && report.contains("command"),"JSON result schema");
        r=tux::execute("storage"); require(r.code==0 && r.output.contains("Filesystem"),"real df diagnostic");
        const auto path=qgetenv("PATH");
        const auto protectedAuthorization=tux::trustedSystemExecutable("pkexec");
        const auto protectedApt=tux::trustedSystemExecutable("apt-get");
        QTemporaryDir temp; require(temp.isValid(),"temporary dependency sandbox");
        QFile::link(self,temp.filePath("pkexec")); QFile::link(self,temp.filePath("apt-get"));
        qputenv("PATH",temp.path().toUtf8());
        require(tux::trustedSystemExecutable("pkexec")==protectedAuthorization&&
                tux::trustedSystemExecutable("apt-get")==protectedApt,
                "user PATH helpers cannot replace protected privileged executables");
        require(!protectedAuthorization.startsWith(temp.path())&&!protectedApt.startsWith(temp.path()),
                "privileged resolution rejects user-controlled helper locations without executing a mutation");
        require(tux::unavailable(tux::action("storage")).contains("Missing dependency"),"missing tool detected");
        qputenv("PATH",path);
        std::cout<<checks<<" checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr<<"FAILED: "<<e.what()<<'\n'; return 1; }
}
