#pragma once

#include <cstddef.hpp>
#include <cstdint.hpp>

extern "C" std::uint64_t _syscall_impl(
    std::uint64_t number,
    std::uint64_t arg1 = 0,
    std::uint64_t arg2 = 0,
    std::uint64_t arg3 = 0,
    std::uint64_t arg4 = 0,
    std::uint64_t arg5 = 0
);

namespace std {
struct OSInfo {
    char osname[10];
    char loggedOnUser[32];
    char cpuname[64];
    char maxRamGB[8];
    char usedRamGB[8];
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint64_t buildnum;
};

struct LoginInfo {
    char username[32];
    char password[64];
};

struct SessionInfo {
    uint32_t sessionID;
    uint32_t uid;
    uint32_t gid;
    uint32_t leaderPID;
    uint64_t loginTime;
    uint8_t state;
};

struct Stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint64_t st_size;
    uint64_t st_blksize;
    uint64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

struct UserInfo {
    uint32_t uid;
    uint32_t gid;
    char username[32];
    char homeDir[256];
    char shell[256];
};

struct SurfaceInfo {
    std::uint64_t id;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t format;
    std::uint32_t pitch;
    std::uint32_t dirtyX;
    std::uint32_t dirtyY;
    std::uint32_t dirtyWidth;
    std::uint32_t dirtyHeight;
    std::uint64_t address;
};

enum WindowStateFlags : std::uint32_t {
    WindowStateNone = 0,
    WindowStateFocused = 1 << 0,
    WindowStateMinimized = 1 << 1,
    WindowStateMaximized = 1 << 2,
    WindowStateClosed = 1 << 3
};

enum class WindowControlAction : std::uint32_t {
    Restore = 0,
    Minimize = 1,
    Maximize = 2,
    Close = 3
};

struct WindowInfo {
    std::uint64_t id;
    std::uint32_t ownerPID;
    std::uint32_t flags;
    std::uint32_t state;
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
    std::uint64_t surfaceID;
    std::uint32_t zOrder;
    char title[64];
};

struct IPCMessage {
    uint64_t id;
    uint32_t senderPID;
    uint16_t flags;
    uint16_t reserved;
    uint64_t size;
    uint8_t data[256];
};

inline constexpr uint16_t IPC_MESSAGE_REQUEST = 1 << 0;
inline constexpr uint16_t IPC_MESSAGE_EVENT = 1 << 1;

enum class EventType : uint16_t {
    None = 0,
    Key,
    Pointer,
    Window
};

enum class KeyEventAction : uint16_t {
    Press = 0,
    Release,
    Repeat
};

enum KeyModifiers : uint16_t {
    KeyModifierNone = 0,
    KeyModifierShift = 1 << 0,
    KeyModifierControl = 1 << 1,
    KeyModifierAlt = 1 << 2,
    KeyModifierCapsLock = 1 << 3
};

enum class PointerEventAction : uint16_t {
    Move = 0,
    Button,
    Scroll
};

enum class WindowEventAction : uint16_t {
    None = 0,
    FocusGained,
    FocusLost,
    CloseRequested,
    Resized,
    Moved
};

struct KeyEvent {
    KeyEventAction action;
    uint16_t modifiers;
    uint16_t keycode;
    uint16_t reserved;
    char text[8];
};

struct PointerEvent {
    PointerEventAction action;
    uint16_t buttons;
    int32_t x;
    int32_t y;
    int32_t deltaX;
    int32_t deltaY;
};

struct WindowEvent {
    WindowEventAction action;
    uint16_t reserved0;
    uint32_t windowId;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

struct Event {
    EventType type;
    uint16_t reserved0;
    uint32_t reserved1;
    union {
        KeyEvent key;
        PointerEvent pointer;
        WindowEvent window;
        uint8_t raw[48];
    };
};

enum class FileType : int {
    Regular,
    Directory,
    CharDevice,
    BlockDevice,
    Symlink,
    Pipe,
    Socket
};

struct DirEntry {
    char name[256];
    uint64_t inode;
    FileType type;
};

using Handle = uint64_t;
using FileHandle = Handle;
using ThreadHandle = Handle;
using ThreadStartRoutine = void (*)(void*);

enum class HandleType : uint16_t {
    None = 0,
    File,
    Process,
    Thread,
    Window,
    Surface,
    EventQueue,
    Service,
    Timer,
    SharedMemory,
    GpuContext,
    Font,
    Pipe
};

inline constexpr std::uint64_t HANDLE_TYPE_SHIFT = 48;
inline constexpr std::uint64_t HANDLE_SLOT_MASK = 0xFFFFFFFFULL;

constexpr Handle make_handle(HandleType type, std::uint64_t slot) noexcept {
    return (static_cast<std::uint64_t>(type) << HANDLE_TYPE_SHIFT) | (slot & HANDLE_SLOT_MASK);
}

inline constexpr FileHandle STDIN_HANDLE = 0;
inline constexpr FileHandle STDOUT_HANDLE = 1;
inline constexpr FileHandle STDERR_HANDLE = 2;

struct FBInfo {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t pixelFormat;
    uint64_t fbSize;
    uint32_t redMask;
    uint32_t greenMask;
    uint32_t blueMask;
};

inline constexpr int NSIG = 32;
inline constexpr int SIGKILL = 9;
inline constexpr int SIGSEGV = 11;
inline constexpr int SIGTERM = 15;

using sighandler_t = void (*)(int);

enum class Syscall : uint64_t {
    OSInfo,
    ProcInfo,
    Exit,
    Write,
    Read,
    Open,
    Close,
    GetPID,
    Fork,
    Exec,
    Wait,
    Kill,
    Mmap,
    Munmap,
    Yield,
    Sleep,
    GetTime,
    Clear,
    FBInfo,
    FBMap,
    Signal,
    SigReturn,
    Login,
    Logout,
    GetUID,
    GetGID,
    SetUID,
    SetGID,
    GetSessionID,
    GetSessionInfo,
    Chdir,
    Getcwd,
    Mkdir,
    Rmdir,
    Unlink,
    Stat,
    Dup,
    Dup2,
    Pipe,
    Getppid,
    Spawn,
    GetUserInfo,
    Readdir,
    FBFlush,
    SharedAlloc,
    SharedMap,
    SharedFree,
    SurfaceCreate,
    SurfaceMap,
    SurfaceCommit,
    SurfacePoll,
    CompositorCreateWindow,
    WindowEventQueue,
    WindowSetTitle,
    WindowAttachSurface,
    WindowList,
    WindowFocus,
    WindowMove,
    WindowResize,
    WindowControl,
    QueueCreate,
    QueueSend,
    QueueReceive,
    QueueReply,
    QueueRequest,
    ServiceRegister,
    ServiceConnect,
    NetGetMAC,
    NetSend,
    NetRecv,
    NetLinkStatus,
    NetPing,
    NetProcessPackets,
    NetGetPingReply,
    ThreadCreate,
    ThreadExit,
    ThreadJoin,
};

ThreadHandle thread_create(ThreadStartRoutine start, void* arg = nullptr, std::uint64_t stackSize = 0) noexcept;
[[noreturn]] void thread_exit(std::uint64_t code = 0) noexcept;
std::uint64_t thread_join(ThreadHandle handle, int* status = nullptr) noexcept;

inline std::uint64_t osinfo(OSInfo* info) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::OSInfo),
        reinterpret_cast<std::uint64_t>(info)
    );
}

