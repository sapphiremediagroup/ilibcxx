#include <cstddef.hpp>
#include <cstdint.hpp>
#include <cstring.hpp>
#include <memory.hpp>
#include <cstdlib.hpp>
#include <cstdio.hpp>
#include <cmath.hpp>
#include <cstdarg.hpp>
#include <stdarg.h>
#include <syscall.hpp>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/select.h>
#include <termios.h>
#include <utime.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static FILE __stdin_file = { 0, 0, 0 };
static FILE __stdout_file = { 1, 0, 0 };
static FILE __stderr_file = { 2, 0, 0 };

extern "C" int _fltused = 0;
extern "C" void __chkstk() {}

extern "C" FILE* stdin = &__stdin_file;
extern "C" FILE* stdout = &__stdout_file;
extern "C" FILE* stderr = &__stderr_file;

static unsigned long __rand_state = 1;
static mode_t __process_umask = 022;

static std::Handle __terminal_console = static_cast<std::Handle>(-2);
static constexpr int kMaxPosixFd = 1024;
static constexpr int kMaxTrackedMappings = 64;
static constexpr std::uint64_t kFileHandleTag = 1ULL << 48;
static std::Handle __fd_handles[kMaxPosixFd] = {};
static bool __fd_handle_valid[kMaxPosixFd] = {};
static bool __signal_restart_enabled = false;

struct TrackedMapping {
    void* addr;
    size_t length;
    int fd;
    off_t offset;
    bool sharedWritable;
};

static TrackedMapping __tracked_mappings[kMaxTrackedMappings] = {};

static bool syscall_failed(std::uint64_t result) {
    return result >= static_cast<std::uint64_t>(-4095);
}

static int syscall_errno(std::uint64_t result, int fallback) {
    if (!syscall_failed(result)) {
        return 0;
    }
    if (result == static_cast<std::uint64_t>(-1)) {
        return fallback;
    }

    const int decoded = -static_cast<int>(static_cast<std::int64_t>(result));
    return decoded > 0 ? decoded : fallback;
}

static int syscall_fail(std::uint64_t result, int fallback) {
    errno = syscall_errno(result, fallback);
    return -1;
}

static std::Handle get_terminal_console() {
    if (__terminal_console != static_cast<std::Handle>(-2)) {
        return __terminal_console;
    }
    __terminal_console = std::service_connect("terminal.console");
    return __terminal_console;
}

static bool is_stdio_fd(int fd) {
    return fd >= 0 && fd <= 2;
}

enum ConsoleOp : std::uint8_t {
    ConsoleOpWrite = 1,
    ConsoleOpRead = 2,
    ConsoleOpGetAttr = 3,
    ConsoleOpSetAttr = 4,
    ConsoleOpIsATTY = 5,
    ConsoleOpGetPgrp = 6,
    ConsoleOpSetPgrp = 7
};

static int terminal_request(std::uint8_t op, const void* payload, std::uint64_t payloadSize,
                            void* reply, std::uint64_t replyCapacity, std::uint64_t* replySize) {
    const std::Handle console = get_terminal_console();
    if (console == static_cast<std::Handle>(-1)) {
        errno = ENOTTY;
        return -1;
    }
    if (payloadSize > sizeof(std::IPCMessage::data) - 1) {
        errno = EINVAL;
        return -1;
    }

    std::IPCMessage request = {};
    request.flags = std::IPC_MESSAGE_REQUEST;
    request.size = 1 + payloadSize;
    request.data[0] = op;
    if (payload != nullptr && payloadSize != 0) {
        std::memcpy(request.data + 1, payload, static_cast<std::size_t>(payloadSize));
    }

    std::uint64_t actualReplySize = 0;
    const auto rc = std::queue_request(console, &request, reply, replyCapacity, &actualReplySize);
    if (syscall_failed(rc)) {
        errno = ENOTTY;
        return -1;
    }
    if (replySize != nullptr) {
        *replySize = actualReplySize;
    }
    return 0;
}

static bool valid_posix_fd(int fd) {
    return fd >= 0 && fd < kMaxPosixFd;
}

static std::Handle synthesize_file_handle(int fd) {
    return kFileHandleTag | static_cast<std::uint64_t>(static_cast<unsigned int>(fd));
}

static std::Handle fd_to_handle(int fd) {
    if (fd >= 0 && fd <= 2) {
        return static_cast<std::Handle>(fd);
    }
    if (!valid_posix_fd(fd)) {
        return static_cast<std::Handle>(-1);
    }
    if (__fd_handle_valid[fd]) {
        return __fd_handles[fd];
    }
    return synthesize_file_handle(fd);
}

static int register_file_handle(std::Handle handle) {
    if (syscall_failed(handle)) {
        return -1;
    }

    const int fd = static_cast<int>(handle & 0xFFFFFFFFULL);
    if (!valid_posix_fd(fd) || fd < 3) {
        errno = EMFILE;
        return -1;
    }

    __fd_handles[fd] = handle;
    __fd_handle_valid[fd] = true;
    return fd;
}

static void unregister_file_handle(int fd) {
    if (valid_posix_fd(fd)) {
        __fd_handles[fd] = 0;
        __fd_handle_valid[fd] = false;
    }
}

static inline float fast_atanf(float x) {
    const float pi = 3.14159265358979323846f;
    float ax = x < 0.0f ? -x : x;
    return (pi * 0.25f) * x - x * (ax - 1.0f) * (0.2447f + 0.0663f * ax);
}

extern "C" {

void* malloc(size_t size) noexcept {
    return std::malloc(size);
}

void free(void* ptr) noexcept {
    std::free(ptr);
}

void* realloc(void* ptr, size_t size) noexcept {
    return std::realloc(ptr, size);
}

void* calloc(size_t nmemb, size_t size) noexcept {
    return std::calloc(nmemb, size);
}

int open(const char* path, int flags, ...) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }

    int mode = 0666;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
        mode &= ~static_cast<int>(__process_umask);
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Open),
        reinterpret_cast<std::uint64_t>(path),
        static_cast<std::uint64_t>(flags),
        static_cast<std::uint64_t>(mode)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return register_file_handle(result);
}

ssize_t read(int fd, void* buf, size_t count) {
    if (buf == nullptr && count != 0) {
        errno = EINVAL;
        return -1;
    }

    if (fd == 0) {
        const std::Handle console = get_terminal_console();
        if (console != static_cast<std::Handle>(-1)) {
            std::IPCMessage request = {};
            request.flags = std::IPC_MESSAGE_REQUEST;
            request.size = 1 + sizeof(std::uint64_t);
            request.data[0] = ConsoleOpRead;
            const std::uint64_t want = static_cast<std::uint64_t>(count);
            std::memcpy(request.data + 1, &want, sizeof(want));

            std::uint64_t replySize = 0;
            const auto rc = std::queue_request(console, &request, buf, count, &replySize);
            if (!syscall_failed(rc)) {
                return static_cast<ssize_t>(replySize);
            }
        }
    }

    for (;;) {
        const auto result = _syscall_impl(
            static_cast<std::uint64_t>(std::Syscall::Read),
            fd_to_handle(fd),
            reinterpret_cast<std::uint64_t>(buf),
            count
        );
        if (syscall_failed(result)) {
            const int err = syscall_errno(result, EBADF);
            if (err == EINTR && __signal_restart_enabled) {
                continue;
            }
            errno = err;
            return -1;
        }
        return static_cast<ssize_t>(result);
    }
}

ssize_t write(int fd, const void* buf, size_t count) {
    if (buf == nullptr && count != 0) {
        errno = EINVAL;
        return -1;
    }

    if (fd == 1 || fd == 2) {
        const std::Handle console = get_terminal_console();
        if (console != static_cast<std::Handle>(-1)) {
            const std::uint8_t* bytes = static_cast<const std::uint8_t*>(buf);
            std::uint64_t total = 0;
            while (total < count) {
                std::IPCMessage message = {};
                message.flags = std::IPC_MESSAGE_EVENT;

                const std::uint64_t remaining = count - total;
                const std::uint64_t chunk = remaining > 254 ? 254 : remaining;
                message.size = 2 + chunk;
                message.data[0] = ConsoleOpWrite;
                message.data[1] = static_cast<std::uint8_t>(fd);
                std::memcpy(message.data + 2, bytes + total, static_cast<std::size_t>(chunk));

                if (syscall_failed(std::queue_send(console, &message, true))) {
                    break;
                }
                total += chunk;
            }

            if (total == count) {
                return static_cast<ssize_t>(count);
            }
        }
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Write),
        fd_to_handle(fd),
        reinterpret_cast<std::uint64_t>(buf),
        count
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }
    return static_cast<ssize_t>(result);
}

int close(int fd) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Close),
        fd_to_handle(fd)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }
    unregister_file_handle(fd);
    return 0;
}

int dup(int fd) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Dup),
        fd_to_handle(fd)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }
    return register_file_handle(result);
}

int dup2(int oldfd, int newfd) {
    if (!valid_posix_fd(newfd) || newfd < 3) {
        errno = EBADF;
        return -1;
    }
    if (oldfd == newfd) {
        const auto check = _syscall_impl(
            static_cast<std::uint64_t>(std::Syscall::Fcntl),
            fd_to_handle(oldfd),
            static_cast<std::uint64_t>(F_GETFD),
            0
        );
        if (syscall_failed(check)) {
            return syscall_fail(check, EBADF);
        }
        return newfd;
    }

    const std::Handle newHandle = synthesize_file_handle(newfd);
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Dup2),
        fd_to_handle(oldfd),
        newHandle
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }

    __fd_handles[newfd] = newHandle;
    __fd_handle_valid[newfd] = true;
    return newfd;
}

int pipe(int fildes[2]) {
    if (fildes == nullptr) {
        errno = EINVAL;
        return -1;
    }

    std::Handle handles[2] = {};
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Pipe),
        reinterpret_cast<std::uint64_t>(handles)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EMFILE);
    }

    const int readFd = register_file_handle(handles[0]);
    const int writeFd = register_file_handle(handles[1]);
    if (readFd < 0 || writeFd < 0) {
        if (readFd >= 0) {
            close(readFd);
        }
        if (writeFd >= 0) {
            close(writeFd);
        }
        errno = EMFILE;
        return -1;
    }

    fildes[0] = readFd;
    fildes[1] = writeFd;
    return 0;
}

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    if (nfds > static_cast<nfds_t>(kMaxPosixFd)) {
        errno = EINVAL;
        return -1;
    }
    if (fds == nullptr && nfds != 0) {
        errno = EFAULT;
        return -1;
    }

    std::PollFD* kernelFds = nullptr;
    if (nfds != 0) {
        kernelFds = static_cast<std::PollFD*>(std::malloc(sizeof(std::PollFD) * nfds));
        if (kernelFds == nullptr) {
            errno = ENOMEM;
            return -1;
        }
    }

    int elapsed = 0;
    for (;;) {
        for (nfds_t i = 0; i < nfds; ++i) {
            if (fds[i].fd < 0) {
                kernelFds[i].fd = -1;
            } else if (!valid_posix_fd(fds[i].fd)) {
                kernelFds[i].fd = fds[i].fd;
            } else {
                kernelFds[i].fd = static_cast<std::int64_t>(fd_to_handle(fds[i].fd));
            }
            kernelFds[i].events = fds[i].events;
            kernelFds[i].revents = 0;
            fds[i].revents = 0;
        }

        const auto result = _syscall_impl(
            static_cast<std::uint64_t>(std::Syscall::Poll),
            reinterpret_cast<std::uint64_t>(kernelFds),
            static_cast<std::uint64_t>(nfds)
        );
        if (syscall_failed(result)) {
            std::free(kernelFds);
            return syscall_fail(result, EINVAL);
        }

        for (nfds_t i = 0; i < nfds; ++i) {
            fds[i].revents = kernelFds[i].revents;
        }

        if (result != 0 || timeout == 0) {
            std::free(kernelFds);
            return static_cast<int>(result);
        }
        if (timeout > 0 && elapsed >= timeout) {
            std::free(kernelFds);
            return 0;
        }

        int nap = 10;
        if (timeout > 0 && timeout - elapsed < nap) {
            nap = timeout - elapsed;
        }
        if (nap <= 0) {
            std::free(kernelFds);
            return 0;
        }
        _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Sleep),
                      static_cast<std::uint64_t>(nap));
        if (timeout > 0) {
            elapsed += nap;
        }
    }
}

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout) {
    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }
    if (timeout != nullptr &&
        (timeout->tv_sec < 0 || timeout->tv_usec < 0 || timeout->tv_usec >= 1000000)) {
        errno = EINVAL;
        return -1;
    }

    struct pollfd pollfds[FD_SETSIZE];
    int count = 0;
    for (int fd = 0; fd < nfds; ++fd) {
        short events = 0;
        if (readfds != nullptr && FD_ISSET(fd, readfds)) {
            events |= POLLIN;
        }
        if (writefds != nullptr && FD_ISSET(fd, writefds)) {
            events |= POLLOUT;
        }
        if (exceptfds != nullptr && FD_ISSET(fd, exceptfds)) {
            events |= POLLPRI;
        }
        if (events != 0) {
            pollfds[count].fd = fd;
            pollfds[count].events = events;
            pollfds[count].revents = 0;
            count++;
        }
    }

    int timeoutMs = -1;
    if (timeout != nullptr) {
        const long long total = static_cast<long long>(timeout->tv_sec) * 1000LL +
                                (timeout->tv_usec + 999) / 1000;
        timeoutMs = total > 2147483647LL ? 2147483647 : static_cast<int>(total);
    }

    const int result = poll(pollfds, static_cast<nfds_t>(count), timeoutMs);
    if (result < 0) {
        return -1;
    }

    if (readfds != nullptr) {
        FD_ZERO(readfds);
    }
    if (writefds != nullptr) {
        FD_ZERO(writefds);
    }
    if (exceptfds != nullptr) {
        FD_ZERO(exceptfds);
    }

    for (int i = 0; i < count; ++i) {
        if (readfds != nullptr && (pollfds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
            FD_SET(pollfds[i].fd, readfds);
        }
        if (writefds != nullptr && (pollfds[i].revents & (POLLOUT | POLLERR))) {
            FD_SET(pollfds[i].fd, writefds);
        }
        if (exceptfds != nullptr && (pollfds[i].revents & POLLPRI)) {
            FD_SET(pollfds[i].fd, exceptfds);
        }
    }

    return result;
}

