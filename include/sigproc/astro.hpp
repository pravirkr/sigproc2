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
 * @brief Convert an RA string to radians.
 *
 * Accepts `[+]hh:mm:ss.sss` with optional surrounding whitespace. Minutes and
 * seconds must be < 60; hours must be < 24. Negative values are rejected.
 *
 * @param ra_string RA in 'hh:mm:ss.ssss' format
 * @return RA in radians, in [0, 2*pi)
 * @throw std::invalid_argument if the format is invalid or the value is out of range
 */
 double ra_to_rad(std::string_view ra_string);

/**
 * @brief Convert a DEC string to radians.
 *
 * Accepts `[+|-]dd:mm:ss.sss` with optional surrounding whitespace. Minutes and
 * seconds must be < 60; the absolute value must be <= 90 degrees. A leading '-'
 * is honoured even when the degree field is zero (e.g. "-00:30:00").
 *
 * @param dec_string DEC in 'dd:mm:ss.ssss' format
 * @return DEC in radians, in [-pi/2, +pi/2]
 * @throw std::invalid_argument if the format is invalid or the value is out of range
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
