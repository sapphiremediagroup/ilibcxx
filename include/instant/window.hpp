#pragma once

#include <cstdint.hpp>
#include <instant/result.hpp>
#include <service_protocol.hpp>
#include <string.hpp>
#include <syscall.hpp>

namespace instant {

using WindowResult = Result<bool, std::string>;

struct WindowConfig {
    int width = 640;
    int height = 480;
    const char* title = "Instant Window";
    std::uint32_t flags = 0;
    std::uint32_t format = std::services::surfaces::FORMAT_BGRA8;
    std::uint64_t frameIntervalMs = 16;
    bool autoCommit = true;
};

struct WindowBuffer {
    std::uint32_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    int pitch = 0;
};

class Window {
public:
    Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    virtual ~Window();

    int run(int argc, char** argv);

    [[nodiscard]] WindowBuffer& buffer() noexcept { return buffer_; }
    [[nodiscard]] const WindowBuffer& buffer() const noexcept { return buffer_; }
    [[nodiscard]] std::uint32_t* pixels() noexcept { return buffer_.pixels; }
    [[nodiscard]] int width() const noexcept { return buffer_.width; }
    [[nodiscard]] int height() const noexcept { return buffer_.height; }
    [[nodiscard]] int pitch() const noexcept { return buffer_.pitch; }
    [[nodiscard]] std::Handle compositor_handle() const noexcept { return compositor_; }
    [[nodiscard]] std::Handle surface_handle() const noexcept { return surface_; }
    [[nodiscard]] std::Handle window_handle() const noexcept { return window_; }
    [[nodiscard]] std::Handle event_queue_handle() const noexcept { return events_; }
    [[nodiscard]] int argc() const noexcept { return argc_; }
    [[nodiscard]] char** argv() const noexcept { return argv_; }
    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] bool focused() const noexcept { return focused_; }
    [[nodiscard]] const std::Event& current_event() const noexcept { return currentEvent_; }

    bool commit() noexcept;
    bool commit(int dirtyX, int dirtyY, int dirtyWidth, int dirtyHeight) noexcept;
    bool set_title(const char* title) noexcept;
    void close() noexcept;

protected:
    virtual WindowConfig configure();
    virtual WindowResult init();
    virtual WindowResult update();
    virtual WindowResult event();
    virtual WindowResult event(const std::Event& event);
    virtual void cleanup();

private:
    bool setup(const WindowConfig& config, std::string* error);
    void teardown() noexcept;
    bool apply_result(const WindowResult& result, const char* phase, int* exitCode);
    void report_error(const char* phase, const std::string& message) noexcept;

    int argc_ = 0;
    char** argv_ = nullptr;
    WindowConfig config_ = {};
    WindowBuffer buffer_ = {};
    std::Handle compositor_ = static_cast<std::Handle>(-1);
    std::Handle surface_ = static_cast<std::Handle>(-1);
    std::Handle window_ = static_cast<std::Handle>(-1);
    std::Handle events_ = static_cast<std::Handle>(-1);
    std::Event currentEvent_ = {};
    bool running_ = false;
    bool focused_ = false;
};

int run_window_app(int argc, char** argv);

}

extern "C" instant::Window* instant_create_window(int argc, char** argv);

#define INSTANT_WINDOW_APP(WindowType) \
    extern "C" ::instant::Window* instant_create_window(int argc, char** argv) { \
        (void)argc; \
        (void)argv; \
        alignas(WindowType) static unsigned char storage[sizeof(WindowType)]; \
        static bool constructed = false; \
        if (!constructed) { \
            new (storage) WindowType(); \
            constructed = true; \
        } \
        return reinterpret_cast<WindowType*>(storage); \
    }