off_t lseek(int fd, off_t offset, int whence) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Seek),
        fd_to_handle(fd),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(offset)),
        static_cast<std::uint64_t>(whence)
    );
    if (syscall_failed(result)) {
        errno = syscall_errno(result, ESPIPE);
        return static_cast<off_t>(-1);
    }
    return static_cast<off_t>(result);
}

int fcntl(int fd, int cmd, ...) {
    std::uint64_t value = 0;
    if (cmd == F_SETFD) {
        va_list args;
        va_start(args, cmd);
        value = static_cast<std::uint64_t>(va_arg(args, int));
        va_end(args);
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Fcntl),
        fd_to_handle(fd),
        static_cast<std::uint64_t>(cmd),
        value
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }
    return static_cast<int>(result);
}

int chdir(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Chdir),
        reinterpret_cast<std::uint64_t>(path)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

char* getcwd(char* buf, size_t size) {
    if (buf == nullptr || size == 0) {
        errno = EINVAL;
        return nullptr;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Getcwd),
        reinterpret_cast<std::uint64_t>(buf),
        size
    );
    if (syscall_failed(result)) {
        errno = syscall_errno(result, EINVAL);
        return nullptr;
    }
    return reinterpret_cast<char*>(result);
}

int unlink(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Unlink),
        reinterpret_cast<std::uint64_t>(path)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

int link(const char* oldpath, const char* newpath) {
    if (oldpath == nullptr || newpath == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Link),
        reinterpret_cast<std::uint64_t>(oldpath),
        reinterpret_cast<std::uint64_t>(newpath)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

int symlink(const char* target, const char* linkpath) {
    if (target == nullptr || linkpath == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Symlink),
        reinterpret_cast<std::uint64_t>(target),
        reinterpret_cast<std::uint64_t>(linkpath)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

ssize_t readlink(const char* path, char* buf, size_t bufsiz) {
    if (path == nullptr || buf == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Readlink),
        reinterpret_cast<std::uint64_t>(path),
        reinterpret_cast<std::uint64_t>(buf),
        static_cast<std::uint64_t>(bufsiz)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EINVAL);
    }
    return static_cast<ssize_t>(result);
}

int rmdir(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Rmdir),
        reinterpret_cast<std::uint64_t>(path)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

int truncate(const char* path, off_t length) {
    if (path == nullptr || length < 0) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Truncate),
        reinterpret_cast<std::uint64_t>(path),
        static_cast<std::uint64_t>(length),
        0
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

int ftruncate(int fd, off_t length) {
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Truncate),
        fd_to_handle(fd),
        static_cast<std::uint64_t>(length),
        1
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }
    return 0;
}

int fsync(int fd) {
    struct stat st {};
    if (fstat(fd, &st) != 0) {
        return -1;
    }
    return 0;
}

int fdatasync(int fd) {
    return fsync(fd);
}

int rename(const char* oldpath, const char* newpath) {
    if (oldpath == nullptr || newpath == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Rename),
        reinterpret_cast<std::uint64_t>(oldpath),
        reinterpret_cast<std::uint64_t>(newpath)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

int chmod(const char* path, mode_t mode) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Chmod),
        reinterpret_cast<std::uint64_t>(path),
        static_cast<std::uint64_t>(mode),
        0
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

int fchmod(int fd, mode_t mode) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Chmod),
        fd_to_handle(fd),
        static_cast<std::uint64_t>(mode),
        1
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }
    return 0;
}

int utime(const char* path, const struct utimbuf* times) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }

    time_t atime = 0;
    time_t mtime = 0;
    if (times != nullptr) {
        atime = times->actime;
        mtime = times->modtime;
    } else {
        atime = time(nullptr);
        mtime = atime;
    }

    if (atime < 0 || mtime < 0) {
        errno = EINVAL;
        return -1;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Utime),
        reinterpret_cast<std::uint64_t>(path),
        static_cast<std::uint64_t>(atime),
        static_cast<std::uint64_t>(mtime)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

static int timespec_pair_to_seconds(const struct timespec times[2], const struct stat& current,
                                    time_t* atime, time_t* mtime) {
    if (times == nullptr) {
        const time_t now = time(nullptr);
        *atime = now;
        *mtime = now;
        return 0;
    }

    time_t values[2] { static_cast<time_t>(current.st_atime), static_cast<time_t>(current.st_mtime) };
    for (int i = 0; i < 2; ++i) {
        if (times[i].tv_nsec == UTIME_OMIT) {
            continue;
        }
        if (times[i].tv_nsec == UTIME_NOW) {
            values[i] = time(nullptr);
            continue;
        }
        if (times[i].tv_sec < 0 || times[i].tv_nsec < 0 || times[i].tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
        values[i] = times[i].tv_sec;
    }

    *atime = values[0];
    *mtime = values[1];
    return 0;
}

int futimens(int fd, const struct timespec times[2]) {
    struct stat current {};
    if (fstat(fd, &current) != 0) {
        return -1;
    }

    time_t atime = 0;
    time_t mtime = 0;
    if (timespec_pair_to_seconds(times, current, &atime, &mtime) != 0) {
        return -1;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Utime),
        fd_to_handle(fd),
        static_cast<std::uint64_t>(atime),
        static_cast<std::uint64_t>(mtime),
        1
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EBADF);
    }
    return 0;
}

int utimensat(int dirfd, const char* pathname, const struct timespec times[2], int flags) {
    if (pathname == nullptr || (flags & ~AT_SYMLINK_NOFOLLOW) != 0) {
        errno = EINVAL;
        return -1;
    }
    if (dirfd != AT_FDCWD && pathname[0] != '/') {
        errno = ENOSYS;
        return -1;
    }

    struct stat current {};
    if ((flags & AT_SYMLINK_NOFOLLOW) != 0) {
        if (lstat(pathname, &current) != 0) {
            return -1;
        }
        if (S_ISLNK(current.st_mode)) {
            errno = ENOSYS;
            return -1;
        }
    } else if (stat(pathname, &current) != 0) {
        return -1;
    }

    time_t atime = 0;
    time_t mtime = 0;
    if (timespec_pair_to_seconds(times, current, &atime, &mtime) != 0) {
        return -1;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Utime),
        reinterpret_cast<std::uint64_t>(pathname),
        static_cast<std::uint64_t>(atime),
        static_cast<std::uint64_t>(mtime),
        0
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }
    return 0;
}

pid_t getpid(void) {
    return static_cast<pid_t>(std::getpid());
}

pid_t getppid(void) {
    return static_cast<pid_t>(std::getppid());
}

pid_t getpgrp(void) {
    return getpid();
}

int setpgid(pid_t pid, pid_t pgid) {
    if (pid < 0 || pgid < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int isatty(int fd) {
    if (!is_stdio_fd(fd)) {
        errno = ENOTTY;
        return 0;
    }

    std::uint8_t ok = 0;
    std::uint64_t replySize = 0;
    if (terminal_request(ConsoleOpIsATTY, nullptr, 0, &ok, sizeof(ok), &replySize) != 0 ||
        replySize < sizeof(ok) || ok == 0) {
        errno = ENOTTY;
        return 0;
    }
    return 1;
}

int tcgetattr(int fd, struct termios* termios_p) {
    if (termios_p == nullptr) {
        errno = EINVAL;
        return -1;
    }
    if (!is_stdio_fd(fd)) {
        errno = ENOTTY;
        return -1;
    }

    std::uint64_t replySize = 0;
    if (terminal_request(ConsoleOpGetAttr, nullptr, 0, termios_p, sizeof(*termios_p), &replySize) != 0) {
        return -1;
    }
    if (replySize < sizeof(*termios_p)) {
        errno = ENOTTY;
        return -1;
    }
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
    if (termios_p == nullptr) {
        errno = EINVAL;
        return -1;
    }
    if (!is_stdio_fd(fd)) {
        errno = ENOTTY;
        return -1;
    }
    if (optional_actions != TCSANOW && optional_actions != TCSADRAIN && optional_actions != TCSAFLUSH) {
        errno = EINVAL;
        return -1;
    }

    return terminal_request(ConsoleOpSetAttr, termios_p, sizeof(*termios_p), nullptr, 0, nullptr);
}

pid_t tcgetpgrp(int fd) {
    if (!is_stdio_fd(fd)) {
        errno = ENOTTY;
        return -1;
    }

    pid_t pgrp = 0;
    std::uint64_t replySize = 0;
    if (terminal_request(ConsoleOpGetPgrp, nullptr, 0, &pgrp, sizeof(pgrp), &replySize) != 0) {
        return -1;
    }
    if (replySize < sizeof(pgrp)) {
        errno = ENOTTY;
        return -1;
    }
    return pgrp;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    if (!is_stdio_fd(fd)) {
        errno = ENOTTY;
        return -1;
    }
    if (pgrp < 0) {
        errno = EINVAL;
        return -1;
    }

    return terminal_request(ConsoleOpSetPgrp, &pgrp, sizeof(pgrp), nullptr, 0, nullptr);
}

int fork(void) {
    const auto result = _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Fork));
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOSYS);
    }
    return static_cast<int>(result);
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Exec),
        reinterpret_cast<std::uint64_t>(path),
        reinterpret_cast<std::uint64_t>(argv),
        reinterpret_cast<std::uint64_t>(envp)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOENT);
    }

    return static_cast<int>(result);
}

pid_t waitpid(pid_t pid, int* status, int options) {
    for (;;) {
        const auto result = _syscall_impl(
            static_cast<std::uint64_t>(std::Syscall::Wait),
            static_cast<std::uint64_t>(static_cast<std::int64_t>(pid)),
            reinterpret_cast<std::uint64_t>(status),
            static_cast<std::uint64_t>(options)
        );
        if (syscall_failed(result)) {
            const int err = syscall_errno(result, ECHILD);
            if (err == EINTR && __signal_restart_enabled && (options & WNOHANG) == 0) {
                continue;
            }
            errno = err;
            return -1;
        }
        return static_cast<pid_t>(result);
    }
}

pid_t wait(int* status) {
    return waitpid(-1, status, 0);
}

static int spawn_error(std::uint64_t result, int fallback) {
    if (!syscall_failed(result)) {
        return 0;
    }
    const int error = syscall_errno(result, fallback);
    errno = error;
    return error;
}

int posix_spawn(pid_t* pid, const char* path,
                const posix_spawn_file_actions_t* file_actions,
                const posix_spawnattr_t* attrp,
                char* const argv[], char* const envp[]) {
    (void)attrp;
    if (pid == nullptr || path == nullptr) {
        errno = EINVAL;
        return EINVAL;
    }
    if (file_actions != nullptr && file_actions->unsupported_actions != 0) {
        errno = ENOSYS;
        return ENOSYS;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Spawn),
        reinterpret_cast<std::uint64_t>(path),
        reinterpret_cast<std::uint64_t>(argv),
        reinterpret_cast<std::uint64_t>(envp)
    );
    const int error = spawn_error(result, ENOENT);
    if (error != 0) {
        return error;
    }

    *pid = static_cast<pid_t>(result);
    return 0;
}

static bool path_has_slash(const char* path) {
    if (path == nullptr) {
        return false;
    }
    while (*path != '\0') {
        if (*path == '/') {
            return true;
        }
        ++path;
    }
    return false;
}

