#include <cmath>
#include <cstdio>
#include <format>
#include <numbers>
#include <tuple>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/spirit/home/x3.hpp>

#include <sigproc/angles.hpp>

namespace angles {

std::string radec_to_str(double angle) {
    const int sign  = angle < 0 ? -1 : 1;
    angle           = std::abs(angle);
    const int dd    = static_cast<int>(angle) / 10000;
    const int mm    = static_cast<int>(angle) / 100 % 100;
    const double ss = angle - mm * 100 - dd * 10000;
    return std::format("{:02d}:{:02d}:{:07.4f}", sign * dd, mm, ss);
}

double hms_to_rad(int hour, int minutes, double sec) {
    const double sign = hour < 0 ? -1.0 : 1.0;
    const double total_seconds =
        std::abs(hour) * 3600.0 + std::abs(minutes) * 60.0 + std::abs(sec);
    return sign * total_seconds * std::numbers::pi_v<double> / 43200.0;
}

double dms_to_rad(int deg, int minutes, double sec) {
    const double sign =
        (deg < 0 || (deg == 0 && (minutes < 0 || sec < 0))) ? -1.0 : 1.0;
    const double total_seconds =
        std::abs(deg) * 3600.0 + std::abs(minutes) * 60.0 + std::abs(sec);
    return sign * total_seconds * std::numbers::pi_v<double> / 648000.0;
}

double ra_to_rad(const std::string_view& ra_string) {
    std::tuple<int, int, double> result;
    namespace x3   = boost::spirit::x3;
    auto parser    = x3::int_ >> ':' >> x3::int_ >> ':' >> x3::double_;
    const auto* it = ra_string.begin();
    bool success   = x3::parse(it, ra_string.end(), parser, result);
    if (!success || it != ra_string.end()) {
        throw std::invalid_argument("Invalid right ascension format");
    }
    auto [hour, minutes, sec] = result;
    return hms_to_rad(hour, minutes, sec);
}

double dec_to_rad(const std::string_view& dec_string) {
    std::tuple<char, int, int, double> result;
    namespace x3 = boost::spirit::x3;
    auto parser =
        x3::char_("-+") >> x3::int_ >> ':' >> x3::int_ >> ':' >> x3::double_;
    const auto* it = dec_string.begin();
    bool success   = x3::parse(it, dec_string.end(), parser, result);
    if (!success || it != dec_string.end()) {
        throw std::invalid_argument("Invalid declination format");
    }
    auto [sign, deg, minutes, sec] = result;
    int sign_multiplier            = sign == '-' ? -1 : 1;
    return dms_to_rad(sign_multiplier * deg, minutes, sec);
}

double deg_to_dms(double angle) {
    const int sign = angle < 0.0 ? -1 : 1;
    angle          = std::abs(angle);
    auto deg       = static_cast<int>(angle);
    angle          = (angle - deg) * 60.0;
    auto min       = static_cast<int>(angle);
    double sec     = (angle - min) * 60.0;
    return (sign * (static_cast<double>(deg) * 10000.0 +
                    static_cast<double>(min) * 100.0 + sec));
}

} // namespace angles

namespace dates {

std::string mjd_to_gregorian(int mjd) {
    // Convert the Modified Julian Date to the gregorian date
    const auto gregorian_date =
        boost::gregorian::gregorian_calendar::from_modjulian_day_number(mjd);
    return boost::gregorian::to_iso_extended_string(
        boost::gregorian::date(gregorian_date));
}

std::string get_duration_string(double duration) {
    const std::array units{"seconds", "minutes", "hours", "days"};
    const std::array factors{60.0, 60.0, 24.0};

    size_t index = 0;
    for (double factor : factors) {
        if (duration <= factor) {
            break;
        }
        duration /= factor;
        ++index;
    }

    return std::format("{:.1f} {}", duration, units.at(index));
}

} // namespace dates
