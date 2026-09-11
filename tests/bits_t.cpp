#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/bits.hpp>

TEST_CASE("4-bit {1,2} packs to 0x21 (low bits first)") {
    const std::vector<std::uint8_t> unpacked = {1, 2};
    std::vector<std::uint8_t> packed(1, 0);
    sigproc::bits::pack(unpacked, packed, 4, "little");
    REQUIRE(packed[0] == 0x21);
}

TEST_CASE("1-bit {1,0,0,0,0,0,0,0} packs to 0x01 including index 0") {
    const std::vector<std::uint8_t> unpacked = {1, 0, 0, 0, 0, 0, 0, 0};
    std::vector<std::uint8_t> packed(1, 0);
    sigproc::bits::pack(unpacked, packed, 1, "little");
    REQUIRE(packed[0] == 0x01);

    std::vector<float> as_float(8, 0.0F);
    sigproc::bits::unpack_to_float(packed, as_float, 1);
    REQUIRE(as_float[0] == 1.0F);
    for (std::size_t i = 1; i < 8; ++i) {
        REQUIRE(as_float[i] == 0.0F);
    }
}

TEST_CASE("unpack_in_place includes index 0 for 4-bit") {
    std::vector<std::uint8_t> buf = {0x21, 0x43, 0, 0};
    sigproc::bits::unpack_in_place(buf, 4, "little");
    REQUIRE(buf[0] == 1);
    REQUIRE(buf[1] == 2);
    REQUIRE(buf[2] == 3);
    REQUIRE(buf[3] == 4);
}

TEST_CASE("integer ramps round-trip through from_float for all nbits") {
    const std::vector<int> widths = {1, 2, 4, 8, 16, 32};
    for (int nbits : widths) {
        INFO("nbits=" << nbits);
        sigproc::bits::BitsInfo info(static_cast<sigproc::SizeType>(nbits));
        const auto maxv = static_cast<int>(info.get_digi_max());
        const int n     = (nbits <= 4) ? (8 / nbits) * 4 : 8;
        std::vector<float> in(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            in[static_cast<std::size_t>(i)] =
                static_cast<float>(i % (maxv + 1));
        }
        std::vector<std::byte> packed(
            sigproc::bits::packed_nbytes(in.size(), info));
        sigproc::bits::from_float(in, packed, info);

        std::vector<float> out(in.size(), -1.0F);
        if (nbits == 1 || nbits == 2 || nbits == 4) {
            sigproc::bits::unpack_to_float(
                std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(packed.data()),
                    packed.size()),
                out, static_cast<sigproc::SizeType>(nbits));
        } else if (nbits == 8) {
            sigproc::bits::u8_to_float(
                std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(packed.data()),
                    in.size()),
                out);
        } else if (nbits == 16) {
            sigproc::bits::u16_to_float(
                std::span<const std::uint16_t>(
                    reinterpret_cast<const std::uint16_t*>(packed.data()),
                    in.size()),
                out);
        } else {
            sigproc::bits::f32_copy(
                std::span<const float>(
                    reinterpret_cast<const float*>(packed.data()), in.size()),
                out);
        }
        REQUIRE(out == in);
    }
}