static int spawn_from_bin(pid_t* pid, const char* file,
                          const posix_spawn_file_actions_t* file_actions,
                          const posix_spawnattr_t* attrp,
                          char* const argv[], char* const envp[]) {
    char path[256];
    const char prefix[] = "/bin/";
    std::size_t i = 0;
    while (prefix[i] != '\0' && i + 1 < sizeof(path)) {
        path[i] = prefix[i];
        ++i;
    }
    std::size_t j = 0;
    while (file[j] != '\0' && i + 1 < sizeof(path)) {
        path[i++] = file[j++];
    }
    path[i] = '\0';
    if (file[j] != '\0') {
        errno = ENOENT;
        return ENOENT;
    }
    return posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

int posix_spawnp(pid_t* pid, const char* file,
                 const posix_spawn_file_actions_t* file_actions,
                 const posix_spawnattr_t* attrp,
                 char* const argv[], char* const envp[]) {
    if (file == nullptr) {
        errno = EINVAL;
        return EINVAL;
    }
    if (path_has_slash(file)) {
        return posix_spawn(pid, file, file_actions, attrp, argv, envp);
    }

    int error = posix_spawn(pid, file, file_actions, attrp, argv, envp);
    if (error == 0) {
        return 0;
    }
    return spawn_from_bin(pid, file, file_actions, attrp, argv, envp);
}

int posix_spawnattr_init(posix_spawnattr_t* attr) {
    if (attr == nullptr) {
        return EINVAL;
    }
    attr->unused = 0;
    return 0;
}

int posix_spawnattr_destroy(posix_spawnattr_t* attr) {
    return attr == nullptr ? EINVAL : 0;
}

int posix_spawn_file_actions_init(posix_spawn_file_actions_t* file_actions) {
    if (file_actions == nullptr) {
        return EINVAL;
    }
    file_actions->unsupported_actions = 0;
    return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* file_actions) {
    return file_actions == nullptr ? EINVAL : 0;
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* file_actions, int fd) {
    (void)fd;
    if (file_actions == nullptr) {
        return EINVAL;
    }
    file_actions->unsupported_actions = 1;
    return ENOSYS;
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* file_actions, int oldfd, int newfd) {
    (void)oldfd;
    (void)newfd;
    if (file_actions == nullptr) {
        return EINVAL;
    }
    file_actions->unsupported_actions = 1;
    return ENOSYS;
}

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* file_actions,
                                     int fd, const char* path, int oflag, mode_t mode) {
    (void)fd;
    (void)path;
    (void)oflag;
    (void)mode;
    if (file_actions == nullptr) {
        return EINVAL;
    }
    file_actions->unsupported_actions = 1;
    return ENOSYS;
}

unsigned int sleep(unsigned int seconds) {
    const auto result = std::sleep(static_cast<std::uint64_t>(seconds) * 1000ULL);
    if (syscall_failed(result)) {
        errno = syscall_errno(result, EINVAL);
        return seconds;
    }
    return 0;
}

size_t strlen(const char* s) noexcept { return std::strlen(s); }
void* memchr(const void* s, int c, size_t n) noexcept {
    const unsigned char* bytes = static_cast<const unsigned char*>(s);
    const unsigned char needle = static_cast<unsigned char>(c);
    for (size_t i = 0; i < n; ++i) {
        if (bytes[i] == needle) {
            return const_cast<unsigned char*>(bytes + i);
        }
    }
    return nullptr;
}
int strcmp(const char* a, const char* b) noexcept { return std::strcmp(a, b); }
int strncmp(const char* a, const char* b, size_t n) noexcept { return std::strncmp(a, b, n); }
int strcoll(const char* a, const char* b) noexcept {
    /* Only the "C" locale is supported, so collation is byte-wise. */
    return std::strcmp(a, b);
}
char* strcat(char* dest, const char* src) noexcept {
    char* out = dest + std::strlen(dest);
    while (*src != '\0') {
        *out++ = *src++;
    }
    *out = '\0';
    return dest;
}
char* strcpy(char* dest, const char* src) noexcept { return std::strcpy(dest, src); }
char* strncpy(char* dest, const char* src, size_t n) noexcept { return std::strncpy(dest, src, n); }
char* strchr(const char* s, int c) noexcept {
    const char needle = static_cast<char>(c);
    for (;; ++s) {
        if (*s == needle) {
            return const_cast<char*>(s);
        }
        if (*s == '\0') {
            return nullptr;
        }
    }
}

size_t strspn(const char* s, const char* accept) noexcept {
    if (s == nullptr || accept == nullptr) {
        return 0;
    }
    const char* p = s;
    while (*p != '\0') {
        bool ok = false;
        for (const char* a = accept; *a != '\0'; ++a) {
            if (*p == *a) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            break;
        }
        ++p;
    }
    return static_cast<size_t>(p - s);
}

char* strpbrk(const char* s, const char* accept) noexcept {
    if (s == nullptr || accept == nullptr) {
        return nullptr;
    }
    for (const char* p = s; *p != '\0'; ++p) {
        for (const char* a = accept; *a != '\0'; ++a) {
            if (*p == *a) {
                return const_cast<char*>(p);
            }
        }
    }
    return nullptr;
}
char* strrchr(const char* s, int c) noexcept {
    const char needle = static_cast<char>(c);
    const char* last = nullptr;
    for (;; ++s) {
        if (*s == needle) {
            last = s;
        }
        if (*s == '\0') {
            break;
        }
    }
    return const_cast<char*>(last);
}
char* strstr(const char* haystack, const char* needle) noexcept {
    if (*needle == '\0') {
        return const_cast<char*>(haystack);
    }
    for (const char* h = haystack; *h != '\0'; ++h) {
        const char* hs = h;
        const char* ns = needle;
        while (*hs != '\0' && *ns != '\0' && *hs == *ns) {
            ++hs;
            ++ns;
        }
        if (*ns == '\0') {
            return const_cast<char*>(h);
        }
    }
    return nullptr;
}
void* memcpy(void* dest, const void* src, size_t n) noexcept { return std::memcpy(dest, src, n); }
void* memmove(void* dest, const void* src, size_t n) noexcept { return std::memmove(dest, src, n); }
void* memset(void* dest, int val, size_t n) noexcept { return std::memset(dest, val, n); }
int memcmp(const void* a, const void* b, size_t n) noexcept { return std::memcmp(a, b, n); }

long strtol(const char* nptr, char** endptr, int base) {
    if (nptr == nullptr) {
        if (endptr) {
            *endptr = nullptr;
        }
        errno = EINVAL;
        return 0;
    }

    const char* p = nptr;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') {
        ++p;
    }

    int sign = 1;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        ++p;
    }

    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0') {
            base = 8;
            ++p;
        } else {
            base = 10;
        }
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }

    if (base < 2 || base > 36) {
        if (endptr) {
            *endptr = const_cast<char*>(nptr);
        }
        errno = EINVAL;
        return 0;
    }

    unsigned long value = 0;
    bool any = false;
    for (;; ++p) {
        int digit;
        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        any = true;
        value = value * static_cast<unsigned long>(base) + static_cast<unsigned long>(digit);
    }

    if (endptr) {
        *endptr = const_cast<char*>(any ? p : nptr);
    }

    if (!any) {
        return 0;
    }

    if (sign < 0) {
        const unsigned long limit = static_cast<unsigned long>(LONG_MAX) + 1UL;
        if (value > limit) {
            errno = ERANGE;
            return LONG_MIN;
        }
        if (value == limit) {
            return LONG_MIN;
        }
        return -static_cast<long>(value);
    }

    if (value > static_cast<unsigned long>(LONG_MAX)) {
        errno = ERANGE;
        return LONG_MAX;
    }

    return static_cast<long>(value);
}

double strtod(const char* nptr, char** endptr) {
    if (endptr) {
        *endptr = const_cast<char*>(nptr);
    }
    if (nptr == nullptr) {
        errno = EINVAL;
        return 0.0;
    }

    const char* p = nptr;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') {
        ++p;
    }

    int sign = 1;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        ++p;
    }

    double intPart = 0.0;
    bool any = false;
    while (*p >= '0' && *p <= '9') {
        any = true;
        intPart = intPart * 10.0 + static_cast<double>(*p - '0');
        ++p;
    }

    double fracPart = 0.0;
    double fracScale = 1.0;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') {
            any = true;
            fracPart = fracPart * 10.0 + static_cast<double>(*p - '0');
            fracScale *= 10.0;
            ++p;
        }
    }

    if (!any) {
        errno = EINVAL;
        return 0.0;
    }

    int exp10 = 0;
    int expSign = 1;
    if (*p == 'e' || *p == 'E') {
        const char* expStart = p;
        ++p;
        if (*p == '+' || *p == '-') {
            expSign = (*p == '-') ? -1 : 1;
            ++p;
        }
        if (*p < '0' || *p > '9') {
            p = expStart;
        } else {
            while (*p >= '0' && *p <= '9') {
                exp10 = exp10 * 10 + (*p - '0');
                ++p;
            }
            exp10 *= expSign;
        }
    }

    if (endptr) {
        *endptr = const_cast<char*>(p);
    }

    double value = intPart;
    if (fracScale != 1.0) {
        value += fracPart / fracScale;
    }

    if (exp10 != 0) {
        double base = (exp10 < 0) ? 0.1 : 10.0;
        int e = (exp10 < 0) ? -exp10 : exp10;
        double factor = 1.0;
        while (e > 0) {
            if ((e & 1) != 0) {
                factor *= base;
            }
            base *= base;
            e >>= 1;
        }
        value *= factor;
    }

    return sign < 0 ? -value : value;
}

char* getenv(const char* name) {
    (void)name;
    return nullptr;
}

static void qsort_swap(unsigned char* a, unsigned char* b, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        const unsigned char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

static void qsort_impl(unsigned char* base, long left, long right, size_t size,
                       int (*compar)(const void*, const void*)) {
    if (left >= right) {
        return;
    }

    const long pivot_index = left + (right - left) / 2;
    unsigned char* pivot = base + static_cast<size_t>(pivot_index) * size;
    qsort_swap(pivot, base + static_cast<size_t>(right) * size, size);

    long store = left;
    for (long i = left; i < right; ++i) {
        unsigned char* current = base + static_cast<size_t>(i) * size;
        if (compar(current, base + static_cast<size_t>(right) * size) < 0) {
            qsort_swap(base + static_cast<size_t>(store) * size, current, size);
            ++store;
        }
    }

    qsort_swap(base + static_cast<size_t>(store) * size, base + static_cast<size_t>(right) * size, size);
    qsort_impl(base, left, store - 1, size, compar);
    qsort_impl(base, store + 1, right, size, compar);
}

void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (base == nullptr || compar == nullptr || nmemb < 2 || size == 0) {
        return;
    }
    qsort_impl(static_cast<unsigned char*>(base), 0, static_cast<long>(nmemb - 1), size, compar);
}

int vsnprintf(char* buffer, size_t size, const char* format, va_list args) noexcept {
    return std::vsnprintf(buffer, size, format, args);
}

int snprintf(char* buffer, size_t size, const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    int r = std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return r;
}

int vsprintf(char* buffer, const char* format, va_list args) noexcept {
    return std::vsnprintf(buffer, static_cast<size_t>(-1), format, args);
}

int sprintf(char* buffer, const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    int r = std::vsnprintf(buffer, static_cast<size_t>(-1), format, args);
    va_end(args);
    return r;
}

int printf(const char* format, ...) noexcept {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (written > 0) {
        std::size_t count = static_cast<std::size_t>(written);
        if (count >= sizeof(buffer)) {
            count = sizeof(buffer) - 1;
        }
        std::write(std::STDOUT_HANDLE, buffer, count);
    }
    return written;
}

int putchar(int c) noexcept {
    const char ch = static_cast<char>(c);
    return static_cast<int>(std::write(std::STDOUT_HANDLE, &ch, 1));
}

FILE* fopen(const char* path, const char* mode) {
    if (path == nullptr || mode == nullptr || mode[0] == '\0') {
        errno = EINVAL;
        return nullptr;
    }

    int flags = O_RDONLY;
    switch (mode[0]) {
        case 'r':
            flags = O_RDONLY;
            break;
        case 'w':
            flags = O_WRONLY;
            break;
        case 'a':
            flags = O_WRONLY;
            break;
        default:
            errno = EINVAL;
            return nullptr;
    }

    bool plus = false;
    for (const char* p = mode + 1; *p != '\0'; ++p) {
        if (*p == '+') {
            plus = true;
            break;
        }
    }

    if (plus) {
        flags = O_RDWR;
    }

    FILE* stream = static_cast<FILE*>(std::malloc(sizeof(FILE)));
    if (stream == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    const int fd = open(path, flags);
    if (fd < 0) {
        std::free(stream);
        return nullptr;
    }

    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;

    if (mode[0] == 'a') {
        if (lseek(fd, 0, SEEK_END) < 0) {
            close(fd);
            std::free(stream);
            return nullptr;
        }
    }

    return stream;
}

int fclose(FILE* stream) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1;
    }

    const int result = close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr) {
        std::free(stream);
    } else {
        stream->eof = 0;
        stream->error = 0;
    }
    return result;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
    if (stream == nullptr || (ptr == nullptr && size != 0 && count != 0)) {
        errno = EINVAL;
        return 0;
    }
    if (size == 0 || count == 0) {
        return 0;
    }

    const size_t total = size * count;
    const ssize_t result = read(stream->fd, ptr, total);
    if (result < 0) {
        stream->error = 1;
        return 0;
    }
    if (result == 0) {
        stream->eof = 1;
        return 0;
    }
    if (static_cast<size_t>(result) < total) {
        stream->eof = 1;
    }
    return static_cast<size_t>(result) / size;
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
    if (stream == nullptr || (ptr == nullptr && size != 0 && count != 0)) {
        errno = EINVAL;
        return 0;
    }
    if (size == 0 || count == 0) {
        return 0;
    }

    const size_t total = size * count;
    const ssize_t result = write(stream->fd, ptr, total);
    if (result < 0) {
        stream->error = 1;
        return 0;
    }
    return static_cast<size_t>(result) / size;
}