[[noreturn]] inline void exit(std::uint64_t code) noexcept {
    _syscall_impl(static_cast<std::uint64_t>(Syscall::Exit), code);
    for (;;) {
        asm volatile("");
    }
}

inline std::uint64_t write(FileHandle fileHandle, const void* buffer, std::uint64_t count) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Write),
        fileHandle,
        reinterpret_cast<std::uint64_t>(buffer),
        count
    );
}

inline std::uint64_t read(FileHandle fileHandle, void* buffer, std::uint64_t count) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Read),
        fileHandle,
        reinterpret_cast<std::uint64_t>(buffer),
        count
    );
}

inline FileHandle open(
    const char* path,
    std::uint64_t flags = 0,
    std::uint64_t mode = 0
) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Open),
        reinterpret_cast<std::uint64_t>(path),
        flags,
        mode
    );
}

inline std::uint64_t close(Handle handle) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Close), handle);
}

inline std::uint64_t getpid() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::GetPID));
}

inline std::uint64_t fork() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Fork));
}

inline std::uint64_t exec(
    const char* path,
    const char* const* argv = nullptr,
    const char* const* envp = nullptr
) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Exec),
        reinterpret_cast<std::uint64_t>(path),
        reinterpret_cast<std::uint64_t>(argv),
        reinterpret_cast<std::uint64_t>(envp)
    );
}

