#include <ui/controls.hpp>

#include <cstring.hpp>

namespace ui {

namespace {

void copy_text(char* dst, std::size_t size, const char* src) {
    std::size_t i = 0;
    for (; src && src[i] != '\0' && i + 1 < size; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

}  // namespace

// ---- the tree ----------------------------------------------------------------

Control::Control(Window* window) : window_(window) {
    if (window->root_) {
        attach(window->root_);
    }
}

Control::Control(Control* parent) : window_(parent->window_) {
    attach(parent);
}

Control::~Control() {
    // Children still attached (declared after their parent) are let go.
    while (firstChild_) {
        Control* child = firstChild_;
        firstChild_ = child->next_;
        child->parent_ = nullptr;
        child->next_ = nullptr;
        child->previous_ = nullptr;
    }
    lastChild_ = nullptr;
    if (parent_) {
        invalidate();
        (previous_ ? previous_->next_ : parent_->firstChild_) = next_;
        (next_ ? next_->previous_ : parent_->lastChild_) = previous_;
    }
    if (window_) {
        window_->forget(this);
    }
}

void Control::attach(Control* parent) {
    parent_ = parent;
    previous_ = parent->lastChild_;
    (previous_ ? previous_->next_ : parent->firstChild_) = this;
    parent->lastChild_ = this;
}

Control& Control::set_bounds(const Rect& bounds) {
    invalidate();
    const bool resizedNow = bounds.width != bounds_.width || bounds.height != bounds_.height;
    bounds_ = bounds;
    record_anchor();
    if (resizedNow) {
        resized();
        layout();
    }
    invalidate();
    return *this;
}

Control& Control::set_anchor(std::uint32_t anchor) {
    anchor_ = anchor;
    record_anchor();
    return *this;
}

Control& Control::set_dock(Dock dock) {
    dock_ = dock;
    if (parent_) {
        parent_->layout();
    }
    return *this;
}

Control& Control::set_visible(bool visible) {
    if (visible != visible_) {
        visible_ = visible;
        if (!visible && window_ && window_->focus_ == this) {
            window_->focus(nullptr);
        }
        if (parent_) {
            window_->invalidate(parent_->window_rect());
            if (dock_ != Dock::None) {
                parent_->layout();
            }
        }
    }
    return *this;
}

Control& Control::set_enabled(bool enabled) {
    if (enabled != enabled_) {
        enabled_ = enabled;
        if (!enabled && window_ && window_->focus_ == this) {
            window_->focus(nullptr);
        }
        invalidate();
    }
    return *this;
}

bool Control::enabled() const {
    return enabled_ && (!parent_ || parent_->enabled());
}

bool Control::focused() const {
    return window_ && window_->focus_ == this && window_->focused();
}

void Control::focus() {
    if (window_ && focusable() && visible_ && enabled()) {
        window_->focus(this);
    }
}

bool Control::hovered() const {
    return window_ && window_->hover_ == this;
}

bool Control::pressed_capture() const {
    return window_ && window_->capture_ == this;
}

void Control::invalidate() {
    invalidate({ 0, 0, bounds_.width, bounds_.height });
}

void Control::invalidate(const Rect& rect) {
    if (window_ && (parent_ || this == window_->root_)) {
        const Rect at = window_rect();
        window_->invalidate(rect.offset(at.x, at.y));
    }
}

Rect Control::window_rect() const {
    Rect rect = bounds_;
    for (const Control* up = parent_; up; up = up->parent_) {
        rect = rect.offset(up->bounds_.x, up->bounds_.y);
    }
    return rect;
}

void Control::record_anchor() {
    if (parent_) {
        rightGap_ = parent_->bounds_.width - bounds_.right();
        bottomGap_ = parent_->bounds_.height - bounds_.bottom();
    }
}

void Control::apply_anchor(const Size& parent) {
    if ((anchor_ & AnchorLeft) && (anchor_ & AnchorRight)) {
        bounds_.width = parent.width - bounds_.x - rightGap_;
    } else if (anchor_ & AnchorRight) {
        bounds_.x = parent.width - rightGap_ - bounds_.width;
    }
    if ((anchor_ & AnchorTop) && (anchor_ & AnchorBottom)) {
        bounds_.height = parent.height - bounds_.y - bottomGap_;
    } else if (anchor_ & AnchorBottom) {
        bounds_.y = parent.height - bottomGap_ - bounds_.height;
    }
    if (bounds_.width < 0) {
        bounds_.width = 0;
    }
    if (bounds_.height < 0) {
        bounds_.height = 0;
    }
}

void Control::layout() {
    Rect free = { 0, 0, bounds_.width, bounds_.height };
    for (Control* child = firstChild_; child; child = child->next_) {
        if (!child->visible_) {
            continue;
        }
        const Rect before = child->bounds_;
        Rect& b = child->bounds_;
        switch (child->dock_) {
        case Dock::Top:
            b = { free.x, free.y, free.width, b.height };
            free.y += b.height;
            free.height -= b.height;
            break;
        case Dock::Bottom:
            b = { free.x, free.bottom() - b.height, free.width, b.height };
            free.height -= b.height;
            break;
        case Dock::Left:
            b = { free.x, free.y, b.width, free.height };
            free.x += b.width;
            free.width -= b.width;
            break;
        case Dock::Right:
            b = { free.right() - b.width, free.y, b.width, free.height };
            free.width -= b.width;
            break;
        case Dock::Fill:
            b = free;
            break;
        case Dock::None:
            child->apply_anchor({ bounds_.width, bounds_.height });
            break;
        }
        if (b.width != before.width || b.height != before.height) {
            child->resized();
        }
        child->layout();
    }
    invalidate();
}

Control* Control::hit(int x, int y) {
    if (!visible_ || !Rect { 0, 0, bounds_.width, bounds_.height }.contains(x, y)) {
        return nullptr;
    }
    for (Control* child = lastChild_; child; child = child->previous_) {
        if (Control* found = child->hit(x - child->bounds_.x, y - child->bounds_.y)) {
            return found;
        }
    }
    return this;
}

void Control::paint_tree(Canvas& canvas) {
    paint(canvas);
    for (Control* child = firstChild_; child; child = child->next_) {
        if (!child->visible_) {
            continue;
        }
        Canvas sub = canvas.sub(child->bounds_);
        if (!sub.clip().empty()) {
            child->paint_tree(sub);
        }
    }
}

void Control::paint(Canvas&) {
}

// ---- Panel, Label ------------------------------------------------------------

void Panel::paint(Canvas& canvas) {
    const Rect all = { 0, 0, bounds().width, bounds().height };
    if (hasBackground_) {
        canvas.fill(all, background_);
    }
    if (border_) {
        canvas.frame(all, theme().border);
    }
}

Label::Label(Window* window, const char* text) : Control(window) {
    copy_text(text_, sizeof(text_), text);
}

Label::Label(Control* parent, const char* text) : Control(parent) {
    copy_text(text_, sizeof(text_), text);
}

Label& Label::set_text(const char* text) {
    copy_text(text_, sizeof(text_), text);
    invalidate();
    return *this;
}

void Label::paint(Canvas& canvas) {
    const Color color = !enabled() ? theme().textDisabled : hasColor_ ? color_ : theme().text;
    canvas.text({ 0, 0, bounds().width, bounds().height }, text_, theme().font(), color, align_);
}

// ---- Button ------------------------------------------------------------------

Button::Button(Window* window, const char* text) : Control(window) {
    copy_text(text_, sizeof(text_), text);
}

Button::Button(Control* parent, const char* text) : Control(parent) {
    copy_text(text_, sizeof(text_), text);
}

Button& Button::set_text(const char* text) {
    copy_text(text_, sizeof(text_), text);
    invalidate();
    return *this;
}

void Button::click() {
    if (enabled() && on_click) {
        on_click();
    }
}

void Button::paint(Canvas& canvas) {
    const Theme& t = theme();
    const Rect all = { 0, 0, bounds().width, bounds().height };
    const bool primary = window() && window()->default_button() == this;
    const bool down = pressed_ && pressed_capture();
    Color face = primary ? (hovered() ? t.accentHover : t.accent) : (hovered() ? t.faceHover : t.face);
    if (down) {
        face = primary ? t.accent : t.facePressed;
    }
    if (!enabled()) {
        face = t.face;
    }
    canvas.fill_rounded(all, t.radius, face);
    if (!primary || !enabled()) {
        canvas.frame(all, t.border, 1, t.radius);
    }
    if (focused()) {
        canvas.frame(all, t.focus, 2, t.radius);
    }
    const Color text = !enabled() ? t.textDisabled : primary ? t.accentText : t.text;
    canvas.text(all.offset(0, down ? 1 : 0), text_, t.font(), text, AlignCenter | AlignMiddle);
}

bool Button::key_down(const KeyEvent& key) {
    if (!key.ctrl() && !key.alt() &&
        (key.key == std::KeySpace || key.key == std::KeyEnter || key.key == std::KeypadEnter)) {
        click();
        return true;
    }
    return false;
}

void Button::mouse_down(const MouseEvent& mouse) {
    if (mouse.button == MouseLeft) {
        pressed_ = true;
        invalidate();
    }
}

void Button::mouse_move(const MouseEvent& mouse) {
    if (!pressed_capture()) {
        return;
    }
    // Dragged off it, it pops up; back on, it is pressed again.
    const bool inside = Rect { 0, 0, bounds().width, bounds().height }.contains(mouse.x, mouse.y);
    if (inside != pressed_) {
        pressed_ = inside;
        invalidate();
    }
}

void Button::mouse_up(const MouseEvent& mouse) {
    if (mouse.button != MouseLeft) {
        return;
    }
    const bool clicked = pressed_ && Rect { 0, 0, bounds().width, bounds().height }.contains(mouse.x, mouse.y);
    pressed_ = false;
    invalidate();
    if (clicked) {
        click();
    }
}

// ---- CheckBox ----------------------------------------------------------------

CheckBox::CheckBox(Window* window, const char* text) : Control(window) {
    copy_text(text_, sizeof(text_), text);
}

CheckBox::CheckBox(Control* parent, const char* text) : Control(parent) {
    copy_text(text_, sizeof(text_), text);
}

CheckBox& CheckBox::set_text(const char* text) {
    copy_text(text_, sizeof(text_), text);
    invalidate();
    return *this;
}

CheckBox& CheckBox::set_checked(bool checked) {
    if (checked != checked_) {
        checked_ = checked;
        invalidate();
        if (on_change) {
            on_change(checked);
        }
    }
    return *this;
}

void CheckBox::paint(Canvas& canvas) {
    const Theme& t = theme();
    const int size = t.checkSize;
    const Rect box = { 0, (bounds().height - size) / 2, size, size };
    if (checked_) {
        canvas.fill_rounded(box, 3, enabled() ? (hovered() ? t.accentHover : t.accent) : t.face);
        const Color mark = enabled() ? t.accentText : t.textDisabled;
        canvas.line({ box.x + 4, box.y + 8 }, { box.x + 7, box.y + 11 }, mark, 2);
        canvas.line({ box.x + 7, box.y + 11 }, { box.x + 12, box.y + 5 }, mark, 2);
    } else {
        canvas.fill_rounded(box, 3, hovered() && enabled() ? t.faceHover : t.field);
        canvas.frame(box, t.border, 1, 3);
    }
    if (focused()) {
        canvas.frame(box.inset(-2), t.focus, 2, 5);
    }
    canvas.text({ size + t.padding, 0, bounds().width - size - t.padding, bounds().height }, text_, t.font(),
                enabled() ? t.text : t.textDisabled);
}

bool CheckBox::key_down(const KeyEvent& key) {
    if (key.key == std::KeySpace && !key.ctrl() && !key.alt()) {
        set_checked(!checked_);
        return true;
    }
    return false;
}

void CheckBox::mouse_up(const MouseEvent& mouse) {
    if (mouse.button == MouseLeft &&
        Rect { 0, 0, bounds().width, bounds().height }.contains(mouse.x, mouse.y)) {
        set_checked(!checked_);
    }
}

// ---- TextBox -----------------------------------------------------------------

TextBox& TextBox::set_text(const char* text) {
    copy_text(text_, sizeof(text_), text);
    length_ = std::strlen(text_);
    caret_ = length_;
    anchor_ = length_;
    scroll_ = 0;
    keep_caret_visible();
    invalidate();
    return *this;
}

TextBox& TextBox::set_placeholder(const char* text) {
    copy_text(placeholder_, sizeof(placeholder_), text);
    invalidate();
    return *this;
}

void TextBox::select_all() {
    anchor_ = 0;
    caret_ = length_;
    keep_caret_visible();
    invalidate();
}

void TextBox::paint(Canvas& canvas) {
    const Theme& t = theme();
    const Font& font = t.font();
    const Rect all = { 0, 0, bounds().width, bounds().height };
    canvas.fill_rounded(all, t.radius, t.field);
    canvas.frame(all, focused() ? t.focus : t.border, 1, t.radius);

    const Rect inner = { t.padding, 1, all.width - 2 * t.padding, all.height - 2 };
    Canvas text = canvas.clipped(inner);
    const int top = (all.height - font.line_height()) / 2;
    const int x0 = t.padding - scroll_;
    if (length_ == 0 && !focused()) {
        text.text(x0, top + font.ascent(), placeholder_, font, t.textDim);
        return;
    }
    if (focused() && caret_ != anchor_) {
        const std::size_t from = caret_ < anchor_ ? caret_ : anchor_;
        const std::size_t to = caret_ < anchor_ ? anchor_ : caret_;
        const int left = x0 + font.width(text_, from);
        text.fill({ left, top, font.width(text_ + from, to - from), font.line_height() }, t.selection);
    }
    text.text(x0, top + font.ascent(), text_, font, enabled() ? t.text : t.textDisabled);
    if (focused()) {
        text.fill({ x0 + font.width(text_, caret_), top, 1, font.line_height() }, t.text);
    }
}

std::size_t TextBox::index_at(int x) const {
    const Font& font = theme().font();
    int pen = theme().padding - scroll_;
    for (std::size_t i = 0; i < length_; ++i) {
        const int advance = font.glyph(static_cast<unsigned char>(text_[i])).advance;
        if (x < pen + advance / 2) {
            return i;
        }
        pen += advance;
    }
    return length_;
}

void TextBox::keep_caret_visible() {
    const int width = bounds().width - 2 * theme().padding;
    const int caretX = theme().font().width(text_, caret_);
    if (caretX - scroll_ > width - 1) {
        scroll_ = caretX - width + 1;
    }
    if (caretX < scroll_) {
        scroll_ = caretX;
    }
    if (scroll_ < 0) {
        scroll_ = 0;
    }
}

void TextBox::move_caret(std::size_t to, bool extend) {
    caret_ = to > length_ ? length_ : to;
    if (!extend) {
        anchor_ = caret_;
    }
    keep_caret_visible();
    invalidate();
}

void TextBox::erase_selection() {
    if (caret_ == anchor_) {
        return;
    }
    const std::size_t from = caret_ < anchor_ ? caret_ : anchor_;
    const std::size_t to = caret_ < anchor_ ? anchor_ : caret_;
    std::memmove(text_ + from, text_ + to, length_ - to + 1);
    length_ -= to - from;
    caret_ = from;
    anchor_ = from;
}

void TextBox::insert(const char* text) {
    erase_selection();
    const std::size_t add = std::strlen(text);
    if (add == 0 || length_ + add > kMaxLength) {
        return;
    }
    std::memmove(text_ + caret_ + add, text_ + caret_, length_ - caret_ + 1);
    std::memcpy(text_ + caret_, text, add);
    length_ += add;
    caret_ += add;
    anchor_ = caret_;
}

void TextBox::changed() {
    keep_caret_visible();
    invalidate();
    if (on_change) {
        on_change(text_);
    }
}

bool TextBox::key_down(const KeyEvent& key) {
    // Where Ctrl+Left/Right stop: the start of a word.
    auto word_left = [this](std::size_t at) {
        while (at > 0 && !word_char(text_[at - 1])) --at;
        while (at > 0 && word_char(text_[at - 1])) --at;
        return at;
    };
    auto word_right = [this](std::size_t at) {
        while (at < length_ && word_char(text_[at])) ++at;
        while (at < length_ && !word_char(text_[at])) ++at;
        return at;
    };
    const bool selecting = caret_ != anchor_;
    switch (key.key) {
    case std::KeyLeft:
        if (selecting && !key.shift()) {
            move_caret(caret_ < anchor_ ? caret_ : anchor_, false);
        } else {
            move_caret(key.ctrl() ? word_left(caret_) : (caret_ > 0 ? caret_ - 1 : 0), key.shift());
        }
        return true;
    case std::KeyRight:
        if (selecting && !key.shift()) {
            move_caret(caret_ > anchor_ ? caret_ : anchor_, false);
        } else {
            move_caret(key.ctrl() ? word_right(caret_) : caret_ + 1, key.shift());
        }
        return true;
    case std::KeyHome:
        move_caret(0, key.shift());
        return true;
    case std::KeyEnd:
        move_caret(length_, key.shift());
        return true;
    case std::KeyBackspace:
        if (!selecting && caret_ > 0) {
            anchor_ = key.ctrl() ? word_left(caret_) : caret_ - 1;
        }
        erase_selection();
        changed();
        return true;
    case std::KeyDelete:
        if (!selecting && caret_ < length_) {
            anchor_ = key.ctrl() ? word_right(caret_) : caret_ + 1;
        }
        erase_selection();
        changed();
        return true;
    case std::KeyA:
        if (key.ctrl()) {
            select_all();
            return true;
        }
        return false;
    case std::KeyEnter:
    case std::KeypadEnter:
        if (on_enter) {
            on_enter(text_);
            return true;
        }
        return false;   // the window's default button
    default:
        return false;
    }
}

bool TextBox::char_typed(const char* text) {
    insert(text);
    changed();
    return true;
}

void TextBox::mouse_down(const MouseEvent& mouse) {
    if (mouse.button != MouseLeft) {
        return;
    }
    if (mouse.clicks >= 2) {
        select_all();
        return;
    }
    move_caret(index_at(mouse.x), false);
}

void TextBox::mouse_move(const MouseEvent& mouse) {
    if ((mouse.buttons & MouseLeft) && pressed_capture()) {
        move_caret(index_at(mouse.x), true);
    }
}

// ---- ScrollBar ---------------------------------------------------------------

ScrollBar::ScrollBar(Window* window, bool vertical) : Control(window), vertical_(vertical) {
}

ScrollBar::ScrollBar(Control* parent, bool vertical) : Control(parent), vertical_(vertical) {
}

ScrollBar& ScrollBar::set_range(int total, int page) {
    total_ = total > 0 ? total : 0;
    page_ = page > 0 ? page : 1;
    if (value_ > max_value()) {
        value_ = max_value();
    }
    invalidate();
    return *this;
}

ScrollBar& ScrollBar::set_value(int value) {
    if (value > max_value()) {
        value = max_value();
    }
    if (value < 0) {
        value = 0;
    }
    if (value != value_) {
        value_ = value;
        invalidate();
    }
    return *this;
}

void ScrollBar::scroll_to(int value) {
    const int before = value_;
    set_value(value);
    if (value_ != before && on_scroll) {
        on_scroll(value_);
    }
}

int ScrollBar::length() const {
    return vertical_ ? bounds().height : bounds().width;
}

Rect ScrollBar::thumb() const {
    int size = length();
    int at = 0;
    if (total_ > page_ && total_ > 0) {
        size = length() * page_ / total_;
        if (size < 20) {
            size = 20;
        }
        if (size > length()) {
            size = length();
        }
        at = max_value() > 0 ? (length() - size) * value_ / max_value() : 0;
    }
    return vertical_ ? Rect { 2, at, bounds().width - 4, size } : Rect { at, 2, size, bounds().height - 4 };
}

void ScrollBar::paint(Canvas& canvas) {
    const Theme& t = theme();
    canvas.fill({ 0, 0, bounds().width, bounds().height }, t.track);
    const Rect knob = thumb();
    const int radius = (vertical_ ? knob.width : knob.height) / 2;
    canvas.fill_rounded(knob, radius, dragOffset_ >= 0 ? t.facePressed : hovered() ? t.faceHover : t.face);
}

void ScrollBar::mouse_down(const MouseEvent& mouse) {
    if (mouse.button != MouseLeft) {
        return;
    }
    const Rect knob = thumb();
    const int at = vertical_ ? mouse.y : mouse.x;
    const int start = vertical_ ? knob.y : knob.x;
    const int size = vertical_ ? knob.height : knob.width;
    if (at >= start && at < start + size) {
        dragOffset_ = at - start;
        invalidate();
    } else {
        scroll_to(value_ + (at < start ? -page_ : page_));   // a page towards the click
    }
}

void ScrollBar::mouse_move(const MouseEvent& mouse) {
    if (dragOffset_ < 0 || !(mouse.buttons & MouseLeft)) {
        return;
    }
    const Rect knob = thumb();
    const int size = vertical_ ? knob.height : knob.width;
    const int room = length() - size;
    if (room <= 0) {
        return;
    }
    const int start = (vertical_ ? mouse.y : mouse.x) - dragOffset_;
    scroll_to((start * max_value() + room / 2) / room);
}

void ScrollBar::mouse_up(const MouseEvent&) {
    dragOffset_ = -1;
    invalidate();
}

bool ScrollBar::wheel(int delta) {
    scroll_to(value_ - delta * 3);
    return true;
}

// ---- ListBox -----------------------------------------------------------------

ListBox::ListBox(Window* window) : Control(window), scrollBar_(this) {
    scrollBar_.set_visible(false);
    scrollBar_.on_scroll = [this](int value) {
        top_ = value;
        invalidate();
    };
}

ListBox::ListBox(Control* parent) : Control(parent), scrollBar_(this) {
    scrollBar_.set_visible(false);
    scrollBar_.on_scroll = [this](int value) {
        top_ = value;
        invalidate();
    };
}

int ListBox::visible_rows() const {
    const int rows = (bounds().height - 2) / theme().rowHeight;
    return rows > 0 ? rows : 1;
}

void ListBox::resized() {
    const int width = theme().scrollBarWidth;
    scrollBar_.set_bounds({ bounds().width - width - 1, 1, width, bounds().height - 2 });
    update_scroll_bar();
}

void ListBox::update_scroll_bar() {
    scrollBar_.set_visible(count_ > visible_rows());
    scrollBar_.set_range(count_, visible_rows());
    scrollBar_.set_value(top_);
}

ListBox& ListBox::set_count(int count) {
    count_ = count > 0 ? count : 0;
    if (selected_ >= count_) {
        selected_ = count_ - 1;
    }
    set_top(top_);
    update_scroll_bar();
    invalidate();
    return *this;
}

ListBox& ListBox::set_selected(int row) {
    if (row >= count_) {
        row = count_ - 1;
    }
    if (row < 0) {
        row = count_ > 0 ? 0 : -1;
    }
    if (row != selected_) {
        selected_ = row;
        invalidate();
        if (on_select) {
            on_select(row);
        }
    }
    if (row >= 0) {
        ensure_visible(row);
    }
    return *this;
}

void ListBox::ensure_visible(int row) {
    if (row < top_) {
        set_top(row);
    } else if (row >= top_ + visible_rows()) {
        set_top(row - visible_rows() + 1);
    }
}

void ListBox::set_top(int top) {
    const int maxTop = count_ > visible_rows() ? count_ - visible_rows() : 0;
    top = top > maxTop ? maxTop : top;
    top = top < 0 ? 0 : top;
    if (top != top_) {
        top_ = top;
        invalidate();
    }
    scrollBar_.set_value(top_);
}

void ListBox::paint(Canvas& canvas) {
    const Theme& t = theme();
    const Rect all = { 0, 0, bounds().width, bounds().height };
    canvas.fill(all, t.field);
    canvas.frame(all, focused() ? t.focus : t.border);

    const int rowWidth = all.width - 2 - (scrollBar_.visible() ? t.scrollBarWidth : 0);
    Canvas rows = canvas.clipped({ 1, 1, rowWidth, all.height - 2 });
    for (int r = 0; r < visible_rows() + 1; ++r) {
        const int index = top_ + r;
        if (index >= count_) {
            break;
        }
        const Rect row = { 1, 1 + r * t.rowHeight, rowWidth, t.rowHeight };
        const bool selected = index == selected_;
        if (selected) {
            rows.fill(row, focused() ? t.selection : t.face);
        }
        if (draw_row) {
            draw_row(rows, index, row, selected);
        } else if (row_text) {
            rows.text({ row.x + t.padding, row.y, row.width - 2 * t.padding, row.height }, row_text(index), t.font(),
                      selected && focused() ? t.selectionText : t.text);
        }
    }
}

bool ListBox::key_down(const KeyEvent& key) {
    if (key.ctrl() || key.alt()) {
        return false;
    }
    const int page = visible_rows();
    switch (key.key) {
    case std::KeyUp: set_selected(selected_ - 1); return true;
    case std::KeyDown: set_selected(selected_ + 1); return true;
    case std::KeyPageUp: set_selected(selected_ - page); return true;
    case std::KeyPageDown: set_selected(selected_ + page); return true;
    case std::KeyHome: set_selected(0); return true;
    case std::KeyEnd: set_selected(count_ - 1); return true;
    case std::KeyEnter:
    case std::KeypadEnter:
        if (on_activate && selected_ >= 0) {
            on_activate(selected_);
            return true;
        }
        return false;
    default:
        return false;
    }
}

// A letter jumps to the next row starting with it.
bool ListBox::char_typed(const char* text) {
    if (!row_text || count_ == 0 || text[0] == ' ') {
        return false;
    }
    const char want = lower(text[0]);
    for (int step = 1; step <= count_; ++step) {
        const int index = (selected_ + step + count_) % count_;
        const char* name = row_text(index);
        if (name && lower(name[0]) == want) {
            set_selected(index);
            return true;
        }
    }
    return true;
}

void ListBox::mouse_down(const MouseEvent& mouse) {
    if (mouse.button != MouseLeft) {
        return;
    }
    const int row = top_ + (mouse.y - 1) / theme().rowHeight;
    if (row < 0 || row >= count_) {
        return;
    }
    set_selected(row);
    if (mouse.clicks == 2 && on_activate) {
        on_activate(row);
    }
}

bool ListBox::wheel(int delta) {
    set_top(top_ - delta * 3);
    return true;
}

// ---- ImageView ---------------------------------------------------------------

void ImageView::paint(Canvas& canvas) {
    if (!image_ || !image_->valid()) {
        return;
    }
    const int w = bounds().width;
    const int h = bounds().height;
    const int iw = image_->width();
    const int ih = image_->height();
    Rect dest = { 0, 0, w, h };
    if (fit_ == Fit::Center) {
        dest = { (w - iw) / 2, (h - ih) / 2, iw, ih };
    } else if (fit_ == Fit::Contain || fit_ == Fit::Cover) {
        // Scaled by the smaller (Contain) or larger (Cover) ratio, centred.
        const bool byWidth = (static_cast<long long>(w) * ih <= static_cast<long long>(h) * iw) == (fit_ == Fit::Contain);
        const int dw = byWidth ? w : static_cast<int>(static_cast<long long>(iw) * h / ih);
        const int dh = byWidth ? static_cast<int>(static_cast<long long>(ih) * w / iw) : h;
        dest = { (w - dw) / 2, (h - dh) / 2, dw, dh };
    }
    if (dest.width == iw && dest.height == ih) {
        canvas.image(*image_, Point { dest.x, dest.y });
        return;
    }
    // Scaling is slow (area averaging); it is done when the size changes.
    if (dest.width <= 0 || dest.height <= 0) {
        return;
    }
    if (scaled_.width() != dest.width || scaled_.height() != dest.height) {
        scaled_.scaled_from(*image_, dest.width, dest.height);
    }
    canvas.image(scaled_, Point { dest.x, dest.y });
}

}  // namespace ui