int fseek(FILE* stream, long offset, int whence) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1;
    }
    if (lseek(stream->fd, static_cast<off_t>(offset), whence) < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE* stream) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1L;
    }
    const off_t result = lseek(stream->fd, 0, SEEK_CUR);
    if (result < 0) {
        stream->error = 1;
        return -1L;
    }
    return static_cast<long>(result);
}

int fflush(FILE* stream) {
    if (stream == nullptr) {
        return 0;
    }
    return stream->error ? -1 : 0;
}

int feof(FILE* stream) {
    return stream ? stream->eof : 0;
}

int ferror(FILE* stream) {
    return stream ? stream->error : 0;
}

void clearerr(FILE* stream) {
    if (stream) {
        stream->eof = 0;
        stream->error = 0;
    }
}

int fileno(FILE* stream) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1;
    }
    return stream->fd;
}

int remove(const char* path) {
    if (unlink(path) == 0) {
        return 0;
    }
    if (errno != EISDIR) {
        return -1;
    }
    return rmdir(path);
}

double sqrt(double x) noexcept { return std::sqrt(x); }
double fabs(double x) noexcept { return std::fabs(x); }
double sin(double x) noexcept { return std::sin(x); }
double cos(double x) noexcept { return std::cos(x); }
double tan(double x) noexcept { return std::tan(x); }
double floor(double x) noexcept { return std::floor(x); }
double ceil(double x) noexcept { return std::ceil(x); }

float sqrtf(float x) { return static_cast<float>(std::sqrt(static_cast<double>(x))); }
float fabsf(float x) { return static_cast<float>(std::fabs(static_cast<double>(x))); }

float sinf(float x) { return static_cast<float>(std::sin(static_cast<double>(x))); }
float cosf(float x) { return static_cast<float>(std::cos(static_cast<double>(x))); }
float floorf(float x) { return static_cast<float>(std::floor(static_cast<double>(x))); }

float fminf(float a, float b) { return a < b ? a : b; }

float atan2f(float y, float x);

float asinf(float x) {
    if (x < -1.0f || x > 1.0f) {
        return 0.0f / 0.0f;
    }
    return atan2f(x, sqrtf(1.0f - x * x));
}

float atan2f(float y, float x) {
    const float pi = 3.14159265358979323846f;
    if (x > 0.0f) {
        return fast_atanf(y / x);
    } else if (x < 0.0f) {
        if (y >= 0.0f) return fast_atanf(y / x) + pi;
        return fast_atanf(y / x) - pi;
    } else {
        if (y > 0.0f) return pi * 0.5f;
        if (y < 0.0f) return -pi * 0.5f;
        return 0.0f;
    }
}

float fmaxf(float a, float b) {
    return a > b ? a : b;
}

float acosf(float x) {
    const float pi = 3.14159265358979323846f;
    return (pi * 0.5f) - asinf(x);
}

int stat(const char* path, struct stat* buf) {
    if (path == nullptr || buf == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Stat),
                             reinterpret_cast<std::uint64_t>(path),
                             reinterpret_cast<std::uint64_t>(buf));
    if (syscall_failed(res)) {
        return syscall_fail(res, ENOENT);
    }
    return 0;
}

int lstat(const char* path, struct stat* buf) {
    if (path == nullptr || buf == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Lstat),
                             reinterpret_cast<std::uint64_t>(path),
                             reinterpret_cast<std::uint64_t>(buf));
    if (syscall_failed(res)) {
        return syscall_fail(res, ENOENT);
    }
    return 0;
}

int fstat(int fd, struct stat* buf) {
    if (buf == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Fstat),
                             fd_to_handle(fd),
                             reinterpret_cast<std::uint64_t>(buf));
    if (syscall_failed(res)) {
        return syscall_fail(res, EBADF);
    }
    return 0;
}

int access(const char* path, int mode) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    if ((mode & ~(R_OK | W_OK | X_OK)) != 0) {
        errno = EINVAL;
        return -1;
    }

    struct stat st {};
    if (stat(path, &st) != 0) {
        return -1;
    }
    if (mode == F_OK) {
        return 0;
    }

    if ((mode & R_OK) && (st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) == 0) {
        errno = EACCES;
        return -1;
    }
    if ((mode & W_OK) && (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0) {
        errno = EACCES;
        return -1;
    }
    if ((mode & X_OK) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) {
        errno = EACCES;
        return -1;
    }

    return 0;
}

int mkdir(const char* path, mode_t mode) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Mkdir),
                             reinterpret_cast<std::uint64_t>(path),
                             static_cast<std::uint64_t>(mode & ~__process_umask));
    if (syscall_failed(res)) {
        return syscall_fail(res, EEXIST);
    }
    return 0;
}

mode_t umask(mode_t mask) {
    const mode_t old = __process_umask;
    __process_umask = mask & 0777;
    return old;
}

static std::uint64_t instant_prot_from_posix(int prot) {
    std::uint64_t out = 0;
    if (prot & PROT_READ) {
        out |= std::MemoryProtRead;
    }
    if (prot & PROT_WRITE) {
        out |= std::MemoryProtWrite;
    }
    if (prot & PROT_EXEC) {
        out |= std::MemoryProtExecute;
    }
    return out;
}

static void* mmap_anonymous_pages(void* addr, size_t length, int prot) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Mmap),
        reinterpret_cast<std::uint64_t>(addr),
        length,
        instant_prot_from_posix(prot)
    );
    if (syscall_failed(result)) {
        errno = syscall_errno(result, ENOMEM);
        return MAP_FAILED;
    }
    return reinterpret_cast<void*>(result);
}

static bool page_aligned_pointer(const void* ptr) {
    return (reinterpret_cast<std::uintptr_t>(ptr) & 4095ULL) == 0;
}

static int restore_file_offset(int fd, off_t offset) {
    return lseek(fd, offset, SEEK_SET) < 0 ? -1 : 0;
}

static size_t page_aligned_length(size_t length) {
    return (length + 4095UL) & ~4095UL;
}

static TrackedMapping* find_tracked_mapping(void* addr, size_t length) {
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    const auto end = start + length;
    for (int i = 0; i < kMaxTrackedMappings; ++i) {
        if (__tracked_mappings[i].addr == nullptr) {
            continue;
        }
        const auto mapStart = reinterpret_cast<std::uintptr_t>(__tracked_mappings[i].addr);
        const auto mapEnd = mapStart + __tracked_mappings[i].length;
        if (start >= mapStart && end <= mapEnd) {
            return &__tracked_mappings[i];
        }
    }
    return nullptr;
}

static int track_mapping(void* addr, size_t length, int fd, off_t offset, bool sharedWritable) {
    for (int i = 0; i < kMaxTrackedMappings; ++i) {
        if (__tracked_mappings[i].addr == nullptr) {
            __tracked_mappings[i].addr = addr;
            __tracked_mappings[i].length = page_aligned_length(length);
            __tracked_mappings[i].fd = fd;
            __tracked_mappings[i].offset = offset;
            __tracked_mappings[i].sharedWritable = sharedWritable;
            return 0;
        }
    }
    errno = ENOMEM;
    return -1;
}

static int writeback_mapping_range(TrackedMapping* mapping, void* addr, size_t length) {
    if (mapping == nullptr || !mapping->sharedWritable) {
        return 0;
    }

    const auto mapStart = reinterpret_cast<std::uintptr_t>(mapping->addr);
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    const size_t delta = static_cast<size_t>(start - mapStart);
    const off_t originalOffset = lseek(mapping->fd, 0, SEEK_CUR);
    if (originalOffset < 0 || lseek(mapping->fd, mapping->offset + static_cast<off_t>(delta), SEEK_SET) < 0) {
        return -1;
    }

    const unsigned char* data = static_cast<const unsigned char*>(addr);
    size_t written = 0;
    while (written < length) {
        const ssize_t nwrite = write(mapping->fd, data + written, length - written);
        if (nwrite < 0) {
            const int savedErrno = errno;
            restore_file_offset(mapping->fd, originalOffset);
            errno = savedErrno;
            return -1;
        }
        if (nwrite == 0) {
            errno = EIO;
            restore_file_offset(mapping->fd, originalOffset);
            return -1;
        }
        written += static_cast<size_t>(nwrite);
    }

    const int savedErrno = errno;
    if (restore_file_offset(mapping->fd, originalOffset) != 0) {
        return -1;
    }
    errno = savedErrno;
    return 0;
}

static void untrack_mapping(TrackedMapping* mapping) {
    if (mapping == nullptr) {
        return;
    }
    if (mapping->fd >= 0) {
        close(mapping->fd);
    }
    mapping->addr = nullptr;
    mapping->length = 0;
    mapping->fd = -1;
    mapping->offset = 0;
    mapping->sharedWritable = false;
}

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (length == 0 || offset < 0 || (static_cast<unsigned long>(offset) & 4095UL) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if ((prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if ((flags & ~(MAP_SHARED | MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS)) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    const bool isPrivate = (flags & MAP_PRIVATE) != 0;
    const bool isShared = (flags & MAP_SHARED) != 0;
    const bool isAnonymous = (flags & MAP_ANONYMOUS) != 0;
    if (isPrivate == isShared) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if ((flags & MAP_FIXED) != 0) {
        if (addr == nullptr || !page_aligned_pointer(addr)) {
            errno = EINVAL;
            return MAP_FAILED;
        }
    } else {
        addr = nullptr;
    }

    if (isAnonymous) {
        if (fd != -1 || offset != 0) {
            errno = EINVAL;
            return MAP_FAILED;
        }
        if (prot != PROT_NONE) {
            return mmap_anonymous_pages(addr, length, prot);
        }
        void* mapping = mmap_anonymous_pages(addr, length, PROT_WRITE);
        if (mapping == MAP_FAILED) {
            return MAP_FAILED;
        }
        if (mprotect(mapping, length, PROT_NONE) != 0) {
            const int savedErrno = errno;
            munmap(mapping, length);
            errno = savedErrno;
            return MAP_FAILED;
        }
        return mapping;
    }

    if (fd < 0) {
        errno = EBADF;
        return MAP_FAILED;
    }
    int trackedFd = -1;
    if (isShared && (prot & PROT_WRITE)) {
        trackedFd = dup(fd);
        if (trackedFd < 0) {
            return MAP_FAILED;
        }
        const char writeProbe = 0;
        if (write(trackedFd, &writeProbe, 0) < 0) {
            const int savedErrno = errno;
            close(trackedFd);
            errno = savedErrno;
            return MAP_FAILED;
        }
    }

    const int loadProt = prot | PROT_WRITE;
    void* mapping = mmap_anonymous_pages(addr, length, loadProt);
    if (mapping == MAP_FAILED) {
        if (trackedFd >= 0) {
            close(trackedFd);
        }
        return MAP_FAILED;
    }

    const off_t originalOffset = lseek(fd, 0, SEEK_CUR);
    if (originalOffset < 0 || lseek(fd, offset, SEEK_SET) < 0) {
        if (trackedFd >= 0) {
            close(trackedFd);
        }
        munmap(mapping, length);
        return MAP_FAILED;
    }

    unsigned char* out = static_cast<unsigned char*>(mapping);
    size_t done = 0;
    while (done < length) {
        const ssize_t nread = read(fd, out + done, length - done);
        if (nread < 0) {
            const int savedErrno = errno;
            restore_file_offset(fd, originalOffset);
            if (trackedFd >= 0) {
                close(trackedFd);
            }
            munmap(mapping, length);
            errno = savedErrno;
            return MAP_FAILED;
        }
        if (nread == 0) {
            break;
        }
        done += static_cast<size_t>(nread);
    }

    const int savedErrno = errno;
    if (restore_file_offset(fd, originalOffset) != 0) {
        const int restoreErrno = errno;
        if (trackedFd >= 0) {
            close(trackedFd);
        }
        munmap(mapping, length);
        errno = restoreErrno;
        return MAP_FAILED;
    }
    errno = savedErrno;

    if (prot != loadProt && mprotect(mapping, length, prot) != 0) {
        const int protectErrno = errno;
        if (trackedFd >= 0) {
            close(trackedFd);
        }
        munmap(mapping, length);
        errno = protectErrno;
        return MAP_FAILED;
    }

    if (trackedFd >= 0 && track_mapping(mapping, length, trackedFd, offset, true) != 0) {
        const int trackErrno = errno;
        close(trackedFd);
        munmap(mapping, length);
        errno = trackErrno;
        return MAP_FAILED;
    }

    return mapping;
}

int munmap(void* addr, size_t length) {
    if (addr == nullptr || length == 0 || !page_aligned_pointer(addr)) {
        errno = EINVAL;
        return -1;
    }

    TrackedMapping* mapping = find_tracked_mapping(addr, length);
    if (mapping && writeback_mapping_range(mapping, addr, length) != 0) {
        return -1;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Munmap),
        reinterpret_cast<std::uint64_t>(addr),
        length
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EINVAL);
    }
    if (mapping && mapping->addr == addr && page_aligned_length(length) >= mapping->length) {
        untrack_mapping(mapping);
    }
    return 0;
}

int mprotect(void* addr, size_t length, int prot) {
    if (addr == nullptr || length == 0 || !page_aligned_pointer(addr) ||
        (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) != 0) {
        errno = EINVAL;
        return -1;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Mprotect),
        reinterpret_cast<std::uint64_t>(addr),
        length,
        instant_prot_from_posix(prot)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EINVAL);
    }
    return 0;
}

