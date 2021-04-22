#pragma once

#include <string>
#include <map>
#include <cmath>

#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

namespace angles {

/**
 * @brief Convert RADEC float to a string.
 *
 * @param angle          RADEC float (eg. 124532.123)
 * @return std::string  'dd:mm:ss.ssss' format string
 */
std::string radec_to_str(double angle) {
    int dd, mm, sign;
    double ss;
    sign  = angle < 0.0 ? -1 : 1;
    angle = std::abs(angle);
    dd    = (int)(angle / 10000.0);
    mm    = (int)(angle / 100.0) - dd * 100;
    ss    = angle - mm * 100 - dd * 10000;

    std::string str = fmt::format("{:02d}:{:02d}:{:07.4f}", sign * dd, mm, ss);
    return str;
}

double hms_to_rad(int hour, int min_, double sec) {
    int sign;
    double radian;
    const double pi = boost::math::constants::pi<double>();

    sign   = hour < 0 ? -1 : 1;
    radian = 60.0 * (60.0 * std::abs(hour) + std::abs(min_)) + std::abs(sec);
    radian *= sign * pi / 12 / 60.0 / 60.0;
    return radian;
}

double dms_to_rad(int deg, int min_, double sec) {
    int sign;
    double radian;
    const double pi = boost::math::constants::pi<double>();

    sign = deg < 0 ? -1 : 1;
    if (deg == 0.0 and (min_ < 0.0 or sec < 0.0)) {
        sign = -1;
    }
    radian = 60.0 * (60.0 * std::abs(deg) + std::abs(min_)) + std::abs(sec);
    radian *= sign * pi / 180 / 60.0 / 60.0;
    return radian;
}

double ra_to_rad(std::string ra_string) {
    std::vector<std::string> result(3);
    boost::split(result, ra_string, boost::is_any_of(":"));
    return hms_to_rad(std::stoi(result[0]), std::stoi(result[1]),
                      std::stod(result[2]));
}

double dec_to_rad(std::string dec_string) {
    std::vector<std::string> result(3);
    boost::split(result, dec_string, boost::is_any_of(":"));
    if (std::stoi(result[0]) == 0 and result[0].rfind("-", 0) == 0) {
        result[1].insert(0, "-");
        result[2].insert(0, "-");
    }
    return dms_to_rad(std::stoi(result[0]), std::stoi(result[1]),
                      std::stod(result[2]));
}

double deg_to_dms(double angle) {
    int deg, min, sign;
    double sec;

    sign  = angle < 0.0 ? -1 : 1;
    angle = std::abs(angle);
    deg   = (int)angle;
    angle = (angle - deg) * 60.0;
    min   = (int)angle;
    sec   = (angle - min) * 60.0;
    return (sign * ((double)deg * 10000.0 + (double)min * 100.0 + sec));
}

}  // namespace angles

namespace dates {

std::string MJD_to_Gregorian(double mjd) {
    // Convert the Modified Julian Date to the gregorian date
    // double fraction = mjd - ((long)mjd);
    auto gregorian_date
        = boost::gregorian::gregorian_calendar::from_modjulian_day_number(mjd);
    std::string ymd = boost::gregorian::to_iso_extended_string(
        boost::gregorian::date(gregorian_date));
    return ymd;
}

/* return UT hours minutes and seconds given an MJD */
void uttime(double mjd, int* hh, int* mm, float* ss) {
    mjd = (mjd - floor(mjd)) * 24.0; /* get UT in hours */
    *hh = (int)mjd;
    mjd = (mjd - floor(mjd)) * 60.0; /* get remainder in minutes */
    *mm = (int)mjd;
    mjd = (mjd - floor(mjd)) * 60.0; /* get remainder in seconds */
    *ss = (float)mjd;
}

struct Duration_print {
    std::string unit;
    double duration;
};

Duration_print get_duration_print(double duration) {
    std::string unit = "seconds";
    if (duration > 60.0) {
        duration /= 60.0;
        unit = "minutes";
        if (duration > 60.0) {
            duration /= 60.0;
            unit = "hours";
            if (duration > 24.0) {
                duration /= 24.0;
                unit = "days";
            }
        }
    }
    Duration_print result = {unit, duration};
    return result;
}

}  // namespace dates
