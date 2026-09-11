#include <sigproc/astro.hpp>

#include <array>
#include <cctype>
#include <cmath>
#include <format>
#include <numbers>
#include <stdexcept>

#include <sigproc/common/types.hpp>

#include "sigproc/parsing.hpp"

namespace sigproc::astro {

std::string radec_to_str(double angle) noexcept {
    const int sign  = angle < 0 ? -1 : 1;
    angle           = std::abs(angle);
    const int dd    = static_cast<int>(angle) / 10000;
    const int mm    = static_cast<int>(angle) / 100 % 100;
    const double ss = angle - (mm * 100.0) - (dd * 10000.0);
    return std::format("{:02d}:{:02d}:{:07.4f}", sign * dd, mm, ss);
}

double hms_to_rad(int hour, int minutes, double sec) noexcept {
    const double sign = hour < 0 ? -1.0 : 1.0;
    const double total_seconds =
        (std::abs(hour) * 3600.0) + (std::abs(minutes) * 60.0) + std::abs(sec);
    // 12 hours = π radians (for RA)
    return sign * total_seconds * std::numbers::pi_v<double> / 43200.0;
}

double dms_to_rad(int deg, int minutes, double sec) noexcept {
    const double sign =
        (deg < 0 || (deg == 0 && (minutes < 0 || sec < 0))) ? -1.0 : 1.0;
    const double total_seconds =
        (std::abs(deg) * 3600.0) + (std::abs(minutes) * 60.0) + std::abs(sec);
    // 180 degrees = π radians (for DEC)
    return sign * total_seconds * std::numbers::pi_v<double> / 648000.0;
}

double deg_to_dms(double angle) noexcept {
    const int sign   = angle < 0.0 ? -1 : 1;
    angle            = std::abs(angle);
    const auto deg   = static_cast<int>(angle);
    angle            = (angle - deg) * 60.0;
    const auto min   = static_cast<int>(angle);
    const double sec = (angle - min) * 60.0;
    return (sign * ((static_cast<double>(deg) * 10000.0) +
                    (static_cast<double>(min) * 100.0) + sec));
}

double ra_to_rad(std::string_view ra_string) {
    const parsing::Sexagesimal v =
        parsing::parse_sexagesimal(ra_string, "right ascension");
    if (v.negative || v.major >= 24) {
        throw std::invalid_argument(
            std::format("Right ascension out of range: '{}' (expected 00:00:00 "
                        "to 23:59:59.9...)",
                        ra_string));
    }
    return hms_to_rad(static_cast<int>(v.major), static_cast<int>(v.minutes),
                      v.seconds);
}

double dec_to_rad(std::string_view dec_string) {
    const parsing::Sexagesimal v =
        parsing::parse_sexagesimal(dec_string, "declination");
    if (v.major > 90 ||
        (v.major == 90 && (v.minutes != 0 || v.seconds != 0.0))) {
        throw std::invalid_argument(std::format(
            "Declination out of range: '{}' (expected -90:00:00 to +90:00:00)",
            dec_string));
    }
    // Sign is applied to the finished value, never to the degree magnitude.
    // This is what makes "-00:30:00" correct.
    const double magnitude = dms_to_rad(static_cast<int>(v.major),
                                        static_cast<int>(v.minutes), v.seconds);
    return v.negative ? -magnitude : magnitude;
}

std::string mjd_to_gregorian(int mjd) {
    // Convert MJD to Julian Day Number
    // MJD = JD - 2400000.5
    const int jd    = mjd + 2400001; // Adding 2400000 + 1 (for .5 day offset)
    const int a     = jd + 32044;
    const int b     = ((4 * a) + 3) / 146097;
    const int c     = a - ((146097 * b) / 4);
    const int d     = ((4 * c) + 3) / 1461;
    const int e     = c - ((1461 * d) / 4);
    const int m     = ((5 * e) + 2) / 153;
    const int day   = e - (((153 * m) + 2) / 5) + 1;
    const int month = m + 3 - (12 * (m / 10));
    const int year  = (100 * b) + d - 4800 + (m / 10);
    return std::format("{:04d}-{:02d}-{:02d}", year, month, day);
}

std::string get_duration_string(double duration) {
    constexpr std::array kUnits{"seconds", "minutes", "hours", "days"};
    constexpr std::array kFactors{60.0, 60.0, 24.0};

    SizeType index = 0;
    for (double factor : kFactors) {
        if (duration < factor) {
            break;
        }
        duration /= factor;
        ++index;
    }

    return std::format("{:.1f} {}", duration, kUnits[index]);
}

} // namespace sigproc::astro