inline std::uint64_t wait(std::uint64_t pid, int* status = nullptr) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Wait),
        pid,
        reinterpret_cast<std::uint64_t>(status)
    );
}

inline std::uint64_t kill(std::uint64_t pid, std::uint64_t sig) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Kill), pid, sig);
}

inline void* mmap(void* address, std::uint64_t length, std::uint64_t protection = 0) noexcept {
    return reinterpret_cast<void*>(_syscall_impl(
        static_cast<std::uint64_t>(Syscall::Mmap),
        reinterpret_cast<std::uint64_t>(address),
        length,
        protection
    ));
}

inline std::uint64_t munmap(void* address, std::uint64_t length) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Munmap),
        reinterpret_cast<std::uint64_t>(address),
        length
    );
}

inline std::uint64_t yield() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Yield));
}

inline std::uint64_t sleep(std::uint64_t ms) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Sleep), ms);
}

inline std::uint64_t gettime() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::GetTime));
}

inline std::uint64_t clear() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Clear));
}

inline std::uint64_t fb_info(FBInfo* info) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::FBInfo),
        reinterpret_cast<std::uint64_t>(info)
    );
}

inline void* fb_map() noexcept {
    return reinterpret_cast<void*>(_syscall_impl(static_cast<std::uint64_t>(Syscall::FBMap)));
}

inline std::uint64_t fb_flush(
    std::uint64_t x,
    std::uint64_t y,
    std::uint64_t w,
    std::uint64_t h
) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::FBFlush),
        x,
        y,
        w,
        h
    );
}

inline sighandler_t signal(std::uint64_t sig, sighandler_t handler) noexcept {
    return reinterpret_cast<sighandler_t>(_syscall_impl(
        static_cast<std::uint64_t>(Syscall::Signal),
        sig,
        reinterpret_cast<std::uint64_t>(handler)
    ));
}

inline std::uint64_t sigreturn() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SigReturn));
}

inline std::uint64_t login(const LoginInfo* info) noexcept {
    if (info == nullptr) {
        return static_cast<std::uint64_t>(-1);
    }

    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Login),
        reinterpret_cast<std::uint64_t>(info)
    );
}

inline std::uint64_t login(const LoginInfo& info) noexcept {
    return login(&info);
}

inline std::uint64_t login(const char* username, const char* password) noexcept {
    if (username == nullptr || password == nullptr) {
        return static_cast<std::uint64_t>(-1);
    }

    LoginInfo info = {};

    size_t index = 0;
    while (username[index] != '\0' && index + 1 < sizeof(info.username)) {
        info.username[index] = username[index];
        ++index;
    }
    info.username[index] = '\0';

    index = 0;
    while (password[index] != '\0' && index + 1 < sizeof(info.password)) {
        info.password[index] = password[index];
        ++index;
    }
    info.password[index] = '\0';

    return login(info);
}

inline std::uint64_t logout(std::uint64_t session_id) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Logout), session_id);
}

inline std::uint64_t getuid() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::GetUID));
}

inline std::uint64_t getgid() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::GetGID));
}

inline std::uint64_t setuid(std::uint64_t uid) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SetUID), uid);
}

inline std::uint64_t setgid(std::uint64_t gid) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SetGID), gid);
}

inline std::uint64_t getsessionid() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::GetSessionID));
}

inline std::uint64_t getsessioninfo(std::uint64_t session_id, SessionInfo* info) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::GetSessionInfo),
        session_id,
        reinterpret_cast<std::uint64_t>(info)
    );
}

inline std::uint64_t chdir(const char* path) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Chdir),
        reinterpret_cast<std::uint64_t>(path)
    );
}

inline char* getcwd(char* buffer, std::size_t size) noexcept {
    return reinterpret_cast<char*>(_syscall_impl(
        static_cast<std::uint64_t>(Syscall::Getcwd),
        reinterpret_cast<std::uint64_t>(buffer),
        size
    ));
}

inline std::uint64_t mkdir(const char* path, std::uint64_t mode = 0755) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Mkdir),
        reinterpret_cast<std::uint64_t>(path),
        mode
    );
}

inline std::uint64_t rmdir(const char* path) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Rmdir),
        reinterpret_cast<std::uint64_t>(path)
    );
}

