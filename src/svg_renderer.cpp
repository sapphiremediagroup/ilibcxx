#include <svg_renderer.hpp>

#include <cmath.hpp>
#include <cstddef.hpp>
#include <cstdint.hpp>
#include <cstring.hpp>
#include <vector.hpp>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxAttributes = 96;

struct Span {
    const char* data;
    std::size_t length;
};

struct Attribute {
    Span name;
    Span value;
};

struct Tag {
    Span name;
    Attribute attributes[kMaxAttributes];
    int attributeCount;
    bool valid;
    bool closing;
    bool selfClosing;
};

struct Point {
    double x;
    double y;
};

struct Edge {
    double x0;
    double y0;
    double x1;
    double y1;
};

struct Intersection {
    double x;
    int winding;
};

struct Affine {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
};

enum class FillRule : std::uint8_t {
    NonZero,
    EvenOdd,
};

struct Paint {
    bool active;
    std::uint32_t color;
    double alpha;
};

struct Style {
    Affine transform;
    Paint fill;
    Paint stroke;
    double opacity;
    double fillOpacity;
    double strokeOpacity;
    double strokeWidth;
    FillRule fillRule;
    bool visible;
};

struct Viewport {
    double viewX;
    double viewY;
    double viewWidth;
    double viewHeight;
    double width;
    double height;
    bool found;
};

bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool is_name_end(char c) noexcept {
    return c == '\0' || is_space(c) || c == '/' || c == '>' || c == '=';
}