int msync(void* addr, size_t length, int flags) {
    if (addr == nullptr || length == 0 || !page_aligned_pointer(addr) ||
        (flags & ~(MS_ASYNC | MS_INVALIDATE | MS_SYNC)) != 0 ||
        ((flags & MS_ASYNC) && (flags & MS_SYNC))) {
        errno = EINVAL;
        return -1;
    }

    TrackedMapping* mapping = find_tracked_mapping(addr, length);
    if (mapping == nullptr) {
        errno = EINVAL;
        return -1;
    }
    return writeback_mapping_range(mapping, addr, length);
}

int abs(int x) noexcept { return std::abs(x); }

int rand(void) noexcept {
    __rand_state = __rand_state * 1103515245u + 12345u;
    return static_cast<int>((__rand_state >> 16) & 0x7FFFFFFF);
}

void srand(unsigned int seed) noexcept {
    __rand_state = static_cast<unsigned long>(seed);
}

struct find_state {
    char dir[256];
    char pattern[256];
    std::DirEntry* entries;
    std::size_t count;
    std::size_t index;
};

static bool wildcard_match(const char* pat, const char* s) {
    if (!pat || !s) return false;
    while (*pat) {
        if (*pat == '*') {
            ++pat;
            if (!*pat) return true;
            while (*s) {
                if (wildcard_match(pat, s)) return true;
                ++s;
            }
            return false;
        } else if (*pat == '?') {
            if (!*s) return false;
            ++pat; ++s;
        } else {
            if (*pat != *s) return false;
            ++pat; ++s;
        }
    }
    return *s == '\0';
}

_find_t _findfirst(const char* filespec, struct _finddata_t* fileinfo) {
    if (filespec == nullptr || fileinfo == nullptr) {
        errno = EINVAL;
        return -1;
    }

    const char* last_slash = nullptr;
    for (const char* p = filespec; *p; ++p) if (*p == '/') last_slash = p;

    char dir[256] = {0};
    char pattern[256] = {0};
    if (last_slash) {
        std::size_t dlen = static_cast<std::size_t>(last_slash - filespec);
        if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
        for (std::size_t i = 0; i < dlen; ++i) dir[i] = filespec[i];
        const char* patstart = last_slash + 1;
        std::size_t plen = 0;
        while (patstart[plen] && plen + 1 < sizeof(pattern)) { pattern[plen] = patstart[plen]; ++plen; }
    } else {
        dir[0] = '.'; dir[1] = '\0';
        std::size_t plen = 0;
        while (filespec[plen] && plen + 1 < sizeof(pattern)) { pattern[plen] = filespec[plen]; ++plen; }
    }

    find_state* state = static_cast<find_state*>(std::malloc(sizeof(find_state)));
    if (!state) {
        errno = ENOMEM;
        return -1;
    }
    for (std::size_t i = 0; i < sizeof(state->dir); ++i) state->dir[i] = dir[i];
    for (std::size_t i = 0; i < sizeof(state->pattern); ++i) state->pattern[i] = pattern[i];
    state->entries = nullptr;
    state->count = 0;
    state->index = 0;

    const std::size_t cap = 1024;
    state->entries = static_cast<std::DirEntry*>(std::malloc(sizeof(std::DirEntry) * cap));
    if (!state->entries) {
        std::free(state);
        errno = ENOMEM;
        return -1;
    }

    std::uint64_t found = std::readdir(state->dir, state->entries, cap);
    if (syscall_failed(found)) {
        std::free(state->entries);
        std::free(state);
        errno = syscall_errno(found, ENOENT);
        return -1;
    }

    state->count = static_cast<std::size_t>(found);

    while (state->index < state->count) {
        const auto& e = state->entries[state->index];
        if (wildcard_match(state->pattern, e.name)) {
            for (std::size_t i = 0; i < sizeof(fileinfo->name) - 1 && e.name[i]; ++i) fileinfo->name[i] = e.name[i];
            fileinfo->name[sizeof(fileinfo->name) - 1] = '\0';
            fileinfo->attrib = (e.type == std::FileType::Directory) ? 0x10 : 0x0;
            fileinfo->size_low = 0;
            fileinfo->size_high = 0;
            state->index++;
            return reinterpret_cast<_find_t>(state);
        }
        state->index++;
    }

    std::free(state->entries);
    std::free(state);
    errno = ENOENT;
    return -1;
}

int _findnext(_find_t handle, struct _finddata_t* fileinfo) {
    if (handle == -1 || fileinfo == nullptr) {
        errno = EINVAL;
        return -1;
    }
    find_state* state = reinterpret_cast<find_state*>(handle);
    while (state->index < state->count) {
        const auto& e = state->entries[state->index];
        if (wildcard_match(state->pattern, e.name)) {
            for (std::size_t i = 0; i < sizeof(fileinfo->name) - 1 && e.name[i]; ++i) fileinfo->name[i] = e.name[i];
            fileinfo->name[sizeof(fileinfo->name) - 1] = '\0';
            fileinfo->attrib = (e.type == std::FileType::Directory) ? 0x10 : 0x0;
            fileinfo->size_low = 0;
            fileinfo->size_high = 0;
            state->index++;
            return 0;
        }
        state->index++;
    }
    errno = ENOENT;
    return -1;
}

int _findclose(_find_t handle) {
    if (handle == -1) {
        errno = EINVAL;
        return -1;
    }
    find_state* state = reinterpret_cast<find_state*>(handle);
    if (state->entries) std::free(state->entries);
    std::free(state);
    return 0;
}

struct DIR {
    char path[256];
    std::DirEntry* entries;
    std::size_t count;
    std::size_t index;
    struct dirent current;
};

static unsigned char dirent_type_from_instant(std::FileType type) {
    switch (type) {
        case std::FileType::Directory:
            return DT_DIR;
        case std::FileType::Regular:
            return DT_REG;
        default:
            return DT_UNKNOWN;
    }
}

DIR* opendir(const char* name) {
    if (name == nullptr) {
        errno = EINVAL;
        return nullptr;
    }

    struct stat st {};
    if (stat(name, &st) != 0) {
        return nullptr;
    }
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return nullptr;
    }

    auto* dir = static_cast<DIR*>(std::calloc(1, sizeof(DIR)));
    if (dir == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    std::size_t i = 0;
    while (name[i] != '\0' && i + 1 < sizeof(dir->path)) {
        dir->path[i] = name[i];
        ++i;
    }
    dir->path[i] = '\0';

    constexpr std::size_t capacity = 1024;
    dir->entries = static_cast<std::DirEntry*>(std::malloc(sizeof(std::DirEntry) * capacity));
    if (dir->entries == nullptr) {
        std::free(dir);
        errno = ENOMEM;
        return nullptr;
    }

    const std::uint64_t found = std::readdir(dir->path, dir->entries, capacity);
    if (syscall_failed(found)) {
        std::free(dir->entries);
        std::free(dir);
        errno = syscall_errno(found, ENOENT);
        return nullptr;
    }

    dir->count = static_cast<std::size_t>(found);
    dir->index = 0;
    return dir;
}

struct dirent* readdir(DIR* dirp) {
    if (dirp == nullptr) {
        errno = EINVAL;
        return nullptr;
    }
    if (dirp->index >= dirp->count) {
        return nullptr;
    }

    const auto& entry = dirp->entries[dirp->index++];
    dirp->current.d_ino = entry.inode;
    dirp->current.d_type = dirent_type_from_instant(entry.type);

    std::size_t i = 0;
    while (entry.name[i] != '\0' && i + 1 < sizeof(dirp->current.d_name)) {
        dirp->current.d_name[i] = entry.name[i];
        ++i;
    }
    dirp->current.d_name[i] = '\0';
    return &dirp->current;
}

int closedir(DIR* dirp) {
    if (dirp == nullptr) {
        errno = EINVAL;
        return -1;
    }
    std::free(dirp->entries);
    std::free(dirp);
    return 0;
}

void rewinddir(DIR* dirp) {
    if (dirp != nullptr) {
        dirp->index = 0;
    }
}

long telldir(DIR* dirp) {
    if (dirp == nullptr) {
        errno = EINVAL;
        return -1;
    }
    return static_cast<long>(dirp->index);
}

void seekdir(DIR* dirp, long loc) {
    if (dirp == nullptr || loc < 0) {
        errno = EINVAL;
        return;
    }

    const std::size_t index = static_cast<std::size_t>(loc);
    dirp->index = index > dirp->count ? dirp->count : index;
}

int _getch(void) {
    unsigned char c = 0;
    auto ret = std::read(0, &c, 1);
    if (syscall_failed(ret)) {
        errno = syscall_errno(ret, EINVAL);
        return -1;
    }
    return static_cast<int>(c);
}

int _chdir(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = std::chdir(path);
    if (syscall_failed(res)) {
        return syscall_fail(res, ENOENT);
    }
    return 0;
}

int _mkdir(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = std::mkdir(path, 0755);
    if (syscall_failed(res)) {
        return syscall_fail(res, EEXIST);
    }
    return 0;
}

int getch(void) {
    return _getch();
}

int kbhit(void) {
    return 0;
}

void exit(int status) {
    std::exit(static_cast<std::uint64_t>(status));
}

void abort(void) {
    std::exit(1);
}

void __stack_chk_fail(void) {
    abort();
}

void __assert_fail(const char* expr, const char* file, int line, const char* func) {
    char buf[256];
    int n = std::snprintf(buf, sizeof(buf), "Assertion failed: %s, function %s, file %s, line %d\n",
                         expr ? expr : "(null)", func ? func : "(null)", file ? file : "(null)", line);
    if (n > 0) {
        std::write(2, buf, static_cast<std::uint64_t>(n));
    }
    std::exit(1);
}

time_t time(time_t* t) {
    time_t sec = static_cast<time_t>(std::getunixtime());
    if (t) *t = sec;
    return sec;
}

int clock_gettime(int clk_id, struct timespec* tp) {
    if (tp == nullptr) return -1;
    uint64_t ms = std::gettime();
    tp->tv_sec = static_cast<time_t>(ms / 1000);
    tp->tv_nsec = static_cast<long>((ms % 1000) * 1000000);
    return 0;
}

static double pow2_int(int exp) {
    double base = 2.0;
    int e = exp;
    if (e < 0) {
        base = 0.5;
        e = -e;
    }

    double result = 1.0;
    while (e > 0) {
        if ((e & 1) != 0) {
            result *= base;
        }
        base *= base;
        e >>= 1;
    }
    return result;
}

double ldexp(double x, int exp) {
    if (x == 0.0) {
        return x;
    }
    return x * pow2_int(exp);
}

float ldexpf(float x, int exp) {
    if (x == 0.0f) {
        return x;
    }
    return x * static_cast<float>(pow2_int(exp));
}

double frexp(double x, int* exp) {
    if (exp == nullptr) {
        errno = EINVAL;
        return 0.0;
    }

    *exp = 0;
    if (x == 0.0) {
        return 0.0;
    }

    union {
        double d;
        std::uint64_t u;
    } v = { x };

    const std::uint64_t sign = v.u & 0x8000000000000000ULL;
    std::uint64_t e = (v.u >> 52) & 0x7FFULL;

    if (e == 0x7FFULL) {  /* inf/nan */
        return x;
    }

    if (e == 0) {  /* subnormal */
        /* scale into normal range */
        v.d = x * 18014398509481984.0; /* 2^54 */
        e = (v.u >> 52) & 0x7FFULL;
        if (e == 0) { /* still zero */
            return 0.0;
        }
        *exp = static_cast<int>(e) - 1023 - 54 + 1;
    } else {
        *exp = static_cast<int>(e) - 1023 + 1;
    }

    /* Set exponent to 1022 so result is in [0.5, 1). */
    v.u = sign | (static_cast<std::uint64_t>(1022) << 52) | (v.u & 0x000FFFFFFFFFFFFFULL);
    return v.d;
}

float frexpf(float x, int* exp) {
    if (exp == nullptr) {
        errno = EINVAL;
        return 0.0f;
    }

    *exp = 0;
    if (x == 0.0f) {
        return 0.0f;
    }

    union {
        float f;
        std::uint32_t u;
    } v = { x };

    const std::uint32_t sign = v.u & 0x80000000U;
    std::uint32_t e = (v.u >> 23) & 0xFFU;

    if (e == 0xFFU) {  /* inf/nan */
        return x;
    }

    if (e == 0) {  /* subnormal */
        v.f = x * 16777216.0f; /* 2^24 */
        e = (v.u >> 23) & 0xFFU;
        if (e == 0) {
            return 0.0f;
        }
        *exp = static_cast<int>(e) - 127 - 24 + 1;
    } else {
        *exp = static_cast<int>(e) - 127 + 1;
    }

    v.u = sign | (static_cast<std::uint32_t>(126) << 23) | (v.u & 0x007FFFFFU);
    return v.f;
}