inline std::uint64_t unlink(const char* path) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Unlink),
        reinterpret_cast<std::uint64_t>(path)
    );
}

inline std::uint64_t stat(const char* path, Stat* statbuf) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Stat),
        reinterpret_cast<std::uint64_t>(path),
        reinterpret_cast<std::uint64_t>(statbuf)
    );
}

inline Handle duplicate_handle(Handle handle) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Dup), handle);
}

inline Handle duplicate_handle_to(Handle oldHandle, Handle newHandle) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Dup2), oldHandle, newHandle);
}

inline std::uint64_t dup(Handle handle) noexcept {
    return duplicate_handle(handle);
}

inline std::uint64_t dup2(Handle oldHandle, Handle newHandle) noexcept {
    return duplicate_handle_to(oldHandle, newHandle);
}

inline std::uint64_t pipe(Handle* pipeHandles) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Pipe),
        reinterpret_cast<std::uint64_t>(pipeHandles)
    );
}

inline std::uint64_t getppid() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::Getppid));
}

inline std::uint64_t spawn(
    const char* path,
    const char* const* argv = nullptr,
    const char* const* envp = nullptr
) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Spawn),
        reinterpret_cast<std::uint64_t>(path),
        reinterpret_cast<std::uint64_t>(argv),
        reinterpret_cast<std::uint64_t>(envp)
    );
}

inline std::uint64_t getuserinfo(std::uint64_t uid, UserInfo* info) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::GetUserInfo),
        uid,
        reinterpret_cast<std::uint64_t>(info)
    );
}

inline std::uint64_t readdir(
    const char* path,
    DirEntry* entries,
    std::uint64_t count
) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::Readdir),
        reinterpret_cast<std::uint64_t>(path),
        reinterpret_cast<std::uint64_t>(entries),
        count
    );
}

inline Handle shared_alloc(std::uint64_t size) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SharedAlloc), size);
}

inline void* shared_map(Handle handle) noexcept {
    return reinterpret_cast<void*>(_syscall_impl(static_cast<std::uint64_t>(Syscall::SharedMap), handle));
}

inline std::uint64_t shared_free(Handle handle) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SharedFree), handle);
}

inline Handle surface_create(std::uint32_t width, std::uint32_t height, std::uint32_t format = 0) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SurfaceCreate), width, height, format);
}

inline void* surface_map(Handle handle) noexcept {
    return reinterpret_cast<void*>(_syscall_impl(static_cast<std::uint64_t>(Syscall::SurfaceMap), handle));
}

inline std::uint64_t surface_commit(
    Handle handle,
    std::uint32_t dirtyX,
    std::uint32_t dirtyY,
    std::uint32_t dirtyWidth,
    std::uint32_t dirtyHeight
) noexcept {
    const std::uint64_t packed = (static_cast<std::uint64_t>(dirtyWidth) << 32) | dirtyHeight;
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SurfaceCommit), handle, dirtyX, dirtyY, packed);
}

inline std::uint64_t surface_poll(SurfaceInfo* info) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::SurfacePoll), reinterpret_cast<std::uint64_t>(info));
}

inline Handle compositor_create_window(
    Handle compositor,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t flags = 0
) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::CompositorCreateWindow), compositor, width, height, flags);
}

inline Handle window_event_queue(Handle window) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::WindowEventQueue), window);
}

inline std::uint64_t window_set_title(Handle window, const char* title) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::WindowSetTitle),
        window,
        reinterpret_cast<std::uint64_t>(title)
    );
}

inline std::uint64_t window_attach_surface(Handle window, Handle surface) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::WindowAttachSurface), window, surface);
}

inline std::uint64_t compositor_list_windows(WindowInfo* windows, std::uint64_t capacity) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::WindowList),
        reinterpret_cast<std::uint64_t>(windows),
        capacity
    );
}

inline std::uint64_t compositor_focus_window(std::uint64_t windowId) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::WindowFocus), windowId);
}

inline std::uint64_t compositor_move_window(std::uint64_t windowId, std::int32_t x, std::int32_t y) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::WindowMove), windowId, static_cast<std::uint64_t>(x), static_cast<std::uint64_t>(y));
}

inline std::uint64_t compositor_resize_window(std::uint64_t windowId, std::int32_t width, std::int32_t height) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::WindowResize), windowId, static_cast<std::uint64_t>(width), static_cast<std::uint64_t>(height));
}

