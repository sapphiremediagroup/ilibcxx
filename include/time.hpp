#pragma once

#include <cstdint.hpp>
#include <syscall.hpp>

namespace std {

class Time {
public:
    struct DateTime {
        std::uint16_t year;
        std::uint8_t month;
        std::uint8_t day;
        std::uint8_t hour;
        std::uint8_t minute;
        std::uint8_t second;
    };

    struct TimeString {
        char value[8];
    };

    struct DateString {
        char value[11];
    };

    static std::uint64_t uptime_milliseconds() {
        return std::gettime();
    }

    static std::uint64_t unix_seconds() {
        return std::getunixtime();
    }

    static DateTime now() {
        return from_unix(unix_seconds());
    }

    static DateTime from_unix(std::uint64_t seconds) {
        DateTime out = {};

        std::uint64_t days = seconds / 86400ULL;
        std::uint32_t daySeconds = static_cast<std::uint32_t>(seconds % 86400ULL);
        out.hour = static_cast<std::uint8_t>(daySeconds / 3600U);
        daySeconds %= 3600U;
        out.minute = static_cast<std::uint8_t>(daySeconds / 60U);
        out.second = static_cast<std::uint8_t>(daySeconds % 60U);

        std::uint16_t year = 1970;
        while (true) {
            const std::uint16_t yearDays = is_leap_year(year) ? 366 : 365;
            if (days < yearDays) {
                break;
            }
            days -= yearDays;
            ++year;
        }

        std::uint8_t month = 1;
        while (month <= 12) {
            const std::uint8_t mdays = days_in_month(year, month);
            if (days < mdays) {
                break;
            }
            days -= mdays;
            ++month;
        }

        out.year = year;
        out.month = month;
        out.day = static_cast<std::uint8_t>(days + 1);
        return out;
    }

    static TimeString time_string() {
        return time_string(now());
    }

    static DateString date_string() {
        return date_string(now());
    }

    static TimeString time_string(const DateTime& value) {
        TimeString out = {};
        format_time(value, out.value, sizeof(out.value));
        return out;
    }

    static DateString date_string(const DateTime& value) {
        DateString out = {};
        format_date(value, out.value, sizeof(out.value));
        return out;
    }

    static bool format_time(const DateTime& value, char* buffer, std::uint64_t size) {
        if (!buffer || size < sizeof(TimeString::value)) {
            return false;
        }

        std::uint8_t hour12 = value.hour;

        std::uint64_t pos = 0;
        if (hour12 >= 10) {
            buffer[pos++] = static_cast<char>('0' + (hour12 / 10U));
        }
        buffer[pos++] = static_cast<char>('0' + (hour12 % 10U));
        buffer[pos++] = ':';
        buffer[pos++] = static_cast<char>('0' + (value.minute / 10U));
        buffer[pos++] = static_cast<char>('0' + (value.minute % 10U));
        buffer[pos] = '\0';
        return true;
    }

    static bool format_date(const DateTime& value, char* buffer, std::uint64_t size) {
        if (!buffer || size < sizeof(DateString::value)) {
            return false;
        }

        buffer[0] = static_cast<char>('0' + (value.day / 10U));
        buffer[1] = static_cast<char>('0' + (value.day % 10U));
        buffer[2] = '/';
        buffer[3] = static_cast<char>('0' + (value.month / 10U));
        buffer[4] = static_cast<char>('0' + (value.month % 10U));
        buffer[5] = '/';
        buffer[6] = static_cast<char>('0' + ((value.year / 1000U) % 10U));
        buffer[7] = static_cast<char>('0' + ((value.year / 100U) % 10U));
        buffer[8] = static_cast<char>('0' + ((value.year / 10U) % 10U));
        buffer[9] = static_cast<char>('0' + (value.year % 10U));
        buffer[10] = '\0';
        return true;
    }

private:
    static bool is_leap_year(std::uint16_t year) {
        return ((year % 4U) == 0 && (year % 100U) != 0) || ((year % 400U) == 0);
    }

    static std::uint8_t days_in_month(std::uint16_t year, std::uint8_t month) {
        static constexpr std::uint8_t days[] = {
            31, 28, 31, 30, 31, 30,
            31, 31, 30, 31, 30, 31
        };

        if (month == 2 && is_leap_year(year)) {
            return 29;
        }
        if (month < 1 || month > 12) {
            return 31;
        }
        return days[month - 1];
    }
};

}