static bool is_nan(double x) {
    return x != x;
}

static double exp_approx(double x) {
    constexpr double ln2 = 0.693147180559945309417232121458176568;
    constexpr double inv_ln2 = 1.44269504088896340735992468100189214;

    int k = static_cast<int>(x * inv_ln2 + (x >= 0.0 ? 0.5 : -0.5));
    double r = x - static_cast<double>(k) * ln2;

    double term = 1.0;
    double sum = 1.0;
    term *= r; sum += term;
    term *= r / 2.0; sum += term;
    term *= r / 3.0; sum += term;
    term *= r / 4.0; sum += term;
    term *= r / 5.0; sum += term;
    term *= r / 6.0; sum += term;

    return ldexp(sum, k);
}

static double log_approx(double x) {
    if (x <= 0.0) {
        return 0.0 / 0.0;
    }

    int e = 0;
    double m = x;
    while (m >= 2.0) {
        m *= 0.5;
        ++e;
    }
    while (m < 1.0) {
        m *= 2.0;
        --e;
    }

    const double y = (m - 1.0) / (m + 1.0);
    const double y2 = y * y;
    double series = y;
    double term = y;
    term *= y2; series += term / 3.0;
    term *= y2; series += term / 5.0;
    term *= y2; series += term / 7.0;

    constexpr double ln2 = 0.693147180559945309417232121458176568;
    return 2.0 * series + static_cast<double>(e) * ln2;
}

double pow(double x, double y) {
    if (is_nan(x) || is_nan(y)) {
        return 0.0 / 0.0;
    }
    if (y == 0.0) {
        return 1.0;
    }
    if (x == 0.0) {
        return (y > 0.0) ? 0.0 : (1.0 / 0.0);
    }

    const double yi = static_cast<double>(static_cast<long long>(y));
    if (yi == y) {
        long long exp = static_cast<long long>(y);
        bool neg = false;
        if (exp < 0) {
            neg = true;
            exp = -exp;
        }
        double result = 1.0;
        double base = x;
        while (exp > 0) {
            if ((exp & 1LL) != 0) {
                result *= base;
            }
            base *= base;
            exp >>= 1;
        }
        return neg ? (1.0 / result) : result;
    }

    if (x < 0.0) {
        return 0.0 / 0.0;
    }

    return exp_approx(y * log_approx(x));
}

float powf(float x, float y) {
    return static_cast<float>(pow(static_cast<double>(x), static_cast<double>(y)));
}

double fmod(double x, double y) {
    return std::fmod(x, y);
}

float fmodf(float x, float y) {
    return static_cast<float>(std::fmod(static_cast<double>(x), static_cast<double>(y)));
}

static struct sigaction __signal_actions[NSIG] = {};

extern "C" void __instant_signal_restorer();
asm(
    ".global __instant_signal_restorer\n"
    "__instant_signal_restorer:\n"
    "mov $21, %rax\n"
    "syscall\n"
    "ud2\n"
);

static std::SigActionInfo to_kernel_sigaction(const struct sigaction& action) {
    std::SigActionInfo kernel = {};
    kernel.handler = reinterpret_cast<std::uint64_t>(action.sa_handler);
    kernel.mask = static_cast<std::uint64_t>(action.sa_mask);
    kernel.flags = static_cast<std::uint64_t>(action.sa_flags);
    kernel.restorer = reinterpret_cast<std::uint64_t>(&__instant_signal_restorer);
    return kernel;
}

static struct sigaction from_kernel_sigaction(const std::SigActionInfo& kernel) {
    struct sigaction action = {};
    action.sa_handler = reinterpret_cast<sighandler_t>(kernel.handler);
    action.sa_mask = static_cast<sigset_t>(kernel.mask);
    action.sa_flags = static_cast<int>(kernel.flags);
    return action;
}

static void refresh_signal_restart_state() {
    __signal_restart_enabled = false;
    for (int sig = 1; sig < NSIG; ++sig) {
        if (__signal_actions[sig].sa_handler != SIG_DFL &&
            __signal_actions[sig].sa_handler != SIG_IGN &&
            (__signal_actions[sig].sa_flags & SA_RESTART) != 0) {
            __signal_restart_enabled = true;
            return;
        }
    }
}

sighandler_t signal(int sig, sighandler_t handler) {
    if (sig <= 0 || sig >= NSIG || sig == SIGKILL) {
        errno = EINVAL;
        return SIG_ERR;
    }

    struct sigaction action = {};
    action.sa_handler = handler;
    std::SigActionInfo kernelAction = to_kernel_sigaction(action);
    std::SigActionInfo kernelPrevious = {};
    const auto result = std::sigaction(static_cast<std::uint64_t>(sig), &kernelAction, &kernelPrevious);
    if (syscall_failed(result)) {
        errno = syscall_errno(result, EINVAL);
        return SIG_ERR;
    }

    __signal_actions[sig] = action;
    refresh_signal_restart_state();
    return reinterpret_cast<sighandler_t>(kernelPrevious.handler);
}

int raise(int sig) {
    if (sig <= 0 || sig >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Kill),
        std::getpid(),
        static_cast<std::uint64_t>(sig)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EINVAL);
    }
    return 0;
}

int sigemptyset(sigset_t* set) {
    if (set == nullptr) {
        errno = EINVAL;
        return -1;
    }

    *set = 0;
    return 0;
}

int sigfillset(sigset_t* set) {
    if (set == nullptr) {
        errno = EINVAL;
        return -1;
    }

    *set = (1UL << NSIG) - 1UL;
    *set &= ~1UL;
    *set &= ~(1UL << SIGKILL);
    return 0;
}

int sigaddset(sigset_t* set, int sig) {
    if (set == nullptr || sig <= 0 || sig >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    if (sig != SIGKILL) {
        *set |= (1UL << sig);
    }
    return 0;
}

int sigdelset(sigset_t* set, int sig) {
    if (set == nullptr || sig <= 0 || sig >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1UL << sig);
    return 0;
}

int sigismember(const sigset_t* set, int sig) {
    if (set == nullptr || sig <= 0 || sig >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    return ((*set & (1UL << sig)) != 0) ? 1 : 0;
}

int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact) {
    if (sig <= 0 || sig >= NSIG || (act == nullptr && oldact == nullptr) ||
        (act != nullptr && sig == SIGKILL)) {
        errno = EINVAL;
        return -1;
    }

    if (act != nullptr) {
        std::SigActionInfo kernelAction = to_kernel_sigaction(*act);
        std::SigActionInfo kernelPrevious = {};
        const auto result = std::sigaction(static_cast<std::uint64_t>(sig), &kernelAction, &kernelPrevious);
        if (syscall_failed(result)) {
            errno = syscall_errno(result, EINVAL);
            return -1;
        }

        if (oldact != nullptr) {
            *oldact = from_kernel_sigaction(kernelPrevious);
        }

        __signal_actions[sig] = *act;
        refresh_signal_restart_state();
        return 0;
    }

    if (oldact != nullptr) {
        std::SigActionInfo kernelPrevious = {};
        const auto result = std::sigaction(static_cast<std::uint64_t>(sig), nullptr, &kernelPrevious);
        if (syscall_failed(result)) {
            errno = syscall_errno(result, EINVAL);
            return -1;
        }
        *oldact = from_kernel_sigaction(kernelPrevious);
    }

    return 0;
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
    if (how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK) {
        errno = EINVAL;
        return -1;
    }
    const auto result = std::sigprocmask(
        static_cast<std::uint64_t>(how),
        reinterpret_cast<const std::uint64_t*>(set),
        reinterpret_cast<std::uint64_t*>(oldset)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, EINVAL);
    }
    return 0;
}

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
    return sigprocmask(how, set, oldset);
}

int pthread_kill(pthread_t thread, int sig) {
    if (sig < 0 || sig >= NSIG) {
        return EINVAL;
    }
    if (sig == 0) {
        return 0;
    }
    if (thread == pthread_self()) {
        return raise(sig) == 0 ? 0 : errno;
    }

    const auto result = std::thread_signal(static_cast<std::ThreadHandle>(thread), static_cast<std::uint64_t>(sig));
    return syscall_failed(result) ? syscall_errno(result, EINVAL) : 0;
}

int sigaltstack(const stack_t* ss, stack_t* old_ss) {
    std::SignalStackInfo next = {};
    std::SignalStackInfo old = {};
    const std::SignalStackInfo* nextPtr = nullptr;
    std::SignalStackInfo* oldPtr = old_ss != nullptr ? &old : nullptr;

    if (ss != nullptr) {
        next.sp = reinterpret_cast<std::uint64_t>(ss->ss_sp);
        next.size = static_cast<std::uint64_t>(ss->ss_size);
        next.flags = static_cast<std::uint32_t>(ss->ss_flags);
        nextPtr = &next;
    }

    const auto result = std::sigaltstack(nextPtr, oldPtr);
    if (syscall_failed(result)) {
        return syscall_fail(result, EINVAL);
    }

    if (old_ss != nullptr) {
        old_ss->ss_sp = reinterpret_cast<void*>(old.sp);
        old_ss->ss_size = static_cast<size_t>(old.size);
        old_ss->ss_flags = static_cast<int>(old.flags);
    }
    return 0;
}

struct pthread_start_context {
    void* (*start)(void*);
    void* arg;
    volatile std::uint64_t handle;
};

struct pthread_record {
    pthread_t thread;
    std::uint64_t kernel_thread;
    int used;
    int cancelled;
    int cancel_state;
    int cancel_type;
    int errno_value;
    void* specific[PTHREAD_KEYS_MAX];
};

static pthread_record __pthread_records[64] = {};
static int __pthread_key_used[PTHREAD_KEYS_MAX] = {};
static void (*__pthread_key_destructors[PTHREAD_KEYS_MAX])(void*) = {};
static int __main_errno = 0;
static int __main_cancel_state = PTHREAD_CANCEL_ENABLE;
static int __main_cancel_type = PTHREAD_CANCEL_DEFERRED;
static void* __main_specific[PTHREAD_KEYS_MAX] = {};

static std::uint64_t current_kernel_thread() {
    return std::getpid();
}

static pthread_record* current_pthread_record() {
    const std::uint64_t current = current_kernel_thread();
    for (auto& record : __pthread_records) {
        if (__atomic_load_n(&record.used, __ATOMIC_ACQUIRE) && record.kernel_thread == current) {
            return &record;
        }
    }
    return nullptr;
}

static pthread_record* find_pthread_record(pthread_t thread) {
    for (auto& record : __pthread_records) {
        if (__atomic_load_n(&record.used, __ATOMIC_ACQUIRE) && record.thread == thread) {
            return &record;
        }
    }
    return nullptr;
}

static pthread_record* register_pthread_record(pthread_t thread) {
    pthread_record* existing = find_pthread_record(thread);
    if (existing != nullptr) {
        return existing;
    }
    for (auto& record : __pthread_records) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&record.used, &expected, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            record.thread = thread;
            record.kernel_thread = 0;
            record.cancel_state = PTHREAD_CANCEL_ENABLE;
            record.cancel_type = PTHREAD_CANCEL_DEFERRED;
            record.errno_value = 0;
            for (int key = 0; key < PTHREAD_KEYS_MAX; ++key) {
                record.specific[key] = nullptr;
            }
            __atomic_store_n(&record.cancelled, 0, __ATOMIC_RELEASE);
            return &record;
        }
    }
    return nullptr;
}

static void unregister_pthread_record(pthread_t thread) {
    pthread_record* record = find_pthread_record(thread);
    if (record != nullptr) {
        record->thread = 0;
        record->kernel_thread = 0;
        for (int key = 0; key < PTHREAD_KEYS_MAX; ++key) {
            record->specific[key] = nullptr;
        }
        __atomic_store_n(&record->cancelled, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&record->used, 0, __ATOMIC_RELEASE);
    }
}

static bool current_pthread_cancelled() {
    pthread_record* record = current_pthread_record();
    return record != nullptr && __atomic_load_n(&record->cancelled, __ATOMIC_ACQUIRE) != 0;
}

static bool should_cancel_current_pthread() {
    pthread_record* record = current_pthread_record();
    const int state = record != nullptr ? record->cancel_state : __main_cancel_state;
    return state == PTHREAD_CANCEL_ENABLE && current_pthread_cancelled();
}

static void run_pthread_key_destructors() {
    for (int pass = 0; pass < 4; ++pass) {
        bool invoked = false;
        for (int key = 0; key < PTHREAD_KEYS_MAX; ++key) {
            pthread_record* record = current_pthread_record();
            void** specific = record != nullptr ? record->specific : __main_specific;
            void* value = specific[key];
            auto destructor = __pthread_key_destructors[key];
            if (value != nullptr && destructor != nullptr && __atomic_load_n(&__pthread_key_used[key], __ATOMIC_ACQUIRE)) {
                specific[key] = nullptr;
                destructor(value);
                invoked = true;
            }
        }
        if (!invoked) {
            break;
        }
    }
}

extern "C" int* __errno_location(void) {
    pthread_record* record = current_pthread_record();
    return record != nullptr ? &record->errno_value : &__main_errno;
}

