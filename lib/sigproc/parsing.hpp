#include <charconv>     // std::from_chars, std::chars_format
#include <format>       // std::format
#include <stdexcept>    // std::invalid_argument
#include <string_view>

namespace sigproc::parsing {

inline constexpr std::string_view k_whitespace = " \t\n\v\f\r";

/// Strip leading and trailing whitespace. Returns an empty view for all-blank.
[[nodiscard]] constexpr std::string_view trim(std::string_view s) noexcept {
    const auto first = s.find_first_not_of(k_whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(k_whitespace);
    return s.substr(first, last - first + 1);
}

[[nodiscard]] constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

/// True for `digits+ ( '.' digits* )?` and nothing else.
///
/// This is the gate that keeps "inf", "nan", "1e3", "0x1p3", "+5", "-5" and
/// ".5" out of the seconds field. std::from_chars alone would accept several
/// of those.
[[nodiscard]] constexpr bool is_plain_decimal(std::string_view field) noexcept {
    if (field.empty() || !is_digit(field.front())) {
        return false;
    }
    bool seen_dot = false;
    for (const char c : field) {
        if (c == '.') {
            if (seen_dot) {
                return false;
            }
            seen_dot = true;
        } else if (!is_digit(c)) {
            return false;
        }
    }
    return true;
}

/// Sign-and-magnitude representation of a `[+|-]DD:MM:SS.sss` value.
/// `major` is hours for RA, degrees for DEC. Always non-negative.
struct Sexagesimal {
    bool     negative = false;
    unsigned major    = 0;
    unsigned minutes  = 0;
    double   seconds  = 0.0;
};

/// Parse `[+|-]DD:MM:SS.sss`, surrounding whitespace allowed, nothing else.
/// `what` names the quantity for the exception message.
/// @throw std::invalid_argument on any malformed input.
[[nodiscard]] Sexagesimal parse_sexagesimal(std::string_view input,
                                            std::string_view what) {
    const auto invalid = [&] {
        return std::invalid_argument(std::format(
            "Invalid {} format: '{}' (expected [+|-]dd:mm:ss.sss)", what, input));
    };

    std::string_view s = trim(input);
    if (s.empty()) {
        throw invalid();
    }

    Sexagesimal out;
    if (s.front() == '+' || s.front() == '-') {
        out.negative = (s.front() == '-');
        s.remove_prefix(1);
    }

    const char* const end = s.data() + s.size();

    // Magnitude. Unsigned target => a stray sign here is a parse error, and
    // there is no signed-overflow UB to worry about.
    const auto [p1, e1] = std::from_chars(s.data(), end, out.major);
    if (e1 != std::errc{} || p1 == end || *p1 != ':') {
        throw invalid();
    }

    const auto [p2, e2] = std::from_chars(p1 + 1, end, out.minutes);
    if (e2 != std::errc{} || p2 == end || *p2 != ':') {
        throw invalid();
    }

    const std::string_view sec_field(p2 + 1, end);
    if (!is_plain_decimal(sec_field)) {
        throw invalid();
    }
    const auto [p3, e3] = std::from_chars(sec_field.data(), end, out.seconds,
                                          std::chars_format::fixed);
    // p3 != end catches any trailing garbage; e3 catches out-of-range values.
    if (e3 != std::errc{} || p3 != end) {
        throw invalid();
    }

    if (out.minutes >= 60 || out.seconds >= 60.0) {
        throw invalid();
    }
    return out;
}

}  // namespace sigproc::parsing
