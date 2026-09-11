#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/astro.hpp>
#include <sigproc/bits.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>
#include <sigproc/kernels.hpp>
#include <sigproc/version.hpp>

namespace {

std::filesystem::path temp_path(std::string_view name) {
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("version header is generated from the project version") {
    REQUIRE(sigproc::kProjectName == "sigproc");
    REQUIRE(sigproc::kVersion == "0.1.0");
    REQUIRE(sigproc::kVersionMajor == 0);
    REQUIRE(sigproc::kVersionMinor == 1);
    REQUIRE(sigproc::kVersionPatch == 0);
}

TEST_CASE("BitsInfo accepts supported bit depths") {
    const sigproc::bits::BitsInfo info(8);
    REQUIRE(info.get_nbits() == 8);
    REQUIRE_FALSE(info.get_can_pack_unpack());
    REQUIRE(info.get_bitfact() == 1);
    REQUIRE(info.get_itemsize() == sizeof(std::uint8_t));
}

TEST_CASE("4-bit pack/unpack round-trips little-endian samples") {
    const std::vector<std::uint8_t> unpacked = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<std::uint8_t> packed(unpacked.size() / 2, 0);
    sigproc::bits::pack(unpacked, packed, 4, "little");

    std::vector<std::uint8_t> restored(unpacked.size(), 0);
    sigproc::bits::unpack(packed, restored, 4, "little");
    REQUIRE(restored == unpacked);
}

TEST_CASE("astro RA/DEC conversions") {
    const double ra  = sigproc::astro::ra_to_rad("12:30:00.0000");
    const double dec = sigproc::astro::dec_to_rad("-30:00:00.0000");
    REQUIRE(std::abs(ra - sigproc::astro::hms_to_rad(12, 30, 0.0)) < 1e-12);
    REQUIRE(std::abs(dec - sigproc::astro::dms_to_rad(-30, 0, 0.0)) < 1e-12);
    REQUIRE(sigproc::astro::mjd_to_gregorian(50000) == "1995-10-10");
    REQUIRE(sigproc::astro::radec_to_str(123000.0) == "12:30:00.0000");
}

TEST_CASE("SigprocHeader get/set and file round-trip") {
    sigproc::io::SigprocHeader hdr;
    hdr.set("source_name", std::string("J0534+2200"));
    hdr.set("nchans", 8);
    hdr.set("nbits", 32);
    hdr.set("nifs", 1);
    hdr.set("nsamples", 16);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -0.5);
    hdr.set("tsamp", 0.001);
    hdr.set("data_type", 1);
    hdr.update({});

    REQUIRE(hdr.get<std::string>("source_name") == "J0534+2200");
    REQUIRE(hdr.get<int>("nchans") == 8);
    REQUIRE(hdr.try_get<int>("not_a_key") == std::nullopt);

    const auto path = temp_path("sigproc2_header_roundtrip.fil");
    hdr.tofile(path.string());

    sigproc::io::SigprocHeader loaded;
    REQUIRE(loaded.fromfile(path.string()));
    REQUIRE(loaded.get<std::string>("source_name") == "J0534+2200");
    REQUIRE(loaded.get<int>("nchans") == 8);
    REQUIRE(loaded.get<int>("nbits") == 32);
    REQUIRE(loaded.get<int>("nsamples") == 16);
    REQUIRE(loaded.get<double>("fch1") == 1400.0);

    std::filesystem::remove(path);
}

TEST_CASE("get_bpass sums each channel over time") {
    // Layout is sample-major: [s0c0, s0c1, s0c2, s1c0, s1c1, s1c2]
    const std::vector<float> in = {1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F};
    std::vector<double> out(3, 0.0);
    sigproc::kernels::get_bpass(in, out, 3, 2);
    REQUIRE(out[0] == 5.0);
    REQUIRE(out[1] == 7.0);
    REQUIRE(out[2] == 9.0);
}

TEST_CASE("FilterbankWriter and FilterbankReader 32-bit round-trip") {
    sigproc::io::SigprocHeader hdr;
    hdr.set("nchans", 4);
    hdr.set("nbits", 32);
    hdr.set("nifs", 1);
    hdr.set("nsamples", 2);
    hdr.set("fch1", 1500.0);
    hdr.set("foff", -1.0);
    hdr.set("tsamp", 0.0001);
    hdr.set("data_type", 1);
    hdr.update({});

    const std::vector<float> samples = {1.0F, 2.0F, 3.0F, 4.0F,
                                        5.0F, 6.0F, 7.0F, 8.0F};
    const auto path = temp_path("sigproc2_filterbank_roundtrip.fil");
    {
        sigproc::FilterbankWriter writer(path.string(), hdr);
        writer.write_block(samples, static_cast<int>(samples.size()));
    }

    sigproc::FilterbankReader reader(path.string());
    REQUIRE(reader.hdr.get<int>("nchans") == 4);
    REQUIRE(reader.hdr.get<int>("nbits") == 32);

    std::vector<float> block;
    reader.read_block(0, 2, block);
    REQUIRE(block.size() == samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        REQUIRE(block[i] == samples[i]);
    }

    std::filesystem::remove(path);
}
