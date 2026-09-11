#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/common/params.hpp>
#include <sigproc/header.hpp>

#include "fil_test_utils.hpp"

namespace {

std::int32_t load_i32(const char* p) {
    std::int32_t v{};
    std::memcpy(&v, p, 4);
    return v;
}

#ifndef SIG_TEST_DATA_DIR
#define SIG_TEST_DATA_DIR "."
#endif

} // namespace

TEST_CASE("new header encodes int32 string lengths in kEncodeOrder") {
    auto hdr = sigproc::test::make_tiny_header();
    std::ostringstream out(std::ios::binary);
    hdr.tostream(out);
    const auto bytes = out.str();

    REQUIRE(load_i32(bytes.data()) == 12); // HEADER_START
    REQUIRE(bytes.substr(4, 12) == "HEADER_START");

    const auto keys = sigproc::test::header_key_order(
        std::span<const char>(bytes.data(), bytes.size()));
    const std::vector<std::string> expected = {
        "source_name", "machine_id", "telescope_id", "data_type",
        "tstart",      "tsamp",      "nbits",        "nsamples",
        "fch1",        "foff",       "nchans",       "nifs"};
    REQUIRE(keys == expected);
}

TEST_CASE("barycentric is four bytes on disk and signed is one byte +1") {
    sigproc::io::SigprocHeader hdr;
    hdr.set("nchans", 1);
    hdr.set("nbits", 8);
    hdr.set("nifs", 1);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", 50000.0);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -1.0);
    hdr.set("data_type", 1);
    hdr.set("barycentric", true);
    hdr.set("signed", false);

    std::ostringstream out(std::ios::binary);
    hdr.tostream(out);
    const auto bytes = out.str();

    auto find_payload = [&](std::string_view key) -> const char* {
        std::size_t i = 0;
        while (i + 4 <= bytes.size()) {
            std::int32_t len{};
            std::memcpy(&len, bytes.data() + i, 4);
            i += 4;
            REQUIRE(len >= 1);
            REQUIRE(i + static_cast<std::size_t>(len) <= bytes.size());
            std::string_view tok(bytes.data() + i,
                                 static_cast<std::size_t>(len));
            i += static_cast<std::size_t>(len);
            if (tok == key) {
                return bytes.data() + i;
            }
            if (tok == "HEADER_END") {
                break;
            }
        }
        return nullptr;
    };

    const char* bary = find_payload("barycentric");
    REQUIRE(bary != nullptr);
    REQUIRE(load_i32(bary) == 1);

    const char* sgn = find_payload("signed");
    REQUIRE(sgn != nullptr);
    REQUIRE(static_cast<std::int8_t>(*sgn) == 1);
}

TEST_CASE("empty source_name is omitted from the encode write-set") {
    auto hdr = sigproc::test::make_tiny_header();
    hdr.set("source_name", std::string());
    std::ostringstream out(std::ios::binary);
    hdr.tostream(out);
    const auto keys = sigproc::test::header_key_order(
        std::span<const char>(out.str().data(), out.str().size()));
    REQUIRE(std::find(keys.begin(), keys.end(), "source_name") == keys.end());
}

TEST_CASE("unknown key FOO+int32+HEADER_END warns and parses") {
    std::vector<char> raw;
    sigproc::test::append_token(raw, "HEADER_START");
    sigproc::test::append_token(raw, "nchans");
    sigproc::test::append_i32(raw, 8);
    sigproc::test::append_token(raw, "nbits");
    sigproc::test::append_i32(raw, 8);
    sigproc::test::append_token(raw, "nifs");
    sigproc::test::append_i32(raw, 1);
    sigproc::test::append_token(raw, "tsamp");
    sigproc::test::append_f64(raw, 0.001);
    sigproc::test::append_token(raw, "tstart");
    sigproc::test::append_f64(raw, 50000.0);
    sigproc::test::append_token(raw, "fch1");
    sigproc::test::append_f64(raw, 1400.0);
    sigproc::test::append_token(raw, "foff");
    sigproc::test::append_f64(raw, -1.0);
    sigproc::test::append_token(raw, "FOO");
    sigproc::test::append_i32(raw, 42);
    sigproc::test::append_token(raw, "HEADER_END");

    std::istringstream in(std::string(raw.begin(), raw.end()),
                          std::ios::binary);
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromstream(in));
    REQUIRE(hdr.get<int>("nchans") == 8);
}

