#pragma once

#include <string>
#include <string_view>

namespace angles {

/**
 * @brief Convert RADEC float to a string.
 *
 * @param angle          RADEC float (eg. 124532.123)
 * @return std::string  'dd:mm:ss.ssss' format string
 */
std::string radec_to_str(double angle);

double hms_to_rad(int hour, int minutes, double sec);

double dms_to_rad(int deg, int minutes, double sec);

/**
 * @brief Convert RA string to radians.
 *
 * @param ra_string RA string in 'hh:mm:ss.ssss' format
 * @return double RA in radians
 */
double ra_to_rad(const std::string_view& ra_string);

/**
 * @brief Convert DEC string to radians.
 *
 * @param dec_string DEC string in 'dd:mm:ss.ssss' format
 * @return double DEC in radians
 */
double dec_to_rad(const std::string_view& dec_string);

double deg_to_dms(double angle);

} // namespace angles

namespace dates {

std::string mjd_to_gregorian(int mjd);

std::string get_duration_string(double duration);

} // namespace dates