bool is_alpha(char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

char to_lower_ascii(char c) noexcept {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

double clamp_double(double value, double minValue, double maxValue) noexcept {
    if (value < minValue) {
        return minValue;
    }
    return value > maxValue ? maxValue : value;
}

int floor_to_int(double value) noexcept {
    return static_cast<int>(std::floor(value));
}

int ceil_to_int(double value) noexcept {
    return static_cast<int>(std::ceil(value));
}

double abs_double(double value) noexcept {
    return value < 0.0 ? -value : value;
}

double min_double(double left, double right) noexcept {
    return left < right ? left : right;
}

double max_double(double left, double right) noexcept {
    return left > right ? left : right;
}

Span trim_span(Span span) noexcept {
    while (span.length != 0 && is_space(span.data[0])) {
        ++span.data;
        --span.length;
    }
    while (span.length != 0 && is_space(span.data[span.length - 1])) {
        --span.length;
    }
    return span;
}

Span local_name(Span name) noexcept {
    for (std::size_t i = 0; i < name.length; ++i) {
        if (name.data[i] == ':') {
            return { name.data + i + 1, name.length - i - 1 };
        }
    }
    return name;
}

bool span_equals(Span span, const char* expected) noexcept {
    span = local_name(trim_span(span));
    std::size_t index = 0;
    while (expected[index] != '\0') {
        if (index >= span.length || span.data[index] != expected[index]) {
            return false;
        }
        ++index;
    }
    return index == span.length;
}

bool span_equals_case_insensitive(Span span, const char* expected) noexcept {
    span = local_name(trim_span(span));
    std::size_t index = 0;
    while (expected[index] != '\0') {
        if (index >= span.length || to_lower_ascii(span.data[index]) != to_lower_ascii(expected[index])) {
            return false;
        }
        ++index;
    }
    return index == span.length;
}

bool span_starts_with_case_insensitive(Span span, const char* expected) noexcept {
    span = trim_span(span);
    std::size_t index = 0;
    while (expected[index] != '\0') {
        if (index >= span.length || to_lower_ascii(span.data[index]) != to_lower_ascii(expected[index])) {
            return false;
        }
        ++index;
    }
    return true;
}

bool same_point(Point left, Point right) noexcept {
    return abs_double(left.x - right.x) < 0.000001 && abs_double(left.y - right.y) < 0.000001;
}

double pow10_int(int exponent) noexcept {
    if (exponent > 308) {
        exponent = 308;
    } else if (exponent < -308) {
        exponent = -308;
    }

    double result = 1.0;
    int count = exponent < 0 ? -exponent : exponent;
    while (count-- > 0) {
        result *= 10.0;
    }
    return exponent < 0 ? (1.0 / result) : result;
}

class NumberParser {
public:
    explicit NumberParser(Span span) : span_(span), position_(0) {}

    void skip_separators() noexcept {
        while (position_ < span_.length) {
            const char c = span_.data[position_];
            if (!is_space(c) && c != ',') {
                break;
            }
            ++position_;
        }
    }

    bool has_number() noexcept {
        skip_separators();
        if (position_ >= span_.length) {
            return false;
        }
        const char c = span_.data[position_];
        return c == '+' || c == '-' || c == '.' || (c >= '0' && c <= '9');
    }

    bool parse(double* out) noexcept {
        skip_separators();
        if (position_ >= span_.length) {
            return false;
        }

        double sign = 1.0;
        if (span_.data[position_] == '+') {
            ++position_;
        } else if (span_.data[position_] == '-') {
            sign = -1.0;
            ++position_;
        }

        bool hasDigits = false;
        double value = 0.0;
        while (position_ < span_.length && span_.data[position_] >= '0' && span_.data[position_] <= '9') {
            hasDigits = true;
            value = (value * 10.0) + static_cast<double>(span_.data[position_] - '0');
            ++position_;
        }

        if (position_ < span_.length && span_.data[position_] == '.') {
            ++position_;
            double place = 0.1;
            while (position_ < span_.length && span_.data[position_] >= '0' && span_.data[position_] <= '9') {
                hasDigits = true;
                value += static_cast<double>(span_.data[position_] - '0') * place;
                place *= 0.1;
                ++position_;
            }
        }

        if (!hasDigits) {
            return false;
        }

        int exponent = 0;
        if (position_ < span_.length && (span_.data[position_] == 'e' || span_.data[position_] == 'E')) {
            const std::size_t exponentStart = position_;
            ++position_;
            int exponentSign = 1;
            if (position_ < span_.length && span_.data[position_] == '+') {
                ++position_;
            } else if (position_ < span_.length && span_.data[position_] == '-') {
                exponentSign = -1;
                ++position_;
            }

            bool hasExponentDigits = false;
            while (position_ < span_.length && span_.data[position_] >= '0' && span_.data[position_] <= '9') {
                hasExponentDigits = true;
                exponent = (exponent * 10) + (span_.data[position_] - '0');
                ++position_;
            }

            if (hasExponentDigits) {
                exponent *= exponentSign;
            } else {
                position_ = exponentStart;
            }
        }

        *out = sign * value * pow10_int(exponent);
        return true;
    }

    std::size_t position() const noexcept { return position_; }
    void set_position(std::size_t position) noexcept { position_ = position; }

private:
    Span span_;
    std::size_t position_;
};

Affine identity_matrix() noexcept {
    return { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
}

Affine translate_matrix(double x, double y) noexcept {
    return { 1.0, 0.0, 0.0, 1.0, x, y };
}

Affine scale_matrix(double x, double y) noexcept {
    return { x, 0.0, 0.0, y, 0.0, 0.0 };
}

Affine rotate_matrix(double degrees) noexcept {
    const double radians = degrees * kPi / 180.0;
    const double s = std::sin(radians);
    const double c = std::cos(radians);
    return { c, s, -s, c, 0.0, 0.0 };
}

Affine multiply_matrix(const Affine& left, const Affine& right) noexcept {
    return {
        left.a * right.a + left.c * right.b,
        left.b * right.a + left.d * right.b,
        left.a * right.c + left.c * right.d,
        left.b * right.c + left.d * right.d,
        left.a * right.e + left.c * right.f + left.e,
        left.b * right.e + left.d * right.f + left.f,
    };
}

Point transform_point(const Affine& matrix, Point point) noexcept {
    return {
        matrix.a * point.x + matrix.c * point.y + matrix.e,
        matrix.b * point.x + matrix.d * point.y + matrix.f,
    };
}

double matrix_average_scale(const Affine& matrix) noexcept {
    const double xScale = std::sqrt(matrix.a * matrix.a + matrix.b * matrix.b);
    const double yScale = std::sqrt(matrix.c * matrix.c + matrix.d * matrix.d);
    const double scale = (xScale + yScale) * 0.5;
    return scale > 0.0 ? scale : 1.0;
}

void skip_until(const char* data, std::size_t length, std::size_t* position, const char* marker) noexcept {
    const std::size_t markerLength = std::strlen(marker);
    while (*position + markerLength <= length) {
        bool match = true;
        for (std::size_t i = 0; i < markerLength; ++i) {
            if (data[*position + i] != marker[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            *position += markerLength;
            return;
        }
        ++*position;
    }
    *position = length;
}

bool parse_tag(const char* data, std::size_t length, std::size_t* position, Tag* tag) noexcept {
    *tag = {};
    if (*position >= length || data[*position] != '<') {
        return false;
    }

    std::size_t pos = *position + 1;
    if (pos >= length) {
        *position = length;
        return false;
    }

    if (data[pos] == '!') {
        if (pos + 2 < length && data[pos + 1] == '-' && data[pos + 2] == '-') {
            pos += 3;
            skip_until(data, length, &pos, "-->");
        } else {
            while (pos < length && data[pos] != '>') {
                ++pos;
            }
            if (pos < length) {
                ++pos;
            }
        }
        *position = pos;
        return true;
    }

    if (data[pos] == '?') {
        ++pos;
        skip_until(data, length, &pos, "?>");
        *position = pos;
        return true;
    }

    if (data[pos] == '/') {
        tag->closing = true;
        ++pos;
    }

    while (pos < length && is_space(data[pos])) {
        ++pos;
    }

    const std::size_t nameStart = pos;
    while (pos < length && !is_name_end(data[pos])) {
        ++pos;
    }
    if (pos == nameStart) {
        *position = pos;
        return false;
    }
    tag->name = { data + nameStart, pos - nameStart };
    tag->valid = true;

    if (tag->closing) {
        while (pos < length && data[pos] != '>') {
            ++pos;
        }
        if (pos < length) {
            ++pos;
        }
        *position = pos;
        return true;
    }

    while (pos < length) {
        while (pos < length && is_space(data[pos])) {
            ++pos;
        }
        if (pos >= length) {
            break;
        }
        if (data[pos] == '>') {
            ++pos;
            break;
        }
        if (data[pos] == '/') {
            tag->selfClosing = true;
            ++pos;
            while (pos < length && data[pos] != '>') {
                ++pos;
            }
            if (pos < length) {
                ++pos;
            }
            break;
        }

        const std::size_t attrNameStart = pos;
        while (pos < length && !is_name_end(data[pos])) {
            ++pos;
        }
        const Span attrName = { data + attrNameStart, pos - attrNameStart };
        while (pos < length && is_space(data[pos])) {
            ++pos;
        }

        Span attrValue = { nullptr, 0 };
        if (pos < length && data[pos] == '=') {
            ++pos;
            while (pos < length && is_space(data[pos])) {
                ++pos;
            }
            if (pos < length && (data[pos] == '"' || data[pos] == '\'')) {
                const char quote = data[pos++];
                const std::size_t valueStart = pos;
                while (pos < length && data[pos] != quote) {
                    ++pos;
                }
                attrValue = { data + valueStart, pos - valueStart };
                if (pos < length) {
                    ++pos;
                }
            } else {
                const std::size_t valueStart = pos;
                while (pos < length && !is_space(data[pos]) && data[pos] != '/' && data[pos] != '>') {
                    ++pos;
                }
                attrValue = { data + valueStart, pos - valueStart };
            }
        }

        if (tag->attributeCount < kMaxAttributes && attrName.length != 0) {
            tag->attributes[tag->attributeCount++] = { attrName, attrValue };
        }
    }

    *position = pos;
    return true;
}

Span find_attribute(const Tag& tag, const char* name) noexcept {
    for (int i = 0; i < tag.attributeCount; ++i) {
        if (span_equals(tag.attributes[i].name, name)) {
            return tag.attributes[i].value;
        }
    }
    return { nullptr, 0 };
}

bool parse_number(Span span, double* value) noexcept {
    NumberParser parser(span);
    return parser.parse(value);
}

bool parse_length_attribute(const Tag& tag, const char* name, double* value) noexcept {
    const Span span = find_attribute(tag, name);
    if (!span.data) {
        return false;
    }
    return parse_number(span, value);
}

bool parse_view_box(Span span, double* x, double* y, double* width, double* height) noexcept {
    NumberParser parser(span);
    return parser.parse(x) && parser.parse(y) && parser.parse(width) && parser.parse(height) && *width > 0.0 && *height > 0.0;
}

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool parse_hex_color(Span span, Paint* paint) noexcept {
    span = trim_span(span);
    if (span.length < 4 || span.data[0] != '#') {
        return false;
    }

    const std::size_t digits = span.length - 1;
    std::uint32_t r = 0;
    std::uint32_t g = 0;
    std::uint32_t b = 0;
    std::uint32_t a = 255;

    if (digits == 3 || digits == 4) {
        const int rv = hex_value(span.data[1]);
        const int gv = hex_value(span.data[2]);
        const int bv = hex_value(span.data[3]);
        if (rv < 0 || gv < 0 || bv < 0) {
            return false;
        }
        r = static_cast<std::uint32_t>((rv << 4) | rv);
        g = static_cast<std::uint32_t>((gv << 4) | gv);
        b = static_cast<std::uint32_t>((bv << 4) | bv);
        if (digits == 4) {
            const int av = hex_value(span.data[4]);
            if (av < 0) {
                return false;
            }
            a = static_cast<std::uint32_t>((av << 4) | av);
        }
    } else if (digits == 6 || digits == 8) {
        std::uint32_t values[8] = {};
        for (std::size_t i = 0; i < digits; ++i) {
            const int value = hex_value(span.data[i + 1]);
            if (value < 0) {
                return false;
            }
            values[i] = static_cast<std::uint32_t>(value);
        }
        r = (values[0] << 4) | values[1];
        g = (values[2] << 4) | values[3];
        b = (values[4] << 4) | values[5];
        if (digits == 8) {
            a = (values[6] << 4) | values[7];
        }
    } else {
        return false;
    }

    paint->active = true;
    paint->color = (r << 16) | (g << 8) | b;
    paint->alpha = static_cast<double>(a) / 255.0;
    return true;
}

struct NamedColor {
    const char* name;
    std::uint32_t value;
};

bool parse_named_color(Span span, Paint* paint) noexcept {
    static constexpr NamedColor colors[] = {
        { "black", 0x000000 },
        { "white", 0xFFFFFF },
        { "red", 0xFF0000 },
        { "green", 0x008000 },
        { "blue", 0x0000FF },
        { "yellow", 0xFFFF00 },
        { "cyan", 0x00FFFF },
        { "magenta", 0xFF00FF },
        { "gray", 0x808080 },
        { "grey", 0x808080 },
        { "silver", 0xC0C0C0 },
        { "maroon", 0x800000 },
        { "purple", 0x800080 },
        { "navy", 0x000080 },
        { "teal", 0x008080 },
        { "lime", 0x00FF00 },
        { "orange", 0xFFA500 },
    };

    for (const auto& color : colors) {
        if (span_equals_case_insensitive(span, color.name)) {
            paint->active = true;
            paint->color = color.value;
            paint->alpha = 1.0;
            return true;
        }
    }
    return false;
}

bool parse_rgb_color(Span span, Paint* paint) noexcept {
    span = trim_span(span);
    if (!span_starts_with_case_insensitive(span, "rgb")) {
        return false;
    }

    std::size_t open = 0;
    while (open < span.length && span.data[open] != '(') {
        ++open;
    }
    if (open == span.length) {
        return false;
    }

    std::size_t close = span.length;
    while (close > open && span.data[close - 1] != ')') {
        --close;
    }
    if (close <= open + 1) {
        return false;
    }

    Span body = { span.data + open + 1, close - open - 2 };
    NumberParser parser(body);
    double values[4] = { 0.0, 0.0, 0.0, 1.0 };
    bool percentages[4] = {};
    int count = 0;
    while (count < 4 && parser.has_number()) {
        if (!parser.parse(&values[count])) {
            break;
        }
        std::size_t pos = parser.position();
        while (pos < body.length && is_space(body.data[pos])) {
            ++pos;
        }
        if (pos < body.length && body.data[pos] == '%') {
            percentages[count] = true;
            parser.set_position(pos + 1);
        }
        ++count;
        parser.skip_separators();
        pos = parser.position();
        if (pos < body.length && body.data[pos] == '/') {
            parser.set_position(pos + 1);
        }
    }
    if (count < 3) {
        return false;
    }

    std::uint32_t channels[3] = {};
    for (int i = 0; i < 3; ++i) {
        const double value = percentages[i] ? (values[i] * 2.55) : values[i];
        channels[i] = static_cast<std::uint32_t>(clamp_double(value, 0.0, 255.0) + 0.5);
    }

    double alpha = 1.0;
    if (count >= 4) {
        alpha = percentages[3] ? (values[3] / 100.0) : values[3];
    }

    paint->active = true;
    paint->color = (channels[0] << 16) | (channels[1] << 8) | channels[2];
    paint->alpha = clamp_double(alpha, 0.0, 1.0);
    return true;
}

bool parse_paint(Span span, Paint* paint) noexcept {
    span = trim_span(span);
    if (span.length == 0) {
        return false;
    }
    if (span_equals_case_insensitive(span, "none")) {
        paint->active = false;
        paint->alpha = 0.0;
        return true;
    }
    if (span_equals_case_insensitive(span, "transparent")) {
        paint->active = true;
        paint->color = 0x000000;
        paint->alpha = 0.0;
        return true;
    }
    if (span_starts_with_case_insensitive(span, "url(")) {
        paint->active = false;
        paint->alpha = 0.0;
        return true;
    }
    if (span_equals_case_insensitive(span, "currentColor")) {
        paint->active = true;
        paint->color = 0x000000;
        paint->alpha = 1.0;
        return true;
    }
    return parse_hex_color(span, paint) || parse_rgb_color(span, paint) || parse_named_color(span, paint);
}

std::uint8_t effective_alpha(const Paint& paint, double opacity, double paintOpacity) noexcept {
    const double alpha = clamp_double(paint.alpha * opacity * paintOpacity, 0.0, 1.0);
    return static_cast<std::uint8_t>((alpha * 255.0) + 0.5);
}

void apply_property(Style* style, Span propertyName, Span propertyValue) noexcept {
    propertyName = trim_span(propertyName);
    propertyValue = trim_span(propertyValue);

    if (span_equals(propertyName, "fill")) {
        Paint paint = style->fill;
        if (parse_paint(propertyValue, &paint)) {
            style->fill = paint;
        }
        return;
    }
    if (span_equals(propertyName, "stroke")) {
        Paint paint = style->stroke;
        if (parse_paint(propertyValue, &paint)) {
            style->stroke = paint;
        }
        return;
    }
    if (span_equals(propertyName, "stroke-width")) {
        double width = 0.0;
        if (parse_number(propertyValue, &width)) {
            style->strokeWidth = width < 0.0 ? 0.0 : width;
        }
        return;
    }
    if (span_equals(propertyName, "opacity")) {
        double opacity = 1.0;
        if (parse_number(propertyValue, &opacity)) {
            style->opacity *= clamp_double(opacity, 0.0, 1.0);
        }
        return;
    }
    if (span_equals(propertyName, "fill-opacity")) {
        double opacity = 1.0;
        if (parse_number(propertyValue, &opacity)) {
            style->fillOpacity = clamp_double(opacity, 0.0, 1.0);
        }
        return;
    }
    if (span_equals(propertyName, "stroke-opacity")) {
        double opacity = 1.0;
        if (parse_number(propertyValue, &opacity)) {
            style->strokeOpacity = clamp_double(opacity, 0.0, 1.0);
        }
        return;
    }
    if (span_equals(propertyName, "fill-rule")) {
        if (span_equals_case_insensitive(propertyValue, "evenodd")) {
            style->fillRule = FillRule::EvenOdd;
        } else if (span_equals_case_insensitive(propertyValue, "nonzero")) {
            style->fillRule = FillRule::NonZero;
        }
        return;
    }
    if (span_equals(propertyName, "display")) {
        if (span_equals_case_insensitive(propertyValue, "none")) {
            style->visible = false;
        }
        return;
    }
    if (span_equals(propertyName, "visibility")) {
        if (span_equals_case_insensitive(propertyValue, "hidden") || span_equals_case_insensitive(propertyValue, "collapse")) {
            style->visible = false;
        } else if (span_equals_case_insensitive(propertyValue, "visible")) {
            style->visible = true;
        }
    }
}

void apply_style_attribute(Style* style, Span styleAttribute) noexcept {
    std::size_t start = 0;
    while (start < styleAttribute.length) {
        std::size_t end = start;
        while (end < styleAttribute.length && styleAttribute.data[end] != ';') {
            ++end;
        }

        std::size_t colon = start;
        while (colon < end && styleAttribute.data[colon] != ':') {
            ++colon;
        }
        if (colon < end) {
            apply_property(style,
                           { styleAttribute.data + start, colon - start },
                           { styleAttribute.data + colon + 1, end - colon - 1 });
        }

        start = end + 1;
    }
}

Affine parse_transform(Span span) noexcept {
    Affine result = identity_matrix();
    std::size_t pos = 0;
    while (pos < span.length) {
        while (pos < span.length && (is_space(span.data[pos]) || span.data[pos] == ',')) {
            ++pos;
        }
        const std::size_t nameStart = pos;
        while (pos < span.length && is_alpha(span.data[pos])) {
            ++pos;
        }
        Span name = { span.data + nameStart, pos - nameStart };
        while (pos < span.length && is_space(span.data[pos])) {
            ++pos;
        }
        if (name.length == 0 || pos >= span.length || span.data[pos] != '(') {
            break;
        }
        ++pos;
        const std::size_t argsStart = pos;
        int depth = 1;
        while (pos < span.length && depth > 0) {
            if (span.data[pos] == '(') {
                ++depth;
            } else if (span.data[pos] == ')') {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
            ++pos;
        }
        if (pos >= span.length) {
            break;
        }

        NumberParser parser({ span.data + argsStart, pos - argsStart });
        double values[6] = {};
        int count = 0;
        while (count < 6 && parser.has_number() && parser.parse(&values[count])) {
            ++count;
        }

        Affine current = identity_matrix();
        if (span_equals_case_insensitive(name, "matrix") && count == 6) {
            current = { values[0], values[1], values[2], values[3], values[4], values[5] };
        } else if (span_equals_case_insensitive(name, "translate") && count >= 1) {
            current = translate_matrix(values[0], count >= 2 ? values[1] : 0.0);
        } else if (span_equals_case_insensitive(name, "scale") && count >= 1) {
            current = scale_matrix(values[0], count >= 2 ? values[1] : values[0]);
        } else if (span_equals_case_insensitive(name, "rotate") && count >= 1) {
            current = rotate_matrix(values[0]);
            if (count >= 3) {
                current = multiply_matrix(multiply_matrix(translate_matrix(values[1], values[2]), current), translate_matrix(-values[1], -values[2]));
            }
        } else if (span_equals_case_insensitive(name, "skewX") && count >= 1) {
            const double radians = values[0] * kPi / 180.0;
            current = { 1.0, 0.0, std::tan(radians), 1.0, 0.0, 0.0 };
        } else if (span_equals_case_insensitive(name, "skewY") && count >= 1) {
            const double radians = values[0] * kPi / 180.0;
            current = { 1.0, std::tan(radians), 0.0, 1.0, 0.0, 0.0 };
        }

        result = multiply_matrix(result, current);
        ++pos;
    }
    return result;
}

bool is_definition_container(Span name) noexcept {
    return span_equals(name, "defs") || span_equals(name, "clipPath") || span_equals(name, "mask") ||
           span_equals(name, "pattern") || span_equals(name, "filter") || span_equals(name, "symbol") ||
           span_equals(name, "linearGradient") || span_equals(name, "radialGradient") || span_equals(name, "style") ||
           span_equals(name, "script") || span_equals(name, "metadata") || span_equals(name, "title") ||
           span_equals(name, "desc");
}

Style style_from_tag(const Style& parent, const Tag& tag) noexcept {
    Style style = parent;
    for (int i = 0; i < tag.attributeCount; ++i) {
        if (!span_equals(tag.attributes[i].name, "style") && !span_equals(tag.attributes[i].name, "transform")) {
            apply_property(&style, tag.attributes[i].name, tag.attributes[i].value);
        }
    }

    for (int i = 0; i < tag.attributeCount; ++i) {
        if (span_equals(tag.attributes[i].name, "style")) {
            apply_style_attribute(&style, tag.attributes[i].value);
        }
    }

    const Span transform = find_attribute(tag, "transform");
    if (transform.data) {
        style.transform = multiply_matrix(parent.transform, parse_transform(transform));
    }

    if (is_definition_container(tag.name)) {
        style.visible = false;
    }
    return style;
}

class RenderContext {
public:
    explicit RenderContext(const std::SvgRenderer::PixelBuffer& target)
        : target_(target) {}

    bool valid() const noexcept {
        return target_.pixels != nullptr && target_.width > 0 && target_.height > 0;
    }

    int pitch() const noexcept {
        return target_.pitch > 0 ? target_.pitch : target_.width;
    }

    void clear_rect(int x, int y, int width, int height, std::uint32_t color) noexcept {
        if (width <= 0 || height <= 0) {
            return;
        }
        int startX = x < 0 ? 0 : x;
        int startY = y < 0 ? 0 : y;
        int endX = x + width;
        int endY = y + height;
        if (endX > target_.width) {
            endX = target_.width;
        }
        if (endY > target_.height) {
            endY = target_.height;
        }
        if (startX >= endX || startY >= endY) {
            return;
        }
        for (int drawY = startY; drawY < endY; ++drawY) {
            for (int drawX = startX; drawX < endX; ++drawX) {
                target_.pixels[drawY * pitch() + drawX] = color & 0x00FFFFFFU;
            }
        }
    }

    void blend_pixel(int x, int y, std::uint32_t color, std::uint8_t alpha) noexcept {
        if (alpha == 0 || x < 0 || y < 0 || x >= target_.width || y >= target_.height) {
            return;
        }

        std::uint32_t& dst = target_.pixels[y * pitch() + x];
        color &= 0x00FFFFFFU;
        if (alpha == 255) {
            dst = color;
            return;
        }

        const std::uint32_t inv = 255U - alpha;
        const std::uint32_t dstR = (dst >> 16) & 0xFFU;
        const std::uint32_t dstG = (dst >> 8) & 0xFFU;
        const std::uint32_t dstB = dst & 0xFFU;
        const std::uint32_t srcR = (color >> 16) & 0xFFU;
        const std::uint32_t srcG = (color >> 8) & 0xFFU;
        const std::uint32_t srcB = color & 0xFFU;
        const std::uint32_t r = (dstR * inv + srcR * alpha) / 255U;
        const std::uint32_t g = (dstG * inv + srcG * alpha) / 255U;
        const std::uint32_t b = (dstB * inv + srcB * alpha) / 255U;
        dst = (r << 16) | (g << 8) | b;
    }

    void fill_span(int y, double left, double right, std::uint32_t color, std::uint8_t alpha) noexcept {
        if (right <= left || y < 0 || y >= target_.height) {
            return;
        }
        int startX = ceil_to_int(left - 0.5);
        int endX = floor_to_int(right - 0.5);
        if (startX < 0) {
            startX = 0;
        }
        if (endX >= target_.width) {
            endX = target_.width - 1;
        }
        for (int x = startX; x <= endX; ++x) {
            blend_pixel(x, y, color, alpha);
        }
    }

    void fill_edges(const std::vector<Edge>& edges, std::uint32_t color, std::uint8_t alpha, FillRule fillRule) {
        if (edges.empty() || alpha == 0) {
            return;
        }

        double minY = edges[0].y0;
        double maxY = edges[0].y0;
        for (std::size_t i = 0; i < edges.size(); ++i) {
            minY = min_double(minY, min_double(edges[i].y0, edges[i].y1));
            maxY = max_double(maxY, max_double(edges[i].y0, edges[i].y1));
        }

        int startY = floor_to_int(minY);
        int endY = ceil_to_int(maxY);
        if (startY < 0) {
            startY = 0;
        }
        if (endY > target_.height) {
            endY = target_.height;
        }

        std::vector<Intersection> intersections;
        for (int y = startY; y < endY; ++y) {
            const double sampleY = static_cast<double>(y) + 0.5;
            intersections.clear();
            for (std::size_t i = 0; i < edges.size(); ++i) {
                const Edge& edge = edges[i];
                if (edge.y0 == edge.y1) {
                    continue;
                }
                if ((edge.y0 <= sampleY && edge.y1 > sampleY) || (edge.y1 <= sampleY && edge.y0 > sampleY)) {
                    const double t = (sampleY - edge.y0) / (edge.y1 - edge.y0);
                    const double x = edge.x0 + (edge.x1 - edge.x0) * t;
                    intersections.push_back({ x, edge.y1 > edge.y0 ? 1 : -1 });
                }
            }

            sort_intersections(intersections);
            if (fillRule == FillRule::EvenOdd) {
                for (std::size_t i = 0; i + 1 < intersections.size(); i += 2) {
                    fill_span(y, intersections[i].x, intersections[i + 1].x, color, alpha);
                }
            } else {
                int winding = 0;
                double spanStart = 0.0;
                for (std::size_t i = 0; i < intersections.size(); ++i) {
                    if (winding == 0) {
                        spanStart = intersections[i].x;
                    }
                    winding += intersections[i].winding;
                    if (winding == 0) {
                        fill_span(y, spanStart, intersections[i].x, color, alpha);
                    }
                }
            }
        }
    }

    void stroke_segment(Point a, Point b, double width, std::uint32_t color, std::uint8_t alpha) noexcept {
        if (alpha == 0 || width <= 0.0) {
            return;
        }

        const double dx = b.x - a.x;
        const double dy = b.y - a.y;
        const double lengthSquared = dx * dx + dy * dy;
        const double half = width * 0.5;
        const double radius = half + 1.0;
        int startX = floor_to_int(min_double(a.x, b.x) - radius);
        int endX = ceil_to_int(max_double(a.x, b.x) + radius);
        int startY = floor_to_int(min_double(a.y, b.y) - radius);
        int endY = ceil_to_int(max_double(a.y, b.y) + radius);
        if (startX < 0) {
            startX = 0;
        }
        if (startY < 0) {
            startY = 0;
        }
        if (endX > target_.width) {
            endX = target_.width;
        }
        if (endY > target_.height) {
            endY = target_.height;
        }

        for (int y = startY; y < endY; ++y) {
            const double py = static_cast<double>(y) + 0.5;
            for (int x = startX; x < endX; ++x) {
                const double px = static_cast<double>(x) + 0.5;
                double closestX = a.x;
                double closestY = a.y;
                if (lengthSquared > 0.0) {
                    const double t = clamp_double(((px - a.x) * dx + (py - a.y) * dy) / lengthSquared, 0.0, 1.0);
                    closestX = a.x + dx * t;
                    closestY = a.y + dy * t;
                }

                const double distanceX = px - closestX;
                const double distanceY = py - closestY;
                const double distance = std::sqrt(distanceX * distanceX + distanceY * distanceY);
                if (distance > radius) {
                    continue;
                }

                double coverage = half + 0.5 - distance;
                if (coverage <= 0.0) {
                    continue;
                }
                if (coverage > 1.0) {
                    coverage = 1.0;
                }
                blend_pixel(x, y, color, static_cast<std::uint8_t>((static_cast<double>(alpha) * coverage) + 0.5));
            }
        }
    }

private:
    static void sort_intersections(std::vector<Intersection>& intersections) {
        for (std::size_t i = 1; i < intersections.size(); ++i) {
            Intersection value = intersections[i];
            std::size_t j = i;
            while (j > 0 && intersections[j - 1].x > value.x) {
                intersections[j] = intersections[j - 1];
                --j;
            }
            intersections[j] = value;
        }
    }

    std::SvgRenderer::PixelBuffer target_;
};

class PathDraw {
public:
    PathDraw(RenderContext& context, const Style& style, bool fillEnabled, bool strokeEnabled)
        : context_(context),
          style_(style),
          fillEnabled_(fillEnabled && style.visible && style.fill.active),
          strokeEnabled_(strokeEnabled && style.visible && style.stroke.active),
          fillAlpha_(effective_alpha(style.fill, style.opacity, style.fillOpacity)),
          strokeAlpha_(effective_alpha(style.stroke, style.opacity, style.strokeOpacity)),
          strokeWidth_(style.strokeWidth * matrix_average_scale(style.transform)),
          current_({ 0.0, 0.0 }),
          start_({ 0.0, 0.0 }),
          hasCurrent_(false),
          hasOpenContour_(false) {}

    void move_to(Point point) {
        close_fill_contour();
        current_ = point;
        start_ = point;
        hasCurrent_ = true;
        hasOpenContour_ = true;
    }

    void line_to(Point point) {
        if (!hasCurrent_) {
            move_to(point);
            return;
        }
        if (!hasOpenContour_) {
            start_ = current_;
            hasOpenContour_ = true;
        }

        add_segment(current_, point, true);
        current_ = point;
    }

    void cubic_to(Point c1, Point c2, Point point) {
        if (!hasCurrent_) {
            move_to(point);
            return;
        }
        const Point p0 = current_;
        const int steps = curve_steps(p0, c1, c2, point);
        for (int i = 1; i <= steps; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(steps);
            const double mt = 1.0 - t;
            Point out = {
                (mt * mt * mt * p0.x) + (3.0 * mt * mt * t * c1.x) + (3.0 * mt * t * t * c2.x) + (t * t * t * point.x),
                (mt * mt * mt * p0.y) + (3.0 * mt * mt * t * c1.y) + (3.0 * mt * t * t * c2.y) + (t * t * t * point.y),
            };
            line_to(out);
        }
    }

    void quad_to(Point c, Point point) {
        if (!hasCurrent_) {
            move_to(point);
            return;
        }
        const Point p0 = current_;
        const int steps = curve_steps(p0, c, c, point);
        for (int i = 1; i <= steps; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(steps);
            const double mt = 1.0 - t;
            Point out = {
                (mt * mt * p0.x) + (2.0 * mt * t * c.x) + (t * t * point.x),
                (mt * mt * p0.y) + (2.0 * mt * t * c.y) + (t * t * point.y),
            };
            line_to(out);
        }
    }

    void close_path() {
        if (!hasCurrent_ || !hasOpenContour_) {
            return;
        }
        if (!same_point(current_, start_)) {
            add_segment(current_, start_, true);
        }
        current_ = start_;
        hasOpenContour_ = false;
    }

    void finish() {
        close_fill_contour();
        if (fillEnabled_ && fillAlpha_ != 0) {
            context_.fill_edges(fillEdges_, style_.fill.color, fillAlpha_, style_.fillRule);
        }
    }

    Point current() const noexcept { return current_; }
    bool has_current() const noexcept { return hasCurrent_; }

private:
    static double distance(Point left, Point right) noexcept {
        const double dx = right.x - left.x;
        const double dy = right.y - left.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static int curve_steps(Point p0, Point p1, Point p2, Point p3) noexcept {
        const double length = distance(p0, p1) + distance(p1, p2) + distance(p2, p3);
        int steps = static_cast<int>(length / 8.0);
        if (steps < 8) {
            steps = 8;
        }
        if (steps > 64) {
            steps = 64;
        }
        return steps;
    }

    void add_segment(Point from, Point to, bool stroke) {
        const Point a = transform_point(style_.transform, from);
        const Point b = transform_point(style_.transform, to);
        if (fillEnabled_) {
            fillEdges_.push_back({ a.x, a.y, b.x, b.y });
        }
        if (stroke && strokeEnabled_ && strokeAlpha_ != 0 && strokeWidth_ > 0.0) {
            context_.stroke_segment(a, b, strokeWidth_, style_.stroke.color, strokeAlpha_);
        }
    }

    void close_fill_contour() {
        if (!hasCurrent_ || !hasOpenContour_) {
            return;
        }
        if (!same_point(current_, start_)) {
            add_segment(current_, start_, false);
        }
        hasOpenContour_ = false;
    }

    RenderContext& context_;
    const Style& style_;
    bool fillEnabled_;
    bool strokeEnabled_;
    std::uint8_t fillAlpha_;
    std::uint8_t strokeAlpha_;
    double strokeWidth_;
    std::vector<Edge> fillEdges_;
    Point current_;
    Point start_;
    bool hasCurrent_;
    bool hasOpenContour_;
};

void add_ellipse_arc(PathDraw& path, double cx, double cy, double rx, double ry, double startRadians, double endRadians, int steps) {
    for (int i = 1; i <= steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps);
        const double angle = startRadians + (endRadians - startRadians) * t;
        path.line_to({ cx + std::cos(angle) * rx, cy + std::sin(angle) * ry });
    }
}

void draw_rect(RenderContext& context, const Style& style, const Tag& tag) {
    if (!style.visible) {
        return;
    }
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    parse_length_attribute(tag, "x", &x);
    parse_length_attribute(tag, "y", &y);
    if (!parse_length_attribute(tag, "width", &width) || !parse_length_attribute(tag, "height", &height) || width <= 0.0 || height <= 0.0) {
        return;
    }

    const bool hasRx = parse_length_attribute(tag, "rx", &rx);
    const bool hasRy = parse_length_attribute(tag, "ry", &ry);
    if (hasRx && !hasRy) {
        ry = rx;
    } else if (!hasRx && hasRy) {
        rx = ry;
    }
    rx = clamp_double(rx, 0.0, width * 0.5);
    ry = clamp_double(ry, 0.0, height * 0.5);

    PathDraw path(context, style, true, true);
    if (rx <= 0.0 || ry <= 0.0) {
        path.move_to({ x, y });
        path.line_to({ x + width, y });
        path.line_to({ x + width, y + height });
        path.line_to({ x, y + height });
        path.close_path();
        path.finish();
        return;
    }

    path.move_to({ x + rx, y });
    path.line_to({ x + width - rx, y });
    add_ellipse_arc(path, x + width - rx, y + ry, rx, ry, -kPi * 0.5, 0.0, 8);
    path.line_to({ x + width, y + height - ry });
    add_ellipse_arc(path, x + width - rx, y + height - ry, rx, ry, 0.0, kPi * 0.5, 8);
    path.line_to({ x + rx, y + height });
    add_ellipse_arc(path, x + rx, y + height - ry, rx, ry, kPi * 0.5, kPi, 8);
    path.line_to({ x, y + ry });
    add_ellipse_arc(path, x + rx, y + ry, rx, ry, kPi, kPi * 1.5, 8);
    path.close_path();
    path.finish();
}

void draw_ellipse(RenderContext& context, const Style& style, double cx, double cy, double rx, double ry) {
    if (!style.visible || rx <= 0.0 || ry <= 0.0) {
        return;
    }
    PathDraw path(context, style, true, true);
    path.move_to({ cx + rx, cy });
    add_ellipse_arc(path, cx, cy, rx, ry, 0.0, kPi * 2.0, 48);
    path.close_path();
    path.finish();
}

void draw_circle_tag(RenderContext& context, const Style& style, const Tag& tag) {
    double cx = 0.0;
    double cy = 0.0;
    double r = 0.0;
    parse_length_attribute(tag, "cx", &cx);
    parse_length_attribute(tag, "cy", &cy);
    if (parse_length_attribute(tag, "r", &r)) {
        draw_ellipse(context, style, cx, cy, r, r);
    }
}

void draw_ellipse_tag(RenderContext& context, const Style& style, const Tag& tag) {
    double cx = 0.0;
    double cy = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    parse_length_attribute(tag, "cx", &cx);
    parse_length_attribute(tag, "cy", &cy);
    if (parse_length_attribute(tag, "rx", &rx) && parse_length_attribute(tag, "ry", &ry)) {
        draw_ellipse(context, style, cx, cy, rx, ry);
    }
}

void draw_line_tag(RenderContext& context, const Style& style, const Tag& tag) {
    if (!style.visible) {
        return;
    }
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    parse_length_attribute(tag, "x1", &x1);
    parse_length_attribute(tag, "y1", &y1);
    parse_length_attribute(tag, "x2", &x2);
    parse_length_attribute(tag, "y2", &y2);
    PathDraw path(context, style, false, true);
    path.move_to({ x1, y1 });
    path.line_to({ x2, y2 });
    path.finish();
}

bool parse_points(Span span, std::vector<Point>* points) {
    NumberParser parser(span);
    while (parser.has_number()) {
        double x = 0.0;
        double y = 0.0;
        if (!parser.parse(&x) || !parser.parse(&y)) {
            return false;
        }
        points->push_back({ x, y });
    }
    return !points->empty();
}

void draw_poly_tag(RenderContext& context, const Style& style, const Tag& tag, bool closed) {
    if (!style.visible) {
        return;
    }
    const Span pointSpan = find_attribute(tag, "points");
    if (!pointSpan.data) {
        return;
    }

    std::vector<Point> points;
    if (!parse_points(pointSpan, &points)) {
        return;
    }

    PathDraw path(context, style, closed, true);
    path.move_to(points[0]);
    for (std::size_t i = 1; i < points.size(); ++i) {
        path.line_to(points[i]);
    }
    if (closed) {
        path.close_path();
    }
    path.finish();
}

Point reflect_point(Point point, Point around) noexcept {
    return { around.x * 2.0 - point.x, around.y * 2.0 - point.y };
}

bool parse_path_data(RenderContext& context, const Style& style, Span data) {
    if (!style.visible || data.length == 0) {
        return true;
    }

    NumberParser parser(data);
    PathDraw path(context, style, true, true);
    char command = 0;
    char previousCommand = 0;
    Point lastCubicControl = { 0.0, 0.0 };
    Point lastQuadControl = { 0.0, 0.0 };

    while (parser.position() < data.length) {
        parser.skip_separators();
        if (parser.position() >= data.length) {
            break;
        }

        const char c = data.data[parser.position()];
        if (is_alpha(c)) {
            command = c;
            parser.set_position(parser.position() + 1);
        } else if (command == 0) {
            return false;
        }

        const bool relative = command >= 'a' && command <= 'z';
        const char op = to_lower_ascii(command);

        if (op == 'z') {
            path.close_path();
            previousCommand = command;
            command = 0;
            continue;
        }

        bool firstMove = true;
        while (parser.has_number()) {
            Point current = path.has_current() ? path.current() : Point { 0.0, 0.0 };
            if (op == 'm') {
                double x = 0.0;
                double y = 0.0;
                if (!parser.parse(&x) || !parser.parse(&y)) {
                    return false;
                }
                Point point = { relative ? current.x + x : x, relative ? current.y + y : y };
                if (firstMove) {
                    path.move_to(point);
                    firstMove = false;
                } else {
                    path.line_to(point);
                }
                previousCommand = command;
            } else if (op == 'l') {
                double x = 0.0;
                double y = 0.0;
                if (!parser.parse(&x) || !parser.parse(&y)) {
                    return false;
                }
                path.line_to({ relative ? current.x + x : x, relative ? current.y + y : y });
                previousCommand = command;
            } else if (op == 'h') {
                double x = 0.0;
                if (!parser.parse(&x)) {
                    return false;
                }
                path.line_to({ relative ? current.x + x : x, current.y });
                previousCommand = command;
            } else if (op == 'v') {
                double y = 0.0;
                if (!parser.parse(&y)) {
                    return false;
                }
                path.line_to({ current.x, relative ? current.y + y : y });
                previousCommand = command;
            } else if (op == 'c') {
                double x1 = 0.0;
                double y1 = 0.0;
                double x2 = 0.0;
                double y2 = 0.0;
                double x = 0.0;
                double y = 0.0;
                if (!parser.parse(&x1) || !parser.parse(&y1) || !parser.parse(&x2) || !parser.parse(&y2) || !parser.parse(&x) || !parser.parse(&y)) {
                    return false;
                }
                Point c1 = { relative ? current.x + x1 : x1, relative ? current.y + y1 : y1 };
                Point c2 = { relative ? current.x + x2 : x2, relative ? current.y + y2 : y2 };
                Point end = { relative ? current.x + x : x, relative ? current.y + y : y };
                path.cubic_to(c1, c2, end);
                lastCubicControl = c2;
                previousCommand = command;
            } else if (op == 's') {
                double x2 = 0.0;
                double y2 = 0.0;
                double x = 0.0;
                double y = 0.0;
                if (!parser.parse(&x2) || !parser.parse(&y2) || !parser.parse(&x) || !parser.parse(&y)) {
                    return false;
                }
                const char previousOp = to_lower_ascii(previousCommand);
                Point c1 = (previousOp == 'c' || previousOp == 's') ? reflect_point(lastCubicControl, current) : current;
                Point c2 = { relative ? current.x + x2 : x2, relative ? current.y + y2 : y2 };
                Point end = { relative ? current.x + x : x, relative ? current.y + y : y };
                path.cubic_to(c1, c2, end);
                lastCubicControl = c2;
                previousCommand = command;
            } else if (op == 'q') {
                double x1 = 0.0;
                double y1 = 0.0;
                double x = 0.0;
                double y = 0.0;
                if (!parser.parse(&x1) || !parser.parse(&y1) || !parser.parse(&x) || !parser.parse(&y)) {
                    return false;
                }
                Point c1 = { relative ? current.x + x1 : x1, relative ? current.y + y1 : y1 };
                Point end = { relative ? current.x + x : x, relative ? current.y + y : y };
                path.quad_to(c1, end);
                lastQuadControl = c1;
                previousCommand = command;
            } else if (op == 't') {
                double x = 0.0;
                double y = 0.0;
                if (!parser.parse(&x) || !parser.parse(&y)) {
                    return false;
                }
                const char previousOp = to_lower_ascii(previousCommand);
                Point c1 = (previousOp == 'q' || previousOp == 't') ? reflect_point(lastQuadControl, current) : current;
                Point end = { relative ? current.x + x : x, relative ? current.y + y : y };
                path.quad_to(c1, end);
                lastQuadControl = c1;
                previousCommand = command;
            } else if (op == 'a') {
                double rx = 0.0;
                double ry = 0.0;
                double rotation = 0.0;
                double largeArc = 0.0;
                double sweep = 0.0;
                double x = 0.0;
                double y = 0.0;
                if (!parser.parse(&rx) || !parser.parse(&ry) || !parser.parse(&rotation) || !parser.parse(&largeArc) || !parser.parse(&sweep) || !parser.parse(&x) || !parser.parse(&y)) {
                    return false;
                }
                (void)rx;
                (void)ry;
                (void)rotation;
                (void)largeArc;
                (void)sweep;
                path.line_to({ relative ? current.x + x : x, relative ? current.y + y : y });
                previousCommand = command;
            } else {
                return false;
            }

            parser.skip_separators();
            if (parser.position() < data.length && is_alpha(data.data[parser.position()])) {
                break;
            }
        }

        if (op == 'm') {
            command = relative ? 'l' : 'L';
        }
    }

    path.finish();
    return true;
}

void draw_path_tag(RenderContext& context, const Style& style, const Tag& tag) {
    const Span data = find_attribute(tag, "d");
    if (data.data) {
        parse_path_data(context, style, data);
    }
}

Viewport discover_viewport(const char* svg, std::size_t length) noexcept {
    Viewport viewport = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false };
    std::size_t position = 0;
    while (position < length) {
        while (position < length && svg[position] != '<') {
            ++position;
        }
        if (position >= length) {
            break;
        }
        Tag tag = {};
        if (!parse_tag(svg, length, &position, &tag)) {
            break;
        }
        if (!tag.valid || tag.closing) {
            continue;
        }
        if (!span_equals(tag.name, "svg")) {
            continue;
        }

        viewport.found = true;
        parse_length_attribute(tag, "width", &viewport.width);
        parse_length_attribute(tag, "height", &viewport.height);
        const Span viewBox = find_attribute(tag, "viewBox");
        if (viewBox.data) {
            parse_view_box(viewBox, &viewport.viewX, &viewport.viewY, &viewport.viewWidth, &viewport.viewHeight);
        }
        if (viewport.viewWidth <= 0.0) {
            viewport.viewWidth = viewport.width;
        }
        if (viewport.viewHeight <= 0.0) {
            viewport.viewHeight = viewport.height;
        }
        if (viewport.width <= 0.0) {
            viewport.width = viewport.viewWidth;
        }
        if (viewport.height <= 0.0) {
            viewport.height = viewport.viewHeight;
        }
        return viewport;
    }
    return viewport;
}

void process_shape(RenderContext& context, const Style& style, const Tag& tag) {
    if (span_equals(tag.name, "rect")) {
        draw_rect(context, style, tag);
    } else if (span_equals(tag.name, "circle")) {
        draw_circle_tag(context, style, tag);
    } else if (span_equals(tag.name, "ellipse")) {
        draw_ellipse_tag(context, style, tag);
    } else if (span_equals(tag.name, "line")) {
        draw_line_tag(context, style, tag);
    } else if (span_equals(tag.name, "polyline")) {
        draw_poly_tag(context, style, tag, false);
    } else if (span_equals(tag.name, "polygon")) {
        draw_poly_tag(context, style, tag, true);
    } else if (span_equals(tag.name, "path")) {
        draw_path_tag(context, style, tag);
    }
}

bool render_svg_document(const char* svg, std::size_t length, const std::SvgRenderer::PixelBuffer& target,
                         const std::SvgRenderer::RenderOptions& options) {
    RenderContext context(target);
    if (!context.valid() || !svg || length == 0) {
        return false;
    }

    const Viewport viewport = discover_viewport(svg, length);
    if (!viewport.found) {
        return false;
    }

    const int destX = options.x;
    const int destY = options.y;
    const int destWidth = options.width > 0 ? options.width : (viewport.width > 0.0 ? static_cast<int>(viewport.width + 0.5) : target.width - destX);
    const int destHeight = options.height > 0 ? options.height : (viewport.height > 0.0 ? static_cast<int>(viewport.height + 0.5) : target.height - destY);
    if (destWidth <= 0 || destHeight <= 0 || viewport.viewWidth <= 0.0 || viewport.viewHeight <= 0.0) {
        return false;
    }

    if (options.clear) {
        context.clear_rect(destX, destY, destWidth, destHeight, options.clearColor);
    }

    Style base = {};
    base.transform = multiply_matrix(
        multiply_matrix(translate_matrix(static_cast<double>(destX), static_cast<double>(destY)),
                        scale_matrix(static_cast<double>(destWidth) / viewport.viewWidth, static_cast<double>(destHeight) / viewport.viewHeight)),
        translate_matrix(-viewport.viewX, -viewport.viewY)
    );
    base.fill = { true, 0x000000, 1.0 };
    base.stroke = { false, 0x000000, 0.0 };
    base.opacity = 1.0;
    base.fillOpacity = 1.0;
    base.strokeOpacity = 1.0;
    base.strokeWidth = 1.0;
    base.fillRule = FillRule::NonZero;
    base.visible = true;

    std::vector<Style> stack;
    stack.push_back(base);

    std::size_t position = 0;
    while (position < length) {
        while (position < length && svg[position] != '<') {
            ++position;
        }
        if (position >= length) {
            break;
        }

        Tag tag = {};
        if (!parse_tag(svg, length, &position, &tag)) {
            break;
        }
        if (!tag.valid) {
            continue;
        }

        if (tag.closing) {
            if (stack.size() > 1) {
                stack.pop_back();
            }
            continue;
        }

        Style style = style_from_tag(stack.back(), tag);
        process_shape(context, style, tag);
        if (!tag.selfClosing) {
            stack.push_back(style);
        }
    }

    return true;
}

}

namespace std {

SvgRenderer::RenderOptions SvgRenderer::default_options() noexcept {
    return { 0, 0, 0, 0, false, 0x000000 };
}

bool SvgRenderer::render(const char* svg, const PixelBuffer& target) {
    if (!svg) {
        return false;
    }
    return render(svg, std::strlen(svg), target, default_options());
}

bool SvgRenderer::render(const char* svg, const PixelBuffer& target, const RenderOptions& options) {
    if (!svg) {
        return false;
    }
    return render(svg, std::strlen(svg), target, options);
}

bool SvgRenderer::render(const char* svg, std::size_t length, const PixelBuffer& target) {
    return render(svg, length, target, default_options());
}

bool SvgRenderer::render(const char* svg, std::size_t length, const PixelBuffer& target, const RenderOptions& options) {
    return render_svg_document(svg, length, target, options);
}

}