static void pthread_start_trampoline(void* raw) {
    auto* context = static_cast<pthread_start_context*>(raw);
    if (context == nullptr || context->start == nullptr) {
        std::thread_exit(static_cast<std::uint64_t>(EINVAL));
    }

    while (__atomic_load_n(&context->handle, __ATOMIC_ACQUIRE) == 0) {
        std::yield();
    }
    const pthread_t self = static_cast<pthread_t>(__atomic_load_n(&context->handle, __ATOMIC_ACQUIRE));
    pthread_record* record = register_pthread_record(self);
    if (record != nullptr) {
        record->kernel_thread = current_kernel_thread();
    }

    void* (*start)(void*) = context->start;
    void* arg = context->arg;
    std::free(context);
    void* result = start(arg);
    pthread_exit(result);
}

int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg) {
    (void)attr;
    if (thread == nullptr || start_routine == nullptr) {
        return EINVAL;
    }

    auto* context = static_cast<pthread_start_context*>(std::malloc(sizeof(pthread_start_context)));
    if (context == nullptr) {
        return ENOMEM;
    }
    context->start = start_routine;
    context->arg = arg;
    context->handle = 0;

    const std::ThreadHandle handle = std::thread_create(&pthread_start_trampoline, context);
    if (handle == static_cast<std::ThreadHandle>(-1)) {
        std::free(context);
        return ENOMEM;
    }

    __atomic_store_n(&context->handle, static_cast<std::uint64_t>(handle), __ATOMIC_RELEASE);
    register_pthread_record(static_cast<pthread_t>(handle));
    *thread = static_cast<pthread_t>(handle);
    return 0;
}

int pthread_join(pthread_t thread, void** retval) {
    int status = 0;
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::ThreadJoin),
        static_cast<std::uint64_t>(thread),
        reinterpret_cast<std::uint64_t>(&status)
    );
    if (syscall_failed(result)) {
        return syscall_errno(result, EINVAL);
    }
    if (retval != nullptr) {
        *retval = reinterpret_cast<void*>(static_cast<std::uint64_t>(status));
    }
    unregister_pthread_record(thread);
    return 0;
}

void pthread_exit(void* retval) {
    run_pthread_key_destructors();
    unregister_pthread_record(pthread_self());
    std::thread_exit(reinterpret_cast<std::uint64_t>(retval));
}

pthread_t pthread_self(void) {
    pthread_record* record = current_pthread_record();
    if (record != nullptr) {
        return record->thread;
    }
    return static_cast<pthread_t>(std::getpid());
}

int pthread_equal(pthread_t a, pthread_t b) {
    return a == b;
}

int pthread_detach(pthread_t thread) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Close),
        static_cast<std::uint64_t>(thread)
    );
    if (syscall_failed(result)) {
        return syscall_errno(result, EINVAL);
    }
    unregister_pthread_record(thread);
    return 0;
}

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    if (key == nullptr) {
        return EINVAL;
    }
    for (int i = 0; i < PTHREAD_KEYS_MAX; ++i) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&__pthread_key_used[i], &expected, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            __pthread_key_destructors[i] = destructor;
            *key = static_cast<pthread_key_t>(i);
            return 0;
        }
    }
    return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !__atomic_load_n(&__pthread_key_used[key], __ATOMIC_ACQUIRE)) {
        return EINVAL;
    }
    __pthread_key_destructors[key] = nullptr;
    __main_specific[key] = nullptr;
    for (auto& record : __pthread_records) {
        if (__atomic_load_n(&record.used, __ATOMIC_ACQUIRE)) {
            record.specific[key] = nullptr;
        }
    }
    __atomic_store_n(&__pthread_key_used[key], 0, __ATOMIC_RELEASE);
    return 0;
}

void* pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !__atomic_load_n(&__pthread_key_used[key], __ATOMIC_ACQUIRE)) {
        return nullptr;
    }
    pthread_record* record = current_pthread_record();
    return record != nullptr ? record->specific[key] : __main_specific[key];
}

int pthread_setspecific(pthread_key_t key, const void* value) {
    if (key >= PTHREAD_KEYS_MAX || !__atomic_load_n(&__pthread_key_used[key], __ATOMIC_ACQUIRE)) {
        return EINVAL;
    }
    pthread_record* record = current_pthread_record();
    if (record != nullptr) {
        record->specific[key] = const_cast<void*>(value);
    } else {
        __main_specific[key] = const_cast<void*>(value);
    }
    return 0;
}

int pthread_cancel(pthread_t thread) {
    pthread_record* record = find_pthread_record(thread);
    if (record == nullptr) {
        return ESRCH;
    }
    __atomic_store_n(&record->cancelled, 1, __ATOMIC_RELEASE);
    return 0;
}

void pthread_testcancel(void) {
    if (should_cancel_current_pthread()) {
        pthread_exit(PTHREAD_CANCELED);
    }
}

int pthread_setcancelstate(int state, int* oldstate) {
    if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
        return EINVAL;
    }
    pthread_record* record = current_pthread_record();
    if (oldstate != nullptr) {
        *oldstate = record != nullptr ? record->cancel_state : __main_cancel_state;
    }
    if (record != nullptr) {
        record->cancel_state = state;
    } else {
        __main_cancel_state = state;
    }
    if (state == PTHREAD_CANCEL_ENABLE) {
        pthread_testcancel();
    }
    return 0;
}

int pthread_setcanceltype(int type, int* oldtype) {
    if (type != PTHREAD_CANCEL_DEFERRED && type != PTHREAD_CANCEL_ASYNCHRONOUS) {
        return EINVAL;
    }
    pthread_record* record = current_pthread_record();
    if (oldtype != nullptr) {
        *oldtype = record != nullptr ? record->cancel_type : __main_cancel_type;
    }
    if (record != nullptr) {
        record->cancel_type = type;
    } else {
        __main_cancel_type = type;
    }
    if (type == PTHREAD_CANCEL_ASYNCHRONOUS) {
        pthread_testcancel();
    }
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr) {
    if (attr == nullptr) {
        return EINVAL;
    }
    attr->type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t* attr) {
    return attr == nullptr ? EINVAL : 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type) {
    if (attr == nullptr ||
        (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_RECURSIVE && type != PTHREAD_MUTEX_ERRORCHECK && type != PTHREAD_MUTEX_DEFAULT)) {
        return EINVAL;
    }
    attr->type = type;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type) {
    if (attr == nullptr || type == nullptr) {
        return EINVAL;
    }
    *type = attr->type;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    if (mutex == nullptr) {
        return EINVAL;
    }
    __atomic_store_n(&mutex->lock, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&mutex->owner, 0UL, __ATOMIC_RELEASE);
    mutex->recursion = 0;
    mutex->type = attr != nullptr ? attr->type : PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (mutex == nullptr) {
        return EINVAL;
    }
    return __atomic_load_n(&mutex->lock, __ATOMIC_ACQUIRE) == 0 ? 0 : EBUSY;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    if (mutex == nullptr) {
        return EINVAL;
    }

    const pthread_t self = pthread_self();
    if (__atomic_load_n(&mutex->lock, __ATOMIC_ACQUIRE) != 0 &&
        __atomic_load_n(&mutex->owner, __ATOMIC_ACQUIRE) == self) {
        if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
            mutex->recursion++;
            return 0;
        }
        return mutex->type == PTHREAD_MUTEX_ERRORCHECK ? EDEADLK : EBUSY;
    }

    int expected = 0;
    if (!__atomic_compare_exchange_n(&mutex->lock, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return EBUSY;
    }
    __atomic_store_n(&mutex->owner, self, __ATOMIC_RELEASE);
    mutex->recursion = 1;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (mutex == nullptr) {
        return EINVAL;
    }

    for (;;) {
        const int result = pthread_mutex_trylock(mutex);
        if (result == 0 || result == EDEADLK) {
            return result;
        }
        std::yield();
        pthread_testcancel();
    }
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (mutex == nullptr) {
        return EINVAL;
    }
    if (__atomic_load_n(&mutex->lock, __ATOMIC_ACQUIRE) == 0 ||
        __atomic_load_n(&mutex->owner, __ATOMIC_ACQUIRE) != pthread_self()) {
        return EPERM;
    }
    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->recursion > 1) {
        mutex->recursion--;
        return 0;
    }
    mutex->recursion = 0;
    __atomic_store_n(&mutex->owner, 0UL, __ATOMIC_RELEASE);
    __atomic_store_n(&mutex->lock, 0, __ATOMIC_RELEASE);
    return 0;
}

static bool instant_timespec_reached(const struct timespec* abstime) {
    if (abstime == nullptr) {
        return false;
    }
    struct timespec now {};
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return false;
    }
    return now.tv_sec > abstime->tv_sec ||
           (now.tv_sec == abstime->tv_sec && now.tv_nsec >= abstime->tv_nsec);
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
    (void)attr;
    if (cond == nullptr) {
        return EINVAL;
    }
    __atomic_store_n(&cond->waiters, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&cond->signals, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&cond->broadcast, 0, __ATOMIC_RELEASE);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
    if (cond == nullptr) {
        return EINVAL;
    }
    return __atomic_load_n(&cond->waiters, __ATOMIC_ACQUIRE) == 0 ? 0 : EBUSY;
}

int pthread_cond_signal(pthread_cond_t* cond) {
    if (cond == nullptr) {
        return EINVAL;
    }
    if (__atomic_load_n(&cond->waiters, __ATOMIC_ACQUIRE) > 0) {
        __atomic_add_fetch(&cond->signals, 1, __ATOMIC_RELEASE);
    }
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
    if (cond == nullptr) {
        return EINVAL;
    }
    const int waiters = __atomic_load_n(&cond->waiters, __ATOMIC_ACQUIRE);
    if (waiters > 0) {
        __atomic_add_fetch(&cond->signals, waiters, __ATOMIC_RELEASE);
        __atomic_add_fetch(&cond->broadcast, 1, __ATOMIC_RELEASE);
    }
    return 0;
}

static int pthread_cond_wait_until(pthread_cond_t* cond, pthread_mutex_t* mutex,
                                   const struct timespec* abstime) {
    if (cond == nullptr || mutex == nullptr) {
        return EINVAL;
    }

    __atomic_add_fetch(&cond->waiters, 1, __ATOMIC_ACQ_REL);
    const int observedBroadcast = __atomic_load_n(&cond->broadcast, __ATOMIC_ACQUIRE);
    int err = pthread_mutex_unlock(mutex);
    if (err != 0) {
        __atomic_sub_fetch(&cond->waiters, 1, __ATOMIC_ACQ_REL);
        return err;
    }

    for (;;) {
        int signals = __atomic_load_n(&cond->signals, __ATOMIC_ACQUIRE);
        while (signals > 0) {
            if (__atomic_compare_exchange_n(&cond->signals, &signals, signals - 1, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
                __atomic_sub_fetch(&cond->waiters, 1, __ATOMIC_ACQ_REL);
                return pthread_mutex_lock(mutex);
            }
        }

        if (__atomic_load_n(&cond->broadcast, __ATOMIC_ACQUIRE) != observedBroadcast) {
            __atomic_sub_fetch(&cond->waiters, 1, __ATOMIC_ACQ_REL);
            return pthread_mutex_lock(mutex);
        }

        if (abstime != nullptr && instant_timespec_reached(abstime)) {
            __atomic_sub_fetch(&cond->waiters, 1, __ATOMIC_ACQ_REL);
            pthread_mutex_lock(mutex);
            return ETIMEDOUT;
        }
        if (should_cancel_current_pthread()) {
            __atomic_sub_fetch(&cond->waiters, 1, __ATOMIC_ACQ_REL);
            pthread_mutex_lock(mutex);
            pthread_exit(PTHREAD_CANCELED);
        }
        std::yield();
    }
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    return pthread_cond_wait_until(cond, mutex, nullptr);
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime) {
    if (abstime == nullptr || abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L) {
        return EINVAL;
    }
    return pthread_cond_wait_until(cond, mutex, abstime);
}

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr) {
    (void)attr;
    if (rwlock == nullptr) {
        return EINVAL;
    }
    __atomic_store_n(&rwlock->readers, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&rwlock->writer, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&rwlock->pending_writers, 0, __ATOMIC_RELEASE);
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t* rwlock) {
    if (rwlock == nullptr) {
        return EINVAL;
    }
    return __atomic_load_n(&rwlock->writer, __ATOMIC_ACQUIRE) == 0 &&
           __atomic_load_n(&rwlock->readers, __ATOMIC_ACQUIRE) == 0 ? 0 : EBUSY;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock) {
    if (rwlock == nullptr) {
        return EINVAL;
    }
    if (__atomic_load_n(&rwlock->writer, __ATOMIC_ACQUIRE) != 0 ||
        __atomic_load_n(&rwlock->pending_writers, __ATOMIC_ACQUIRE) != 0) {
        return EBUSY;
    }
    __atomic_add_fetch(&rwlock->readers, 1, __ATOMIC_ACQUIRE);
    if (__atomic_load_n(&rwlock->writer, __ATOMIC_ACQUIRE) != 0 ||
        __atomic_load_n(&rwlock->pending_writers, __ATOMIC_ACQUIRE) != 0) {
        __atomic_sub_fetch(&rwlock->readers, 1, __ATOMIC_RELEASE);
        return EBUSY;
    }
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock) {
    if (rwlock == nullptr) {
        return EINVAL;
    }
    while (pthread_rwlock_tryrdlock(rwlock) == EBUSY) {
        std::yield();
        pthread_testcancel();
    }
    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock) {
    if (rwlock == nullptr) {
        return EINVAL;
    }
    int expected = 0;
    if (!__atomic_compare_exchange_n(&rwlock->writer, &expected, 1, false,
                                     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return EBUSY;
    }
    if (__atomic_load_n(&rwlock->readers, __ATOMIC_ACQUIRE) != 0) {
        __atomic_store_n(&rwlock->writer, 0, __ATOMIC_RELEASE);
        return EBUSY;
    }
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock) {
    if (rwlock == nullptr) {
        return EINVAL;
    }
    __atomic_add_fetch(&rwlock->pending_writers, 1, __ATOMIC_ACQ_REL);
    for (;;) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&rwlock->writer, &expected, 1, false,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            while (__atomic_load_n(&rwlock->readers, __ATOMIC_ACQUIRE) != 0) {
                std::yield();
                pthread_testcancel();
            }
            __atomic_sub_fetch(&rwlock->pending_writers, 1, __ATOMIC_ACQ_REL);
            return 0;
        }
        if (should_cancel_current_pthread()) {
            __atomic_sub_fetch(&rwlock->pending_writers, 1, __ATOMIC_ACQ_REL);
            pthread_exit(PTHREAD_CANCELED);
        }
        std::yield();
    }
}