TEST_CASE("FREQUENCY_START table is not handled as an unknown key") {
    std::vector<char> raw;
    sigproc::test::append_token(raw, "HEADER_START");
    sigproc::test::append_token(raw, "data_type");
    sigproc::test::append_i32(raw, 1);
    sigproc::test::append_token(raw, "nbits");
    sigproc::test::append_i32(raw, 8);
    sigproc::test::append_token(raw, "nifs");
    sigproc::test::append_i32(raw, 1);
    sigproc::test::append_token(raw, "tsamp");
    sigproc::test::append_f64(raw, 0.001);
    sigproc::test::append_token(raw, "tstart");
    sigproc::test::append_f64(raw, 50000.0);
    sigproc::test::append_token(raw, "FREQUENCY_START");
    sigproc::test::append_token(raw, "nchans");
    sigproc::test::append_i32(raw, 2);
    sigproc::test::append_token(raw, "fchannel");
    sigproc::test::append_f64(raw, 1400.0);
    sigproc::test::append_token(raw, "fchannel");
    sigproc::test::append_f64(raw, 1399.0);
    sigproc::test::append_token(raw, "FREQUENCY_END");
    sigproc::test::append_token(raw, "HEADER_END");

    std::istringstream in(std::string(raw.begin(), raw.end()),
                          std::ios::binary);
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromstream(in));
    REQUIRE(hdr.has_freq_table());
    const auto table = hdr.get_freq_table();
    REQUIRE(table.size() == 2);
    REQUIRE(table[0] == 1400.0);
    REQUIRE(table[1] == 1399.0);
    REQUIRE(hdr.get_freqs().size() == 2);

    std::ostringstream out(std::ios::binary);
    hdr.tostream(out);
    const auto encoded = out.str();
    const auto keys    = sigproc::test::header_key_order(
        std::span<const char>(encoded.data(), encoded.size()));
    REQUIRE(std::find(keys.begin(), keys.end(), "FREQUENCY_START") !=
            keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "fch1") == keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "foff") == keys.end());
}

TEST_CASE("string length 0 and 81 fail the header") {
    auto try_len = [](std::int32_t len) {
        std::vector<char> raw;
        sigproc::test::append_i32(raw, len);
        raw.insert(raw.end(), static_cast<std::size_t>(std::max(len, 1)), 'X');
        std::istringstream in(std::string(raw.begin(), raw.end()),
                              std::ios::binary);
        sigproc::io::SigprocHeader hdr;
        return hdr.fromstream(in);
    };
    REQUIRE_FALSE(try_len(0));
    REQUIRE_FALSE(try_len(81));
}

TEST_CASE("seekable stringstream with bad magic returns false and tellg==0") {
    std::istringstream in(std::string("NOTAHEADER"), std::ios::binary);
    sigproc::io::SigprocHeader hdr;
    REQUIRE_FALSE(hdr.fromstream(in));
    REQUIRE(in.tellg() == std::streampos(0));
}

TEST_CASE("non-seekable shim with bad magic throws") {
    sigproc::test::NonSeekableIStream in(std::string("NOTAHEADER"));
    sigproc::io::SigprocHeader hdr;
    REQUIRE_THROWS_AS(hdr.fromstream(in), std::runtime_error);
}

TEST_CASE("non-seekable valid header succeeds with nsamples==0") {
    auto hdr_src = sigproc::test::make_tiny_header();
    hdr_src.set("nsamples", 0); // omit from present? 0 is still present
    // Build a header-only buffer without nsamples in the write-set.
    sigproc::io::SigprocHeader bare;
    bare.set("nchans", 8);
    bare.set("nbits", 8);
    bare.set("nifs", 1);
    bare.set("tsamp", 0.001);
    bare.set("tstart", 50000.0);
    bare.set("fch1", 1400.0);
    bare.set("foff", -1.0);
    bare.set("data_type", 1);
    std::ostringstream encoded(std::ios::binary);
    bare.tostream(encoded);

    sigproc::test::NonSeekableIStream in(encoded.str());
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromstream(in));
    REQUIRE(hdr.get<int>("nsamples") == 0);
    REQUIRE(hdr.get<int>("nchans") == 8);
    REQUIRE(hdr.raw_header().size() == encoded.str().size());
}

TEST_CASE("seekable header-only stringstream does not throw") {
    auto hdr_src = sigproc::test::make_tiny_header();
    std::ostringstream encoded(std::ios::binary);
    hdr_src.tostream(encoded);
    const auto fed = encoded.str();

    std::istringstream in(fed, std::ios::binary);
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromstream(in));
    REQUIRE(hdr.raw_header().size() == fed.size());
    REQUIRE(hdr.get<int>("nchans") == 8);
}

TEST_CASE("tiny.fil vendor fixture parses") {
    const auto path = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    REQUIRE(std::filesystem::exists(path));
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromfile(path.string()));
    REQUIRE(hdr.get<int>("nchans") == 8);
    REQUIRE(hdr.get<int>("nsamples") == 16);
    REQUIRE(hdr.get<std::string>("source_name") == "TINY");
}
