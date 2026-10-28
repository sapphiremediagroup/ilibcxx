#include <ui/window.hpp>

#include <ui/controls.hpp>

#include <cstring.hpp>
#include <poll.h>
#include <service_protocol.hpp>

namespace ui {

namespace {

constexpr std::Handle kNone = static_cast<std::Handle>(-1);
constexpr std::uint64_t kDoubleClickMs = 400;
constexpr int kDoubleClickSlop = 4;

bool failed(std::uint64_t value) {
    return std::syscall_failed(value);
}

void log(const char* what) {
    std::serial_write("[ui.window] ", 12);
    std::serial_write(what, std::strlen(what));
    std::serial_write("\n", 1);
}

std::Handle connect_compositor() {
    // The compositor may not have registered its service yet.
    for (int attempt = 0; attempt < 500; ++attempt) {
        const std::Handle handle = std::service_connect(std::services::graphics_compositor::NAME);
        if (!failed(handle)) {
            return handle;
        }
        std::yield();
    }
    return kNone;
}

}  // namespace

Window::Window() : compositor_(kNone), window_(kNone), surface_(kNone), shownSurface_(kNone), events_(kNone) {
    // The first control made for a window is the root of its tree.
    root_ = new (std::nothrow) Panel(this);
}

Window::~Window() {
    teardown();
    // The app's controls (members of the derived class) are gone by now.
    delete root_;
}

void Window::on_paint(Canvas& canvas, const Rect& dirty) {
    canvas.fill(dirty, theme().window);
}

// ---- requests ----------------------------------------------------------------

void Window::invalidate() {
    invalidate({ 0, 0, width_, height_ });
}

void Window::invalidate(const Rect& rect) {
    dirty_ = dirty_.unite(rect.intersect({ 0, 0, width_, height_ }));
}

void Window::update() {
    paint();
}

int Window::set_timer(int ms, bool repeat) {
    for (Timer& timer : timers_) {
        if (timer.id == 0) {
            timer.id = nextTimerId_++;
            timer.intervalMs = ms > 0 ? static_cast<std::uint64_t>(ms) : 1;
            timer.dueMs = std::gettime() + timer.intervalMs;
            timer.repeat = repeat;
            return timer.id;
        }
    }
    return 0;
}

void Window::kill_timer(int id) {
    for (Timer& timer : timers_) {
        if (timer.id == id) {
            timer.id = 0;
        }
    }
}

void Window::set_title(const char* title) {
    if (!failed(window_)) {
        std::window_set_title(window_, title ? title : "");
    }
}

void Window::close() {
    running_ = false;
}

bool Window::key_held(std::uint16_t key) const {
    return key < 256 && (keysHeld_[key >> 3] & (1u << (key & 7))) != 0;
}

// ---- the window and its buffer ---------------------------------------------

bool Window::open(const WindowOptions& options) {
    if (options.width <= 0 || options.height <= 0) {
        log("invalid window size");
        return false;
    }
    compositor_ = connect_compositor();
    if (failed(compositor_)) {
        log("graphics.compositor service unavailable");
        return false;
    }
    window_ = std::compositor_create_window(compositor_, static_cast<std::uint32_t>(options.width),
                                            static_cast<std::uint32_t>(options.height), options.flags);
    if (failed(window_)) {
        log("compositor_create_window failed");
        return false;
    }
    set_title(options.title);
    events_ = std::window_event_queue(window_);
    if (failed(events_)) {
        log("window_event_queue failed");
        return false;
    }
    return make_buffer(options.width, options.height);
}

// A surface of width x height, mapped here, and the private buffer painting
// goes to. The surface is shown in the window (and the old one dropped; the
// compositor unmaps its copy itself) only once it has been painted whole,
// so the window never shows a blank or half-drawn new buffer.
bool Window::make_buffer(int width, int height) {
    std::uint32_t* back = new (std::nothrow) std::uint32_t[static_cast<std::size_t>(width) * static_cast<std::size_t>(height)];
    if (!back) {
        log("out of memory for the window buffer");
        return false;
    }
    const std::Handle surface = std::surface_create(static_cast<std::uint32_t>(width),
                                                    static_cast<std::uint32_t>(height),
                                                    std::services::surfaces::FORMAT_BGRA8);
    if (failed(surface)) {
        log("surface_create failed");
        return false;
    }
    auto* pixels = static_cast<std::uint32_t*>(std::shared_map(surface));
    if (!pixels || failed(reinterpret_cast<std::uint64_t>(pixels))) {
        std::shared_free(surface);
        delete[] back;
        log("shared_map(surface) failed");
        return false;
    }
    if (!failed(surface_)) {
        // Still waiting to be shown: replaced before it ever was.
        if (!failed(shownSurface_) && shownSurface_ != surface_) {
            std::shared_free(surface_);
        } else {
            shownSurface_ = surface_;
        }
    }
    surface_ = surface;
    pixels_ = pixels;
    delete[] back_;
    back_ = back;
    width_ = width;
    height_ = height;
    pendingWidth_ = width;
    pendingHeight_ = height;
    dirty_ = { 0, 0, width, height };
    if (root_) {
        root_->bounds_ = { 0, 0, width, height };
        root_->layout();
    }
    return true;
}

void Window::teardown() {
    if (!failed(events_)) {
        std::close(events_);
        events_ = kNone;
    }
    if (!failed(window_)) {
        std::close(window_);
        window_ = kNone;
    }
    if (!failed(surface_)) {
        std::shared_free(surface_);
    }
    if (!failed(shownSurface_) && shownSurface_ != surface_) {
        std::shared_free(shownSurface_);
    }
    surface_ = kNone;
    shownSurface_ = kNone;
    delete[] back_;
    back_ = nullptr;
    if (!failed(compositor_)) {
        std::close(compositor_);
        compositor_ = kNone;
    }
    pixels_ = nullptr;
}

// ---- the loop ----------------------------------------------------------------

int Window::run(const WindowOptions& options) {
    if (!open(options)) {
        teardown();
        return 1;
    }
    running_ = true;
    on_create();
    if (root_) {
        root_->layout();
    }

    while (running_) {
        paint();

        std::PollFD wait = { static_cast<std::int64_t>(events_), POLLIN, 0 };
        std::poll(&wait, 1, next_wait_ms());

        // Everything queued, then a resize to the latest size only.
        for (;;) {
            std::Event event = {};
            if (failed(std::event_poll(events_, &event))) {
                break;
            }
            dispatch(event);
            if (!running_) {
                break;
            }
        }
        if (running_ && (pendingWidth_ != width_ || pendingHeight_ != height_) &&
            pendingWidth_ > 0 && pendingHeight_ > 0) {
            if (make_buffer(pendingWidth_, pendingHeight_)) {
                on_size({ width_, height_ });
            } else {
                pendingWidth_ = width_;
                pendingHeight_ = height_;
            }
        }
        if (running_) {
            run_due_timers();
        }
    }

    on_destroy();
    teardown();
    return 0;
}

void Window::paint() {
    const Rect dirty = dirty_.intersect({ 0, 0, width_, height_ });
    dirty_ = {};
    if (dirty.empty() || !pixels_ || !back_) {
        return;
    }
    // Painted off screen, then copied into the surface in one go: the
    // compositor, which reads the surface whenever it redraws, never sees a
    // paint half done.
    Canvas canvas = Canvas(back_, width_, height_, width_).clipped(dirty);
    on_paint(canvas, dirty);
    if (root_) {
        root_->paint_tree(canvas);
    }
    Canvas(pixels_, width_, height_, width_).copy(back_, width_, dirty, { dirty.x, dirty.y });
    if (shownSurface_ != surface_) {
        if (failed(std::window_attach_surface(window_, surface_))) {
            log("window_attach_surface failed");
        } else {
            if (!failed(shownSurface_)) {
                std::shared_free(shownSurface_);
            }
            shownSurface_ = surface_;
        }
    }
    std::surface_commit(surface_, static_cast<std::uint32_t>(dirty.x), static_cast<std::uint32_t>(dirty.y),
                        static_cast<std::uint32_t>(dirty.width), static_cast<std::uint32_t>(dirty.height));
}

std::uint64_t Window::next_wait_ms() const {
    if (!dirty_.empty()) {
        return 0;
    }
    std::uint64_t soonest = std::kPollForever;
    const std::uint64_t now = std::gettime();
    for (const Timer& timer : timers_) {
        if (timer.id == 0) {
            continue;
        }
        const std::uint64_t wait = timer.dueMs > now ? timer.dueMs - now : 0;
        if (wait < soonest) {
            soonest = wait;
        }
    }
    return soonest;
}

void Window::run_due_timers() {
    const std::uint64_t now = std::gettime();
    for (Timer& timer : timers_) {
        if (timer.id == 0 || timer.dueMs > now) {
            continue;
        }
        const int id = timer.id;
        if (timer.repeat) {
            // Late (the app was busy): carry on from now rather than burst.
            timer.dueMs += timer.intervalMs;
            if (timer.dueMs <= now) {
                timer.dueMs = now + timer.intervalMs;
            }
        } else {
            timer.id = 0;
        }
        on_timer(id);
        if (!running_) {
            return;
        }
    }
}

// ---- events ----------------------------------------------------------------

void Window::dispatch(const std::Event& event) {
    switch (event.type) {
    case std::EventType::Key:
        dispatch_key(event.key);
        break;
    case std::EventType::Pointer:
        dispatch_pointer(event.pointer);
        break;
    case std::EventType::Window:
        switch (event.window.action) {
        case std::WindowEventAction::FocusGained:
        case std::WindowEventAction::FocusLost:
            focused_ = event.window.action == std::WindowEventAction::FocusGained;
            if (!focused_) {
                // Releases now go to another window.
                std::memset(keysHeld_, 0, sizeof(keysHeld_));
                capture_ = nullptr;
                set_hover(nullptr);
            }
            if (focus_) {
                focus_->invalidate();   // its focus ring follows the window's
            }
            on_focus(focused_);
            break;
        case std::WindowEventAction::CloseRequested:
            if (on_close()) {
                running_ = false;
            }
            break;
        case std::WindowEventAction::Resized:
            pendingWidth_ = event.window.width;
            pendingHeight_ = event.window.height;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

void Window::dispatch_key(const std::KeyEvent& event) {
    const std::uint16_t key = event.keycode;
    if (key < 256) {
        if (event.action == std::KeyEventAction::Release) {
            keysHeld_[key >> 3] &= static_cast<std::uint8_t>(~(1u << (key & 7)));
        } else {
            keysHeld_[key >> 3] |= static_cast<std::uint8_t>(1u << (key & 7));
        }
    }
    if ((event.modifiers & std::KeyModifierSuper) != 0) {
        return;
    }

    KeyEvent key_event = {};
    key_event.key = key;
    key_event.modifiers = event.modifiers;
    key_event.repeat = event.action == std::KeyEventAction::Repeat;
    std::memcpy(key_event.text, event.text, sizeof(key_event.text));
    key_event.text[sizeof(key_event.text) - 1] = '\0';

    if (event.action == std::KeyEventAction::Release) {
        on_key_up(key_event);
        return;
    }
    if (focus_ && focus_->enabled() && focus_->visible() && focus_->key_down(key_event)) {
        return;
    }
    if (on_key_down(key_event)) {
        return;
    }
    const bool plain = (event.modifiers & (std::KeyModifierControl | std::KeyModifierAlt)) == 0;
    if (plain && key == std::KeyTab) {
        move_focus(key_event.shift());
        return;
    }
    if (plain && (key == std::KeyEnter || key == std::KeypadEnter) && defaultButton_ && defaultButton_->enabled()) {
        defaultButton_->click();
        return;
    }
    if (plain && key == std::KeyEscape && cancelButton_ && cancelButton_->enabled()) {
        cancelButton_->click();
        return;
    }
    if (key_event.text[0] != '\0' && plain) {
        if (focus_ && focus_->enabled() && focus_->visible() && focus_->char_typed(key_event.text)) {
            return;
        }
        on_char(key_event.text, key_event);
    }
}

// The control a mouse event goes to: the one holding the mouse, else the one
// under the pointer (null: the window's own background).
static MouseEvent local_to(Control* control, MouseEvent mouse) {
    const Rect at = control->window_rect();
    mouse.x -= at.x;
    mouse.y -= at.y;
    return mouse;
}

void Window::dispatch_pointer(const std::PointerEvent& event) {
    MouseEvent mouse = {};
    mouse.x = event.x;
    mouse.y = event.y;
    mouse.buttons = event.buttons;
    Control* under = root_ ? root_->hit(event.x, event.y) : nullptr;
    if (under == root_) {
        under = nullptr;
    }

    if (event.action == std::PointerEventAction::Scroll) {
        for (Control* control = under; control && control != root_; control = control->parent_) {
            if (control->enabled() && control->wheel(event.deltaY)) {
                return;
            }
        }
        on_wheel(mouse, event.deltaY);
        return;
    }
    if (event.action != std::PointerEventAction::Button) {
        if (capture_) {
            capture_->mouse_move(local_to(capture_, mouse));
            return;
        }
        set_hover(under);
        if (under) {
            if (under->enabled()) {
                under->mouse_move(local_to(under, mouse));
            }
        } else {
            on_mouse_move(mouse);
        }
        return;
    }

    const std::uint16_t pressed = static_cast<std::uint16_t>(event.buttons & ~buttons_);
    const std::uint16_t released = static_cast<std::uint16_t>(buttons_ & ~event.buttons);
    buttons_ = event.buttons;
    for (std::uint16_t bit = 1; bit <= MouseMiddle; bit = static_cast<std::uint16_t>(bit << 1)) {
        if ((released & bit) != 0) {
            mouse.button = bit;
            mouse.clicks = clicks_;
            Control* target = capture_ ? capture_ : under;
            if (target) {
                if (target->enabled()) {
                    target->mouse_up(local_to(target, mouse));
                }
            } else {
                on_mouse_up(mouse);
            }
        }
        if ((pressed & bit) != 0) {
            const std::uint64_t now = std::gettime();
            const int dx = event.x - lastPressX_;
            const int dy = event.y - lastPressY_;
            const bool again = bit == lastPressButton_ && now - lastPressMs_ <= kDoubleClickMs &&
                               dx <= kDoubleClickSlop && dx >= -kDoubleClickSlop &&
                               dy <= kDoubleClickSlop && dy >= -kDoubleClickSlop;
            clicks_ = again ? clicks_ + 1 : 1;
            lastPressMs_ = now;
            lastPressX_ = event.x;
            lastPressY_ = event.y;
            lastPressButton_ = bit;
            mouse.button = bit;
            mouse.clicks = clicks_;
            Control* target = capture_ ? capture_ : under;
            if (target) {
                if (target->enabled()) {
                    if (target->focusable()) {
                        focus(target);
                    }
                    capture_ = target;
                    target->mouse_down(local_to(target, mouse));
                }
            } else {
                on_mouse_down(mouse);
            }
        }
    }
    if (buttons_ == 0) {
        capture_ = nullptr;
        set_hover(under);
    }
}

// ---- focus and the control tree ----------------------------------------------

void Window::focus(Control* control) {
    if (control == focus_) {
        return;
    }
    Control* old = focus_;
    focus_ = control;
    if (old) {
        old->focus_changed(false);
    }
    if (control) {
        control->focus_changed(true);
    }
}

void Window::set_hover(Control* control) {
    if (control == hover_) {
        return;
    }
    Control* old = hover_;
    hover_ = control;
    if (old) {
        old->mouse_leave();
    }
    if (control) {
        control->mouse_enter();
    }
}

// The next (or previous) control that takes the focus, in tree order,
// wrapping around.
void Window::move_focus(bool backwards) {
    Control* order[64];
    int count = 0;
    // Depth first over the visible, enabled part of the tree.
    Control* stack[32];
    int depth = 0;
    if (root_) {
        stack[depth++] = root_->firstChild_;
    }
    while (depth > 0 && count < 64) {
        Control* control = stack[depth - 1];
        if (!control) {
            --depth;
            continue;
        }
        stack[depth - 1] = control->next_;
        if (!control->visible_ || !control->enabled_) {
            continue;
        }
        if (control->focusable()) {
            order[count++] = control;
        }
        if (control->firstChild_ && depth < 32) {
            stack[depth++] = control->firstChild_;
        }
    }
    if (count == 0) {
        return;
    }
    int at = -1;
    for (int i = 0; i < count; ++i) {
        if (order[i] == focus_) {
            at = i;
        }
    }
    if (at < 0) {
        focus(order[backwards ? count - 1 : 0]);
    } else {
        focus(order[(at + (backwards ? count - 1 : 1)) % count]);
    }
}

// A control going away: nothing may point at it any more.
void Window::forget(Control* control) {
    if (focus_ == control) {
        focus_ = nullptr;
    }
    if (capture_ == control) {
        capture_ = nullptr;
    }
    if (hover_ == control) {
        hover_ = nullptr;
    }
    if (static_cast<Control*>(defaultButton_) == control) {
        defaultButton_ = nullptr;
    }
    if (static_cast<Control*>(cancelButton_) == control) {
        cancelButton_ = nullptr;
    }
}

}  // namespace ui