inline std::uint64_t compositor_control_window(std::uint64_t windowId, WindowControlAction action) noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::WindowControl), windowId, static_cast<std::uint64_t>(action));
}

inline Handle queue_create() noexcept {
    return _syscall_impl(static_cast<std::uint64_t>(Syscall::QueueCreate));
}

inline std::uint64_t queue_send(Handle handle, const IPCMessage* message, bool wait = true) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::QueueSend),
        handle,
        reinterpret_cast<std::uint64_t>(message),
        wait ? 1 : 0
    );
}

inline std::uint64_t queue_receive(Handle handle, IPCMessage* message, bool wait = true) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::QueueReceive),
        handle,
        reinterpret_cast<std::uint64_t>(message),
        wait ? 1 : 0
    );
}

inline std::uint64_t queue_reply(
    Handle handle,
    std::uint64_t requestId,
    const void* data,
    std::uint64_t size
) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::QueueReply),
        handle,
        requestId,
        reinterpret_cast<std::uint64_t>(data),
        size
    );
}

inline std::uint64_t queue_request(
    Handle handle,
    const IPCMessage* request,
    void* response,
    std::uint64_t responseCapacity,
    std::uint64_t* responseSize = nullptr
) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::QueueRequest),
        handle,
        reinterpret_cast<std::uint64_t>(request),
        reinterpret_cast<std::uint64_t>(response),
        responseCapacity,
        reinterpret_cast<std::uint64_t>(responseSize)
    );
}

inline std::uint64_t service_register(const char* name, Handle queueHandle) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::ServiceRegister),
        reinterpret_cast<std::uint64_t>(name),
        queueHandle
    );
}

inline Handle service_connect(const char* name) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::ServiceConnect),
        reinterpret_cast<std::uint64_t>(name)
    );
}

inline Handle event_queue_create() noexcept {
    return queue_create();
}

inline std::uint64_t event_push(Handle handle, const IPCMessage* message, bool wait = true) noexcept {
    return queue_send(handle, message, wait);
}

inline std::uint64_t event_wait(Handle handle, IPCMessage* message) noexcept {
    return queue_receive(handle, message, true);
}

inline std::uint64_t event_poll(Handle handle, IPCMessage* message) noexcept {
    return queue_receive(handle, message, false);
}

inline IPCMessage make_event_message(const Event& event) noexcept {
    IPCMessage message = {};
    message.flags = IPC_MESSAGE_EVENT;
    message.size = sizeof(Event);

    const auto* source = reinterpret_cast<const uint8_t*>(&event);
    for (size_t i = 0; i < sizeof(Event); ++i) {
        message.data[i] = source[i];
    }
    return message;
}

inline bool event_from_message(const IPCMessage& message, Event* event) noexcept {
    if (event == nullptr || (message.flags & IPC_MESSAGE_EVENT) == 0 || message.size < sizeof(Event)) {
        return false;
    }

    auto* destination = reinterpret_cast<uint8_t*>(event);
    for (size_t i = 0; i < sizeof(Event); ++i) {
        destination[i] = message.data[i];
    }
    return true;
}

inline std::uint64_t event_push(Handle handle, const Event* event, bool wait = true) noexcept {
    if (event == nullptr) {
        return static_cast<std::uint64_t>(-1);
    }

    const IPCMessage message = make_event_message(*event);
    return queue_send(handle, &message, wait);
}

inline std::uint64_t event_wait(Handle handle, Event* event) noexcept {
    if (event == nullptr) {
        return static_cast<std::uint64_t>(-1);
    }

    IPCMessage message = {};
    if (queue_receive(handle, &message, true) == static_cast<std::uint64_t>(-1)) {
        return static_cast<std::uint64_t>(-1);
    }
    return event_from_message(message, event) ? 0 : static_cast<std::uint64_t>(-1);
}

inline std::uint64_t event_poll(Handle handle, Event* event) noexcept {
    if (event == nullptr) {
        return static_cast<std::uint64_t>(-1);
    }

    IPCMessage message = {};
    if (queue_receive(handle, &message, false) == static_cast<std::uint64_t>(-1)) {
        return static_cast<std::uint64_t>(-1);
    }
    return event_from_message(message, event) ? 0 : static_cast<std::uint64_t>(-1);
}
}
