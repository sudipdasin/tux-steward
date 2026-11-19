#include "tux/core.hpp"
#include <QElapsedTimer>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <chrono>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
extern char** environ;

namespace tux {
namespace {
struct Child {
    pid_t pid=-1;
    int fd=-1;
    bool exited=false, identityHeld=true, complete=false;
    ~Child() {
        if (fd>=0) close(fd);
        if (pid>0 && identityHeld) {
            if (!complete) kill(-pid,SIGKILL); // Leader identity remains reserved until this reap.
            for (int n=0;n<100;++n) {
                const auto state=waitpid(pid,nullptr,WNOHANG);
                if (state==pid || (state<0 && errno==ECHILD)) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            // Elevated descendants can deny signals. Do not block the GUI forever.
        }
    }
};
}
Result runCommand(const QStringList& command, int timeoutMs, Output output, const std::atomic_bool* cancel) {
    if (command.isEmpty() || command.first().isEmpty() || timeoutMs<=0)
        throw std::invalid_argument("A command and a positive timeout are required");
    std::vector<QByteArray> bytes;
    for (const auto& arg:command) {
        auto encoded=arg.toUtf8();
        if (encoded.contains('\0')) throw std::invalid_argument("Command arguments cannot contain NUL");
        bytes.push_back(std::move(encoded));
    }
    std::vector<char*> argv;
    for (auto& arg:bytes) argv.push_back(arg.data());
    argv.push_back(nullptr);
    const auto stopping=[&]{return cancel && cancel->load(std::memory_order_relaxed);};
    if(stopping()) {
        Result result; result.command=command; result.code=130; result.cancelled=true;
        result.output="Cancelled before starting the diagnostic.\n";
        if(output) output(result.output);
        return result;
    }
    int original[2];
    if (pipe2(original,O_CLOEXEC)!=0) throw std::runtime_error("Cannot create output pipe");
    // Keep pipe descriptors away from standard streams, even for desktop launchers
    // that start with stdin/stdout closed.
    int pipes[2]={fcntl(original[0],F_DUPFD_CLOEXEC,3),fcntl(original[1],F_DUPFD_CLOEXEC,3)};
    close(original[0]); close(original[1]);
    if (pipes[0]<0 || pipes[1]<0) {
        if (pipes[0]>=0) close(pipes[0]);
        if (pipes[1]>=0) close(pipes[1]);
        throw std::runtime_error("Cannot allocate output descriptors");
    }
    Child child; child.fd=pipes[0];
    const int flags=fcntl(child.fd,F_GETFL);
    if (flags<0 || fcntl(child.fd,F_SETFL,flags|O_NONBLOCK)<0) {
        close(pipes[1]); throw std::runtime_error("Cannot configure output pipe");
    }
    posix_spawn_file_actions_t files;
    posix_spawn_file_actions_init(&files);
    posix_spawn_file_actions_addopen(&files,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    posix_spawn_file_actions_adddup2(&files,pipes[1],STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&files,pipes[1],STDERR_FILENO);
    posix_spawn_file_actions_addclose(&files,pipes[0]);
    posix_spawn_file_actions_addclose(&files,pipes[1]);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes,0);
    QElapsedTimer timer; timer.start();
    const int error=posix_spawnp(&child.pid,argv[0],&files,&attributes,argv.data(),environ);
    posix_spawn_file_actions_destroy(&files);
    posix_spawnattr_destroy(&attributes);
    close(pipes[1]);
    if (error!=0) {
        child.pid=-1;
        throw std::runtime_error(std::string("Cannot start command: ")+std::strerror(error));
    }
    Result result; result.command=command;
    auto deliver=[&](const QByteArray& chunk) {
        result.output.append(chunk);
        if (result.output.size()>OutputLimit) {
            result.truncated=true; result.output=result.output.right(OutputLimit);
        }
        if (output) output(chunk);
    };
    bool eof=false, timeout=false, cancelled=false;
    siginfo_t status{};
    while (!eof || !child.exited) {
        if (!child.exited) {
            // Observe without reaping: a zombie leader reserves its PID/PGID
            // while inherited output is drained and group cleanup may be needed.
            siginfo_t observed{};
            const int state=waitid(P_PID,static_cast<id_t>(child.pid),&observed,WEXITED|WNOHANG|WNOWAIT);
            if (state==0 && observed.si_pid==child.pid) { child.exited=true; status=observed; }
            else if (state<0 && errno!=EINTR) {
                if (errno==ECHILD) child.identityHeld=false; // Never signal after ownership is lost.
                throw std::runtime_error("Cannot read task exit status");
            }
        }
        if (eof && child.exited) break;
        if(stopping()) { cancelled=true; break; }
        const auto remaining=timeoutMs-timer.elapsed();
        if (remaining<=0) { timeout=true; break; }
        pollfd fd{child.fd,POLLIN,0};
        const int ready=poll(eof?nullptr:&fd,eof?0:1,static_cast<int>(std::min<qint64>(remaining,25)));
        if (ready<0 && errno!=EINTR) throw std::runtime_error("Cannot poll task output");
        if (ready>0) {
            char buffer[4096];
            const auto size=read(child.fd,buffer,sizeof(buffer));
            if (size>0) deliver(QByteArray(buffer,static_cast<int>(size)));
            else if (size==0) eof=true;
            else if (errno!=EAGAIN && errno!=EINTR) throw std::runtime_error("Cannot read task output");
        }
    }
    if (cancelled) {
        if(child.identityHeld) kill(-child.pid,SIGKILL);
        deliver("\nCancellation requested. Captured output may be incomplete. Some background processes may remain running.\n");
        result.code=130; result.cancelled=true;
    } else if (timeout) {
        if(child.identityHeld) kill(-child.pid,SIGKILL);
        deliver("\nTimed out. Changes may be partially applied; inspect system state before retrying. Elevated descendants may still be running.\n");
        result.code=124;
    } else {
        result.code=status.si_code==CLD_EXITED?status.si_status:128+status.si_status;
    }
    child.complete=true;
    result.seconds=timer.elapsed()/1000.0;
    return result;
}
}
