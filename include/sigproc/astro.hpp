#pragma once

#include <string>
#include <string_view>

namespace sigproc::astro {

/**
 * @brief Convert RADEC float to a string.
 *
 * @param angle RADEC float (eg. 124532.123)
 * @return std::string 'dd:mm:ss.ssss' format string
 */
std::string radec_to_str(double angle) noexcept;

/**
 * @brief Convert hours, minutes, seconds to radians.
 */
double hms_to_rad(int hour, int minutes, double sec) noexcept;

/**
 * @brief Convert degrees, minutes, seconds to radians.
 */
double dms_to_rad(int deg, int minutes, double sec) noexcept;

/**
 * @brief Convert decimal degrees to DMS format as a float.
 */
double deg_to_dms(double angle) noexcept;

/**
 * @brief Convert RA string to radians.
 *
 * @param ra_string RA string in 'hh:mm:ss.ssss' format
 * @return double RA in radians
 * @throw std::invalid_argument if format is invalid
 */
double ra_to_rad(std::string_view ra_string);

/**
 * @brief Convert DEC string to radians.
 *
 * @param dec_string DEC string in 'dd:mm:ss.ssss' format
 * @return double DEC in radians
 * @throw std::invalid_argument if format is invalid
 */
double dec_to_rad(std::string_view dec_string);

/**
 * @brief Convert Modified Julian Date to Gregorian calendar string.
 * @param mjd Modified Julian Date (integer)
 * @return std::string ISO 8601 extended format (YYYY-MM-DD)
 *
 * Fliegel & van Flandern (FVF) algorithm. Code adapted from boost::gregorian.
 */
std::string mjd_to_gregorian(int mjd);

/**
 * @brief Convert duration in seconds to human-readable string.
 * @param duration Duration in seconds
 * @return std::string Formatted duration with appropriate units
 */
std::string get_duration_string(double duration);

} // namespace sigproc::astro
