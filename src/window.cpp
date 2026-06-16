#include <instant/window.hpp>

#include <cstring.hpp>

namespace {

constexpr std::uint64_t fail = static_cast<std::uint64_t>(-1);

bool failed(std::uint64_t value) noexcept {
    return std::syscall_failed(value);
}

std::Handle connect_service_with_retry(const char* name) noexcept {
    for (int attempt = 0; attempt < 500; ++attempt) {
        const std::Handle handle = std::service_connect(name);
        if (!failed(handle)) {
            return handle;
        }
        std::yield();
    }
    return fail;
}

void write_str(std::FileHandle handle, const char* text) noexcept {
    (void)handle;
    if (text) {
        std::serial_write(text, std::strlen(text));
    }
}

}

namespace instant {

Window::~Window() {
    teardown();
}

WindowConfig Window::configure() {
    return {};
}

WindowResult Window::init() {
    return true;
}

WindowResult Window::update() {
    return true;
}

WindowResult Window::event() {
    return true;
}

WindowResult Window::event(const std::Event&) {
    return event();
}

void Window::cleanup() {
}

bool Window::commit() noexcept {
    return commit(0, 0, buffer_.width, buffer_.height);
}

bool Window::commit(int dirtyX, int dirtyY, int dirtyWidth, int dirtyHeight) noexcept {
    if (failed(surface_) || dirtyWidth <= 0 || dirtyHeight <= 0) {
        return false;
    }
    return !failed(std::surface_commit(
        surface_,
        static_cast<std::uint32_t>(dirtyX < 0 ? 0 : dirtyX),
        static_cast<std::uint32_t>(dirtyY < 0 ? 0 : dirtyY),
        static_cast<std::uint32_t>(dirtyWidth),
        static_cast<std::uint32_t>(dirtyHeight)
    ));
}

bool Window::set_title(const char* title) noexcept {
    if (failed(window_)) {
        return false;
    }
    return !failed(std::window_set_title(window_, title ? title : ""));
}

void Window::close() noexcept {
    running_ = false;
}

bool Window::setup(const WindowConfig& config, std::string* error) {
    if (config.width <= 0 || config.height <= 0) {
        if (error) {
            *error = "invalid window size";
        }
        return false;
    }

    config_ = config;
    compositor_ = connect_service_with_retry(std::services::graphics_compositor::NAME);
    if (failed(compositor_)) {
        if (error) {
            *error = "graphics.compositor service unavailable";
        }
        return false;
    }

    surface_ = std::surface_create(
        static_cast<std::uint32_t>(config.width),
        static_cast<std::uint32_t>(config.height),
        config.format
    );
    if (failed(surface_)) {
        if (error) {
            *error = "surface_create failed";
        }
        return false;
    }

    auto* pixels = static_cast<std::uint32_t*>(std::shared_map(surface_));
    if (pixels == nullptr || failed(reinterpret_cast<std::uint64_t>(pixels))) {
        if (error) {
            *error = "shared_map(surface) failed";
        }
        return false;
    }

    window_ = std::compositor_create_window(
        compositor_,
        static_cast<std::uint32_t>(config.width),
        static_cast<std::uint32_t>(config.height),
        config.flags
    );
    if (failed(window_)) {
        if (error) {
            *error = "compositor_create_window failed";
        }
        return false;
    }

    if (!set_title(config.title)) {
        if (error) {
            *error = "window_set_title failed";
        }
        return false;
    }

    if (failed(std::window_attach_surface(window_, surface_))) {
        if (error) {
            *error = "window_attach_surface failed";
        }
        return false;
    }

    events_ = std::window_event_queue(window_);
    if (failed(events_)) {
        if (error) {
            *error = "window_event_queue failed";
        }
        return false;
    }

    buffer_.pixels = pixels;
    buffer_.width = config.width;
    buffer_.height = config.height;
    buffer_.pitch = config.width;
    return true;
}

void Window::teardown() noexcept {
    if (!failed(events_)) {
        std::close(events_);
        events_ = fail;
    }
    if (!failed(window_)) {
        std::close(window_);
        window_ = fail;
    }
    if (!failed(surface_)) {
        std::close(surface_);
        surface_ = fail;
    }
    if (!failed(compositor_)) {
        std::close(compositor_);
        compositor_ = fail;
    }
    buffer_ = {};
}

void Window::report_error(const char* phase, const std::string& message) noexcept {
    write_str(std::STDERR_HANDLE, "[instant.window] ");
    write_str(std::STDERR_HANDLE, phase ? phase : "window");
    write_str(std::STDERR_HANDLE, ": ");
    std::serial_write(message.data(), message.size());
    write_str(std::STDERR_HANDLE, "\n");
}

bool Window::apply_result(const WindowResult& result, const char* phase, int* exitCode) {
    if (result.is_error()) {
        report_error(phase, result.error());
        if (exitCode) {
            *exitCode = 1;
        }
        running_ = false;
        return false;
    }

    if (!result.value()) {
        running_ = false;
        return false;
    }
    return true;
}

int Window::run(int argc, char** argv) {
    argc_ = argc;
    argv_ = argv;
    running_ = true;
    focused_ = false;

    std::string setupError;
    if (!setup(configure(), &setupError)) {
        report_error("setup", setupError);
        teardown();
        return 1;
    }

    int exitCode = 0;
    if (!apply_result(init(), "init", &exitCode)) {
        cleanup();
        teardown();
        return exitCode;
    }
    if (config_.autoCommit && !commit()) {
        report_error("commit", std::string("initial surface_commit failed"));
        cleanup();
        teardown();
        return 1;
    }

    while (running_) {
        for (;;) {
            std::Event nextEvent = {};
            if (std::event_poll(events_, &nextEvent) == fail) {
                break;
            }

            currentEvent_ = nextEvent;
            if (nextEvent.type == std::EventType::Window) {
                if (nextEvent.window.action == std::WindowEventAction::FocusGained) {
                    focused_ = true;
                } else if (nextEvent.window.action == std::WindowEventAction::FocusLost) {
                    focused_ = false;
                } else if (nextEvent.window.action == std::WindowEventAction::CloseRequested) {
                    running_ = false;
                }
            }

            if (!apply_result(event(nextEvent), "event", &exitCode)) {
                break;
            }
        }

        if (!running_) {
            break;
        }

        if (!apply_result(update(), "update", &exitCode)) {
            break;
        }
        if (config_.autoCommit && !commit()) {
            report_error("commit", std::string("surface_commit failed"));
            exitCode = 1;
            break;
        }

        if (config_.frameIntervalMs != 0) {
            std::sleep(config_.frameIntervalMs);
        } else {
            std::yield();
        }
    }

    cleanup();
    teardown();
    return exitCode;
}

}
