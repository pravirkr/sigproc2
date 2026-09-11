#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <sigproc/astro.hpp>

namespace sigproc::astro {

namespace {

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

inline constexpr double kPi  = std::numbers::pi;
inline constexpr double kTol = 1e-12;

/// Reference implementation, independent of the production helpers, so a bug
/// in hms_to_rad/dms_to_rad cannot cancel out against a bug in the test.
constexpr double hours_to_rad(double h, double m, double s) {
    return (h + m / 60.0 + s / 3600.0) * (kPi / 12.0);
}
constexpr double degrees_to_rad(double d, double m, double s) {
    return (d + m / 60.0 + s / 3600.0) * (kPi / 180.0);
}

} // namespace

// ---------------------------------------------------------------------------
//  RA: valid input
// ---------------------------------------------------------------------------
TEST_CASE("ra_to_rad parses valid right ascensions", "[astro][ra]") {
    SECTION("cardinal values") {
        CHECK_THAT(ra_to_rad("00:00:00"), WithinAbs(0.0, kTol));
        CHECK_THAT(ra_to_rad("06:00:00"), WithinAbs(kPi / 2.0, kTol));
        CHECK_THAT(ra_to_rad("12:00:00"), WithinAbs(kPi, kTol));
        CHECK_THAT(ra_to_rad("18:00:00"), WithinAbs(3.0 * kPi / 2.0, kTol));
    }

    SECTION("one hour is exactly 15 degrees") {
        CHECK_THAT(ra_to_rad("01:00:00"), WithinAbs(15.0 * kPi / 180.0, kTol));
    }

    SECTION("fractional seconds") {
        CHECK_THAT(ra_to_rad("01:02:03.456"),
                   WithinAbs(hours_to_rad(1, 2, 3.456), kTol));
        CHECK_THAT(ra_to_rad("12:34:56.789012"),
                   WithinAbs(hours_to_rad(12, 34, 56.789012), kTol));
    }

    SECTION("upper edge of the domain") {
        CHECK_THAT(ra_to_rad("23:59:59.999999"),
                   WithinAbs(hours_to_rad(23, 59, 59.999999), kTol));
        CHECK(ra_to_rad("23:59:59.999999") < 2.0 * kPi);
    }

    SECTION("explicit plus sign is accepted") {
        CHECK_THAT(ra_to_rad("+12:00:00"), WithinAbs(kPi, kTol));
    }

    SECTION(
        "surrounding whitespace is tolerated (CRLF-terminated catalogues)") {
        const auto input = GENERATE(as<std::string>{}, " 12:00:00", "12:00:00 ",
                                    "\t12:00:00\r\n", "   12:00:00   ");
        CAPTURE(input);
        CHECK_THAT(ra_to_rad(input), WithinAbs(kPi, kTol));
    }

    SECTION("seconds field may end in a bare decimal point") {
        CHECK_THAT(ra_to_rad("12:30:05."),
                   WithinAbs(hours_to_rad(12, 30, 5.0), kTol));
    }

    SECTION("leading zeros and extra-wide fields") {
        CHECK_THAT(ra_to_rad("0012:0030:0005.5"),
                   WithinAbs(hours_to_rad(12, 30, 5.5), kTol));
    }

    SECTION("monotonic in the seconds field") {
        CHECK(ra_to_rad("12:00:00") < ra_to_rad("12:00:00.001"));
        CHECK(ra_to_rad("12:00:59.999") < ra_to_rad("12:01:00"));
    }

    SECTION("string_view need not be null-terminated") {
        const std::string buf = "12:00:00garbage";
        const std::string_view view(buf.data(), 8);
        CHECK_THAT(ra_to_rad(view), WithinAbs(kPi, kTol));
    }
}

// ---------------------------------------------------------------------------
//  RA: rejected input
// ---------------------------------------------------------------------------
TEST_CASE("ra_to_rad rejects malformed input", "[astro][ra][error]") {
    const auto input =
        GENERATE(as<std::string_view>{},
                 "",    // empty
                 "   ", // whitespace only
                 "\t\r\n",
                 "12", // too few fields
                 "12:30",
                 "12:30:00:00", // too many fields
                 ":30:00",      // missing hour
                 "12::00",      // missing minute
                 "12:30:",      // missing second
                 "12:30:00x",   // trailing garbage
                 "12:30:00 x",
                 "x12:30:00", // leading garbage
                 "12;30;00",  // wrong separator
                 "12-30-00", "12.30.00",
                 "12: 30:00", // internal whitespace
                 "12 :30:00", "12:30: 00",
                 "12:-30:00", // sign in an inner field
                 "12:+30:00", "12:30:-05", "12:30:+05",
                 "--12:30:00", // doubled sign
                 "+-12:30:00",
                 "12:30:1e2", // scientific notation
                 "12:30:1E2",
                 "12:30:nan", // std::from_chars would otherwise accept these
                 "12:30:NAN", "12:30:inf", "12:30:infinity",
                 "0x12:30:00", // hex
                 "12:30:0x1p3",
                 "12:30:.5",    // no integer part
                 "12:30:5.5.5", // two decimal points
                 "12:30:5,5", // locale-style comma; must NOT be a decimal point
                 "99999999999999999999:00:00"); // integer overflow
    CAPTURE(input);
    CHECK_THROWS_AS(ra_to_rad(input), std::invalid_argument);
}

TEST_CASE("ra_to_rad rejects out-of-range values", "[astro][ra][error]") {
    const auto input =
        GENERATE(as<std::string_view>{},
                 "24:00:00", // hours must be < 24
                 "25:00:00", "99:00:00",
                 "12:60:00", // minutes must be < 60
                 "12:99:00", // silently accepted by the old scnlib version
                 "12:30:60", // seconds must be < 60
                 "12:30:60.0", "12:30:99.9",
                 "-00:00:01", // RA is never negative
                 "-00:00:00", "-12:00:00");
    CAPTURE(input);
    CHECK_THROWS_AS(ra_to_rad(input), std::invalid_argument);
}

TEST_CASE("ra_to_rad error message names the offending input",
          "[astro][ra][error]") {
    try {
        ra_to_rad("12:99:00");
        FAIL("expected std::invalid_argument");
    } catch (const std::invalid_argument& e) {
        const std::string msg = e.what();
        CHECK(msg.find("12:99:00") != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
//  DEC: valid input
// ---------------------------------------------------------------------------
TEST_CASE("dec_to_rad parses valid declinations", "[astro][dec]") {
    SECTION("cardinal values") {
        CHECK_THAT(dec_to_rad("00:00:00"), WithinAbs(0.0, kTol));
        CHECK_THAT(dec_to_rad("90:00:00"), WithinAbs(kPi / 2.0, kTol));
        CHECK_THAT(dec_to_rad("-90:00:00"), WithinAbs(-kPi / 2.0, kTol));
        CHECK_THAT(dec_to_rad("+90:00:00"), WithinAbs(kPi / 2.0, kTol));
    }

    SECTION("positive, with and without an explicit sign") {
        CHECK_THAT(dec_to_rad("41:16:09.0"),
                   WithinAbs(degrees_to_rad(41, 16, 9.0), kTol));
        CHECK_THAT(dec_to_rad("+41:16:09.0"),
                   WithinAbs(degrees_to_rad(41, 16, 9.0), kTol));
    }

    SECTION("negative") {
        CHECK_THAT(dec_to_rad("-41:16:09.0"),
                   WithinAbs(-degrees_to_rad(41, 16, 9.0), kTol));
        CHECK_THAT(dec_to_rad("-29:00:28.1"),
                   WithinAbs(-degrees_to_rad(29, 0, 28.1), kTol));
    }

    SECTION("sign and magnitude are symmetric") {
        CHECK_THAT(dec_to_rad("-12:34:56.7"),
                   WithinRel(-dec_to_rad("12:34:56.7"), 1e-15));
    }

    SECTION("whitespace tolerated") {
        CHECK_THAT(dec_to_rad("  -41:16:09.0\r\n"),
                   WithinAbs(-degrees_to_rad(41, 16, 9.0), kTol));
    }
}

// ---------------------------------------------------------------------------
//  DEC: the regression that motivated this rewrite
// ---------------------------------------------------------------------------
TEST_CASE("dec_to_rad keeps the sign when the degree field is zero",
          "[astro][dec][regression]") {
    // The scnlib version computed sign * abs(deg) and fed that to dms_to_rad.
    // With deg == 0 that is -1 * 0 == 0, so the whole band (-1deg, 0deg) was
    // silently mirrored into the northern hemisphere.
    SECTION("-00:30:00 is thirty arcminutes SOUTH") {
        const double v = dec_to_rad("-00:30:00");
        CHECK(v < 0.0);
        CHECK_THAT(v, WithinAbs(-degrees_to_rad(0, 30, 0), kTol));
    }

    SECTION("smallest representable southern offsets") {
        CHECK(dec_to_rad("-00:00:01") < 0.0);
        CHECK(dec_to_rad("-00:00:00.001") < 0.0);
        CHECK_THAT(dec_to_rad("-00:00:01"),
                   WithinAbs(-degrees_to_rad(0, 0, 1), kTol));
    }

    SECTION("+00:30:00 stays north") {
        CHECK(dec_to_rad("+00:30:00") > 0.0);
        CHECK(dec_to_rad("00:30:00") > 0.0);
    }

    SECTION("mirrored pairs differ") {
        CHECK(dec_to_rad("-00:30:00") != dec_to_rad("00:30:00"));
        CHECK_THAT(dec_to_rad("-00:30:00") + dec_to_rad("00:30:00"),
                   WithinAbs(0.0, kTol));
    }

    SECTION("signed zero compares equal to zero") {
        CHECK_THAT(dec_to_rad("-00:00:00"), WithinAbs(0.0, kTol));
    }
}

// ---------------------------------------------------------------------------
//  DEC: rejected input
// ---------------------------------------------------------------------------
TEST_CASE("dec_to_rad rejects malformed input", "[astro][dec][error]") {
    const auto input = GENERATE(
        as<std::string_view>{}, "", "   ", "41", "41:16", "41:16:09:00",
        ":16:09", "41::09", "41:16:", "41:16:09x", "x41:16:09", "41;16;09",
        "41 :16:09", "41: 16:09", "41:-16:09", "41:16:-09", "--41:16:09",
        "+-41:16:09", "-+41:16:09", "41:16:1e1", "41:16:nan", "41:16:inf",
        "0x41:16:09", "41:16:.5", "41:16:0.9.9", "99999999999999999999:00:00");
    CAPTURE(input);
    CHECK_THROWS_AS(dec_to_rad(input), std::invalid_argument);
}

TEST_CASE("dec_to_rad rejects out-of-range values", "[astro][dec][error]") {
    const auto input =
        GENERATE(as<std::string_view>{},
                 "90:00:00.1", // just past the pole
                 "-90:00:00.1", "90:00:01", "-90:00:01", "90:01:00", "91:00:00",
                 "-91:00:00", "180:00:00",
                 "41:60:00", // minutes must be < 60
                 "41:16:60", // seconds must be < 60
                 "41:99:09");
    CAPTURE(input);
    CHECK_THROWS_AS(dec_to_rad(input), std::invalid_argument);
}

TEST_CASE("dec_to_rad error message names the offending input",
          "[astro][dec][error]") {
    try {
        dec_to_rad("91:00:00");
        FAIL("expected std::invalid_argument");
    } catch (const std::invalid_argument& e) {
        const std::string msg = e.what();
        CHECK(msg.find("91:00:00") != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
//  Cross-checks against catalogue coordinates
// ---------------------------------------------------------------------------
TEST_CASE("known objects round-trip to the expected radians", "[astro][data]") {
    struct Target {
        std::string_view name;
        std::string_view ra;
        std::string_view dec;
        double ra_h, ra_m, ra_s;
        double dec_d, dec_m, dec_s;
        int dec_sign;
    };

    const auto t = GENERATE(
        // Sgr A*
        Target{"Sgr A*", "17:45:40.0409", "-29:00:28.118", 17, 45, 40.0409, 29,
               0, 28.118, -1},
        // Betelgeuse
        Target{"Betelgeuse", "05:55:10.3053", "+07:24:25.426", 5, 55, 10.3053,
               7, 24, 25.426, +1},
        // Northern object very close to the equator, exercising the sign path
        Target{"near-equator", "00:00:01.0", "-00:00:59.999", 0, 0, 1.0, 0, 0,
               59.999, -1});

    CAPTURE(t.name);
    CHECK_THAT(ra_to_rad(t.ra),
               WithinAbs(hours_to_rad(t.ra_h, t.ra_m, t.ra_s), kTol));
    CHECK_THAT(dec_to_rad(t.dec),
               WithinAbs(t.dec_sign * degrees_to_rad(t.dec_d, t.dec_m, t.dec_s),
                         kTol));
    CHECK(std::signbit(dec_to_rad(t.dec)) == (t.dec_sign < 0));
}

TEST_CASE("outputs stay inside the mathematical domain", "[astro][invariant]") {
    const auto ra = GENERATE(as<std::string_view>{}, "00:00:00", "05:30:15.5",
                             "11:59:59.9", "12:00:00", "23:59:59.999");
    CAPTURE(ra);
    const double r = ra_to_rad(ra);
    CHECK(r >= 0.0);
    CHECK(r < 2.0 * std::numbers::pi);

    const auto dec =
        GENERATE(as<std::string_view>{}, "-90:00:00", "-45:30:00", "-00:00:01",
                 "00:00:00", "+00:00:01", "+45:30:00", "+90:00:00");
    CAPTURE(dec);
    const double d = dec_to_rad(dec);
    CHECK(d >= -std::numbers::pi / 2.0);
    CHECK(d <= std::numbers::pi / 2.0);
}

} // namespace sigproc::astro