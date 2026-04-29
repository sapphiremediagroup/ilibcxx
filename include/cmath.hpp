#pragma once

#include <cstddef.hpp>

namespace std {
inline double fabs(double value) noexcept {
    return value < 0.0 ? -value : value;
}

inline double floor(double value) noexcept {
    const long long truncated = static_cast<long long>(value);
    if (value < 0.0 && static_cast<double>(truncated) != value) {
        return static_cast<double>(truncated - 1);
    }
    return static_cast<double>(truncated);
}

inline double ceil(double value) noexcept {
    const long long truncated = static_cast<long long>(value);
    if (value > 0.0 && static_cast<double>(truncated) != value) {
        return static_cast<double>(truncated + 1);
    }
    return static_cast<double>(truncated);
}

inline double sqrt(double value) noexcept {
    if (value <= 0.0) {
        return value == 0.0 ? 0.0 : 0.0 / 0.0;
    }

    double result = value;
    for (int i = 0; i < 16; ++i) {
        result = 0.5 * (result + value / result);
    }
    return result;
}

inline double fmod(double value, double modulus) noexcept {
    if (modulus == 0.0) {
        return 0.0 / 0.0;
    }
    double quotient = value / modulus;
    double truncated = floor(quotient);
    return value - truncated * modulus;
}

inline double sin(double value) noexcept {
    constexpr double pi = 3.14159265358979323846;
    constexpr double two_pi = 2.0 * pi;
    value = fmod(value, two_pi);
    if (value < -pi) {
        value += two_pi;
    } else if (value > pi) {
        value -= two_pi;
    }

    double sign = 1.0;
    if (value < 0.0) {
        sign = -1.0;
        value = -value;
    }

    if (value > pi / 2.0) {
        value = pi - value;
    }

    const double x2 = value * value;
    double result = value;
    result -= (x2 * value) / 6.0;
    result += (x2 * x2 * value) / 120.0;
    result -= (x2 * x2 * x2 * value) / 5040.0;
    result += (x2 * x2 * x2 * x2 * value) / 362880.0;
    return sign * result;
}

inline double cos(double value) noexcept {
    constexpr double pi = 3.14159265358979323846;
    constexpr double two_pi = 2.0 * pi;
    value = fmod(value, two_pi);
    if (value < -pi) {
        value += two_pi;
    } else if (value > pi) {
        value -= two_pi;
    }

    double sign = 1.0;
    if (value < 0.0) {
        value = -value;
    }
    if (value > pi / 2.0) {
        value = pi - value;
        sign = -1.0;
    }

    const double x2 = value * value;
    double result = 1.0;
    result -= x2 / 2.0;
    result += (x2 * x2) / 24.0;
    result -= (x2 * x2 * x2) / 720.0;
    result += (x2 * x2 * x2 * x2) / 40320.0;
    return sign * result;
}

inline double tan(double value) noexcept {
    const double cosine = cos(value);
    if (cosine == 0.0) {
        return 0.0 / 0.0;
    }
    return sin(value) / cosine;
}
}
