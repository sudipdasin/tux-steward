// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QUuid>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace syspulse::storage {
struct Error : std::runtime_error { explicit Error(const char* what) : std::runtime_error(what) {} };
class Fd {
    int fd_ = -1;
public:
    explicit Fd(int fd = -1) : fd_(fd) {}
    ~Fd() { if (fd_ >= 0) ::close(fd_); }
    Fd(const Fd&) = delete; Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    Fd& operator=(Fd&& other) noexcept { if (fd_ >= 0) ::close(fd_); fd_ = other.fd_; other.fd_ = -1; return *this; }
    int get() const { return fd_; }
    explicit operator bool() const { return fd_ >= 0; }
};
inline struct stat info(int fd) { struct stat st{}; if (::fstat(fd, &st)) throw Error("Storage metadata unavailable."); return st; }
inline void ownerDirectory(int fd, bool privateMode = false) {
    const auto st = info(fd);
    if (!S_ISDIR(st.st_mode) || st.st_uid != ::geteuid() || (st.st_mode & (privateMode ? 077 : 022)))
        throw Error("Storage directory ownership or permissions are unsafe.");
}
inline Fd childDirectory(int parent, const QByteArray& name, bool create, bool privateMode = false) {
    if (name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\0')) throw Error("Invalid storage component.");
    if (create && ::mkdirat(parent, name.constData(), 0700) && errno != EEXIST) throw Error("Cannot create private storage.");
    Fd fd(::openat(parent, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (!fd) throw Error("Storage directory is missing or is a symbolic link.");
    ownerDirectory(fd.get(), privateMode); return fd;
}
inline Fd directory(const QString& path, bool create = false, bool privateMode = false) {
    if (::geteuid() == 0 || !QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path || path.contains(QChar::Null))
        throw Error("Use a normal desktop user and an absolute clean storage path.");
    Fd current(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    const auto parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) throw Error("System root cannot be application storage.");
    for (const auto& part : parts) {
        const auto name = QFile::encodeName(part);
        int next = ::openat(current.get(), name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && errno == ENOENT && create) {
            if (::mkdirat(current.get(), name.constData(), 0700)) throw Error("Cannot create storage directory.");
            next = ::openat(current.get(), name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        Fd opened(next); if (!opened) throw Error("Storage path is missing or contains a symbolic link.");
        const auto st = info(opened.get());
        if ((st.st_uid != 0 && st.st_uid != ::geteuid()) || ((st.st_mode & 022) && !(st.st_uid == 0 && (st.st_mode & S_ISVTX))))
            throw Error("Storage ancestor ownership or permissions are unsafe.");
        current = std::move(opened);
    }
    ownerDirectory(current.get(), privateMode); return current;
}
inline void privateFile(const struct stat& st) {
    if (!S_ISREG(st.st_mode) || st.st_uid != ::geteuid() || st.st_nlink != 1 || (st.st_mode & 077))
        throw Error("Private file ownership, link count or permissions are unsafe.");
}
inline QByteArray read(int dir, const char* name, int limit = 2 * 1024 * 1024) {
    Fd fd(::openat(dir, name, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    if (!fd) { if (errno == ENOENT) return {}; throw Error("Cannot open private file."); }
    const auto st = info(fd.get()); privateFile(st);
    if (st.st_size < 0 || st.st_size > limit) throw Error("Private file exceeds its size limit.");
    QByteArray data; char buffer[8192];
    while (true) { const auto size = ::read(fd.get(), buffer, sizeof(buffer)); if (size == 0) break; if (size < 0) { if (errno == EINTR) continue; throw Error("Cannot read private file."); } data.append(buffer, static_cast<int>(size)); if (data.size() > limit) throw Error("Private file grew beyond its size limit."); }
    return data;
}
inline void write(int dir, const char* name, const QByteArray& data) {
    if (data.size() > 2 * 1024 * 1024) throw Error("Private record exceeds its size limit.");
    struct stat existing{};
    if (::fstatat(dir, name, &existing, AT_SYMLINK_NOFOLLOW) == 0) privateFile(existing);
    else if (errno != ENOENT) throw Error("Cannot inspect private record.");
    const auto temporary = QByteArray(".save-") + QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1();
    Fd fd(::openat(dir, temporary.constData(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (!fd) throw Error("Cannot create private record.");
    try {
        qint64 offset = 0;
        while (offset < data.size()) { const auto n = ::write(fd.get(), data.constData() + offset, static_cast<size_t>(data.size() - offset)); if (n < 0) { if (errno == EINTR) continue; throw Error("Cannot write private record."); } if(!n)throw Error("Private record write stopped."); offset += n; }
        if (::fsync(fd.get()) || ::renameat(dir, temporary.constData(), dir, name) || ::fsync(dir)) throw Error("Cannot commit private record.");
    } catch (...) { ::unlinkat(dir, temporary.constData(), 0); throw; }
}
inline Fd lock(int dir) {
    Fd fd(::openat(dir, ".lock", O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (!fd) throw Error("Cannot open private storage lock.");
    privateFile(info(fd.get()));
    if (::flock(fd.get(), LOCK_EX | LOCK_NB)) throw Error("Storage is busy in another operation; try again.");
    return fd;
}
}