int pthread_rwlock_unlock(pthread_rwlock_t* rwlock) {
    if (rwlock == nullptr) {
        return EINVAL;
    }
    if (__atomic_load_n(&rwlock->writer, __ATOMIC_ACQUIRE) != 0) {
        __atomic_store_n(&rwlock->writer, 0, __ATOMIC_RELEASE);
        return 0;
    }
    int readers = __atomic_load_n(&rwlock->readers, __ATOMIC_ACQUIRE);
    while (readers > 0) {
        if (__atomic_compare_exchange_n(&rwlock->readers, &readers, readers - 1, false,
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            return 0;
        }
    }
    return EPERM;
}

int sem_init(sem_t* sem, int pshared, unsigned int value) {
    if (sem == nullptr || pshared != 0 || value > 2147483647U) {
        return EINVAL;
    }
    __atomic_store_n(&sem->value, static_cast<int>(value), __ATOMIC_RELEASE);
    __atomic_store_n(&sem->waiters, 0, __ATOMIC_RELEASE);
    return 0;
}

int sem_destroy(sem_t* sem) {
    if (sem == nullptr) {
        errno = EINVAL;
        return -1;
    }
    if (__atomic_load_n(&sem->waiters, __ATOMIC_ACQUIRE) != 0) {
        errno = EBUSY;
        return -1;
    }
    return 0;
}

int sem_trywait(sem_t* sem) {
    if (sem == nullptr) {
        return EINVAL;
    }
    int current = __atomic_load_n(&sem->value, __ATOMIC_ACQUIRE);
    while (current > 0) {
        if (__atomic_compare_exchange_n(&sem->value, &current, current - 1, false,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            return 0;
        }
    }
    errno = EAGAIN;
    return -1;
}

int sem_wait(sem_t* sem) {
    if (sem == nullptr) {
        errno = EINVAL;
        return -1;
    }
    __atomic_add_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
    while (sem_trywait(sem) != 0) {
        if (errno != EAGAIN) {
            __atomic_sub_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
            return -1;
        }
        std::yield();
        if (should_cancel_current_pthread()) {
            __atomic_sub_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
            pthread_exit(PTHREAD_CANCELED);
        }
    }
    __atomic_sub_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
    return 0;
}

int sem_timedwait(sem_t* sem, const struct timespec* abstime) {
    if (sem == nullptr || abstime == nullptr ||
        abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    __atomic_add_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
    while (sem_trywait(sem) != 0) {
        if (errno != EAGAIN) {
            __atomic_sub_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
            return -1;
        }
        if (instant_timespec_reached(abstime)) {
            __atomic_sub_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
            errno = ETIMEDOUT;
            return -1;
        }
        std::yield();
        if (should_cancel_current_pthread()) {
            __atomic_sub_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
            pthread_exit(PTHREAD_CANCELED);
        }
    }
    __atomic_sub_fetch(&sem->waiters, 1, __ATOMIC_ACQ_REL);
    return 0;
}

int sem_post(sem_t* sem) {
    if (sem == nullptr) {
        errno = EINVAL;
        return -1;
    }
    int current = __atomic_load_n(&sem->value, __ATOMIC_ACQUIRE);
    for (;;) {
        if (current == 2147483647) {
            errno = EOVERFLOW;
            return -1;
        }
        if (__atomic_compare_exchange_n(&sem->value, &current, current + 1, false,
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            return 0;
        }
    }
}

int sem_getvalue(sem_t* sem, int* sval) {
    if (sem == nullptr || sval == nullptr) {
        errno = EINVAL;
        return -1;
    }
    *sval = __atomic_load_n(&sem->value, __ATOMIC_ACQUIRE);
    return 0;
}

static int socket_unimplemented() {
    errno = ENOSYS;
    return -1;
}

uint16_t htons(uint16_t hostshort) {
    return static_cast<uint16_t>((hostshort >> 8) | (hostshort << 8));
}

uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);
}

uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0x000000ffU) << 24) |
           ((hostlong & 0x0000ff00U) << 8) |
           ((hostlong & 0x00ff0000U) >> 8) |
           ((hostlong & 0xff000000U) >> 24);
}

uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

int socket(int domain, int type, int protocol) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Socket),
        static_cast<std::uint64_t>(domain),
        static_cast<std::uint64_t>(type),
        static_cast<std::uint64_t>(protocol)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOSYS);
    }
    return register_file_handle(result);
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    (void)domain;
    (void)type;
    (void)protocol;
    (void)sv;
    return socket_unimplemented();
}

int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Bind),
        fd_to_handle(sockfd),
        reinterpret_cast<std::uint64_t>(addr),
        static_cast<std::uint64_t>(addrlen)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOTSOCK);
    }
    return 0;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Connect),
        fd_to_handle(sockfd),
        reinterpret_cast<std::uint64_t>(addr),
        static_cast<std::uint64_t>(addrlen)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOTSOCK);
    }
    return 0;
}

int listen(int sockfd, int backlog) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Listen),
        fd_to_handle(sockfd),
        static_cast<std::uint64_t>(backlog)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOTSOCK);
    }
    return 0;
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Accept),
        fd_to_handle(sockfd),
        reinterpret_cast<std::uint64_t>(addr),
        reinterpret_cast<std::uint64_t>(addrlen)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOTSOCK);
    }
    return register_file_handle(result);
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Send),
        fd_to_handle(sockfd),
        reinterpret_cast<std::uint64_t>(buf),
        static_cast<std::uint64_t>(len),
        static_cast<std::uint64_t>(flags)
    );
    if (syscall_failed(result)) {
        errno = syscall_errno(result, ENOTSOCK);
        return static_cast<ssize_t>(-1);
    }
    return static_cast<ssize_t>(result);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Recv),
        fd_to_handle(sockfd),
        reinterpret_cast<std::uint64_t>(buf),
        static_cast<std::uint64_t>(len),
        static_cast<std::uint64_t>(flags)
    );
    if (syscall_failed(result)) {
        errno = syscall_errno(result, ENOTSOCK);
        return static_cast<ssize_t>(-1);
    }
    return static_cast<ssize_t>(result);
}

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen) {
    if (dest_addr != nullptr && connect(sockfd, dest_addr, addrlen) != 0) {
        return -1;
    }
    return send(sockfd, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
    const ssize_t result = recv(sockfd, buf, len, flags);
    if (result >= 0 && src_addr != nullptr && addrlen != nullptr) {
        *addrlen = 0;
    }
    return result;
}

int shutdown(int sockfd, int how) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::Shutdown),
        fd_to_handle(sockfd),
        static_cast<std::uint64_t>(how)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOTSOCK);
    }
    return 0;
}

int getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::GetSockOpt),
        fd_to_handle(sockfd),
        static_cast<std::uint64_t>(level),
        static_cast<std::uint64_t>(optname),
        reinterpret_cast<std::uint64_t>(optval),
        reinterpret_cast<std::uint64_t>(optlen)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOTSOCK);
    }
    return 0;
}

int setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    const auto result = _syscall_impl(
        static_cast<std::uint64_t>(std::Syscall::SetSockOpt),
        fd_to_handle(sockfd),
        static_cast<std::uint64_t>(level),
        static_cast<std::uint64_t>(optname),
        reinterpret_cast<std::uint64_t>(optval),
        static_cast<std::uint64_t>(optlen)
    );
    if (syscall_failed(result)) {
        return syscall_fail(result, ENOTSOCK);
    }
    return 0;
}

static int parse_ipv4_octet(const char*& p, uint32_t* value) {
    if (p == nullptr || *p < '0' || *p > '9') {
        return -1;
    }
    uint32_t out = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        out = out * 10U + static_cast<uint32_t>(*p - '0');
        if (out > 255U) {
            return -1;
        }
        ++p;
        ++digits;
    }
    if (digits == 0) {
        return -1;
    }
    *value = out;
    return 0;
}

int inet_pton(int af, const char* src, void* dst) {
    if (src == nullptr || dst == nullptr) {
        errno = EINVAL;
        return -1;
    }
    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    const char* p = src;
    uint32_t parts[4] = {};
    for (int i = 0; i < 4; ++i) {
        if (parse_ipv4_octet(p, &parts[i]) != 0) {
            return 0;
        }
        if (i < 3) {
            if (*p != '.') {
                return 0;
            }
            ++p;
        }
    }
    if (*p != '\0') {
        return 0;
    }

    uint32_t host = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    static_cast<struct in_addr*>(dst)->s_addr = htonl(host);
    return 1;
}

in_addr_t inet_addr(const char* cp) {
    struct in_addr addr {};
    return inet_pton(AF_INET, cp, &addr) == 1 ? addr.s_addr : INADDR_NONE;
}

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
    if (src == nullptr || dst == nullptr) {
        errno = EINVAL;
        return nullptr;
    }
    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return nullptr;
    }

    uint32_t host = ntohl(static_cast<const struct in_addr*>(src)->s_addr);
    const unsigned int a = (host >> 24) & 0xffU;
    const unsigned int b = (host >> 16) & 0xffU;
    const unsigned int c = (host >> 8) & 0xffU;
    const unsigned int d = host & 0xffU;
    const int needed = snprintf(dst, size, "%u.%u.%u.%u", a, b, c, d);
    if (needed < 0 || static_cast<socklen_t>(needed + 1) > size) {
        errno = ENOSPC;
        return nullptr;
    }
    return dst;
}

int getaddrinfo(const char* node, const char* service,
                const struct addrinfo* hints, struct addrinfo** res) {
    (void)node;
    (void)service;
    (void)hints;
    if (res != nullptr) {
        *res = nullptr;
    }
    return EAI_NONAME;
}

void freeaddrinfo(struct addrinfo* res) {
    (void)res;
}

const char* gai_strerror(int errcode) {
    switch (errcode) {
        case 0: return "Success";
        case EAI_BADFLAGS: return "Bad flags";
        case EAI_NONAME: return "Name or service not known";
        case EAI_AGAIN: return "Temporary failure";
        case EAI_FAIL: return "Non-recoverable failure";
        case EAI_MEMORY: return "Memory allocation failure";
        case EAI_SYSTEM: return "System error";
        default: return "Unknown getaddrinfo error";
    }
}

struct hostent* gethostbyname(const char* name) {
    (void)name;
    errno = ENOSYS;
    return nullptr;
}

static struct lconv __instant_lconv = {
    (char*)".",     /* decimal_point */
    (char*)"",      /* thousands_sep */
    (char*)"",      /* grouping */
    (char*)"",      /* int_curr_symbol */
    (char*)"",      /* currency_symbol */
    (char*)".",     /* mon_decimal_point */
    (char*)"",      /* mon_thousands_sep */
    (char*)"",      /* mon_grouping */
    (char*)"",      /* positive_sign */
    (char*)"",      /* negative_sign */
    127,            /* int_frac_digits */
    127,            /* frac_digits */
    127,            /* p_cs_precedes */
    127,            /* p_sep_by_space */
    127,            /* n_cs_precedes */
    127,            /* n_sep_by_space */
    127,            /* p_sign_posn */
    127,            /* n_sign_posn */
    127,            /* int_p_cs_precedes */
    127,            /* int_p_sep_by_space */
    127,            /* int_n_cs_precedes */
    127,            /* int_n_sep_by_space */
    127,            /* int_p_sign_posn */
    127             /* int_n_sign_posn */
};

char* setlocale(int category, const char* locale) {
    (void)category;
    (void)locale;
    /* Only the "C" locale is supported. */
    return (char*)"C";
}

struct lconv* localeconv(void) {
    return &__instant_lconv;
}

}
