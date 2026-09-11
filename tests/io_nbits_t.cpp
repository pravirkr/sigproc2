#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/bits.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>
#include <sigproc/io.hpp>

#include "fil_test_utils.hpp"

namespace {

sigproc::io::SigprocHeader
header_with_nbits(int nbits, int nchans, int nsamps) {
    sigproc::io::SigprocHeader hdr;
    hdr.set("source_name", std::string("RAMP"));
    hdr.set("data_type", 1);
    hdr.set("nchans", nchans);
    hdr.set("nbits", nbits);
    hdr.set("nifs", 1);
    hdr.set("nsamples", nsamps);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -1.0);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", 50000.0);
    return hdr;
}

} // namespace

TEST_CASE("FilterbankWriter/Reader round-trip 8-bit 4-chan two-sample file") {
    auto hdr                         = header_with_nbits(8, 4, 2);
    const std::vector<float> samples = {1.F, 2.F, 3.F, 4.F, 5.F, 6.F, 7.F, 8.F};

    std::ostringstream out(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(out, hdr);
        writer.write_block(samples, static_cast<int>(samples.size()));
    }

    std::istringstream in(out.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    REQUIRE(reader.hdr.get<int>("nchans") == 4);
    REQUIRE(reader.hdr.get<int>("nbits") == 8);

    std::vector<float> block;
    reader.read_block(0, 2, block);
    REQUIRE(block.size() == samples.size());
    REQUIRE(block == samples);
}

TEST_CASE("nbits integer ramp round-trip through writer/reader") {
    const std::vector<int> widths = {1, 2, 4, 8, 16, 32};
    for (int nbits : widths) {
        INFO("nbits=" << nbits);
        const int nchans = 8;
        const int nsamps = 4;
        auto hdr         = header_with_nbits(nbits, nchans, nsamps);
        sigproc::bits::BitsInfo info(static_cast<sigproc::SizeType>(nbits));
        const auto maxv = static_cast<int>(info.get_digi_max());
        std::vector<float> samples(static_cast<std::size_t>(nchans * nsamps));
        for (std::size_t i = 0; i < samples.size(); ++i) {
            samples[i] = static_cast<float>(static_cast<int>(i) % (maxv + 1));
        }

        std::ostringstream out(std::ios::binary);
        {
            sigproc::FilterbankWriter writer(out, hdr);
            writer.write_block(samples, static_cast<int>(samples.size()));
        }
        std::istringstream in(out.str(), std::ios::binary);
        sigproc::FilterbankReader reader(in);
        std::vector<float> block;
        reader.read_block(0, nsamps, block);
        REQUIRE(block == samples);
    }
}

TEST_CASE("copy_samples payload equals a file slice") {
    auto hdr = header_with_nbits(8, 4, 4);
    std::vector<float> samples(16);
    for (int i = 0; i < 16; ++i) {
        samples[static_cast<std::size_t>(i)] = static_cast<float>(i);
    }
    std::ostringstream whole(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(whole, hdr);
        writer.write_block(samples, static_cast<int>(samples.size()));
    }

    std::istringstream in(whole.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    std::ostringstream slice(std::ios::binary);
    reader.copy_samples(slice, 1, 2); // samples 1..2 (0-based), 8 bytes

    // Rebuild just samples 1 and 2 (8 values) as a payload-only comparison.
    std::vector<std::uint8_t> expected;
    for (int i = 4; i < 12; ++i) {
        expected.push_back(static_cast<std::uint8_t>(i));
    }
    const auto got = slice.str();
    REQUIRE(got.size() == expected.size());
    REQUIRE(std::vector<std::uint8_t>(got.begin(), got.end()) == expected);
}

TEST_CASE("write_raw_header matches raw_header()") {
    auto hdr                         = header_with_nbits(8, 4, 2);
    const std::vector<float> samples = {1, 2, 3, 4, 5, 6, 7, 8};
    std::ostringstream whole(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(whole, hdr);
        writer.write_block(samples, 8);
    }
    std::istringstream in(whole.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    std::ostringstream raw(std::ios::binary);
    reader.write_raw_header(raw);
    const auto from_reader = raw.str();
    const auto from_hdr    = reader.hdr.raw_header();
    REQUIRE(from_reader.size() == from_hdr.size());
    REQUIRE(std::equal(from_hdr.begin(), from_hdr.end(),
                       reinterpret_cast<const std::byte*>(from_reader.data())));
}

TEST_CASE("get_readplan with nsamples==0 has until_eof") {
    sigproc::io::SigprocHeader hdr;
    hdr.set("nchans", 4);
    hdr.set("nbits", 8);
    hdr.set("nifs", 1);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", 50000.0);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -1.0);
    hdr.set("data_type", 1);
    std::vector<float> samples = {1, 2, 3, 4, 5, 6, 7, 8};
    std::ostringstream whole(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(whole, hdr);
        writer.write_block(samples, 8);
    }
    sigproc::test::NonSeekableIStream in(whole.str());
    sigproc::FilterbankReader reader(in);
    REQUIRE(reader.nsamps() == 0);
    const auto plan = reader.get_readplan(2);
    REQUIRE(plan.size() == 1);
    REQUIRE(plan[0].until_eof);
    REQUIRE(plan[0].nvalues == 8); // 2 samples * 4 chans

    std::vector<float> block;
    auto n = reader.read_plan(plan[0].nvalues, block, 0);
    REQUIRE(n == 8);
    n = reader.read_plan(plan[0].nvalues, block, 0);
    REQUIRE(n == 0);
}

TEST_CASE("read_data returns a short count on truncated payload") {
    auto hdr = header_with_nbits(8, 4, 2);
    std::ostringstream whole(std::ios::binary);
    hdr.tostream(whole);
    // only 3 bytes of the 8-byte payload
    whole.put(static_cast<char>(1));
    whole.put(static_cast<char>(2));
    whole.put(static_cast<char>(3));

    std::istringstream in(whole.str(), std::ios::binary);
    sigproc::io::SigprocHeader parsed;
    REQUIRE(parsed.fromstream(in));
    sigproc::io::FileIO fio(in, 8);
    std::vector<float> block;
    const auto n = fio.read_data(block, 8);
    REQUIRE(n == 3);
    REQUIRE(block.size() == 3);
    REQUIRE(block[0] == 1.F);
    REQUIRE(block[1] == 2.F);
    REQUIRE(block[2] == 3.F);
}

TEST_CASE("FilterbankWriter to ostringstream round-trips") {
    auto hdr                         = header_with_nbits(32, 2, 2);
    const std::vector<float> samples = {1.5F, 2.5F, 3.5F, 4.5F};
    std::ostringstream out(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(out, hdr);
        writer.write_block(samples, 4);
    }
    std::istringstream in(out.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    std::vector<float> block;
    reader.read_block(0, 2, block);
    REQUIRE(block == samples);
}

TEST_CASE("copy_samples(0, n) is the first n spectra") {
    auto hdr                   = header_with_nbits(8, 2, 4);
    std::vector<float> samples = {10, 11, 20, 21, 30, 31, 40, 41};
    std::ostringstream whole(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(whole, hdr);
        writer.write_block(samples, 8);
    }
    std::istringstream in(whole.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    std::ostringstream slice(std::ios::binary);
    reader.copy_samples(slice, 0, 2);
    REQUIRE(slice.str().size() == 4);
    REQUIRE(static_cast<unsigned char>(slice.str()[0]) == 10);
    REQUIRE(static_cast<unsigned char>(slice.str()[1]) == 11);
    REQUIRE(static_cast<unsigned char>(slice.str()[2]) == 20);
    REQUIRE(static_cast<unsigned char>(slice.str()[3]) == 21);
}

TEST_CASE("seek_sample(1) then read is the second spectrum") {
    auto hdr                   = header_with_nbits(8, 2, 3);
    std::vector<float> samples = {1, 2, 3, 4, 5, 6};
    std::ostringstream whole(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(whole, hdr);
        writer.write_block(samples, 6);
    }
    std::istringstream in(whole.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    reader.seek_sample(1);
    std::vector<float> block;
    reader.read_block(1, 1, block);
    REQUIRE(block.size() == 2);
    REQUIRE(block[0] == 3.F);
    REQUIRE(block[1] == 4.F);
}

TEST_CASE("nifs=2 stride is nchans*nifs") {
    sigproc::io::SigprocHeader hdr;
    hdr.set("nchans", 2);
    hdr.set("nifs", 2);
    hdr.set("nbits", 8);
    hdr.set("nsamples", 1);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", 50000.0);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -1.0);
    hdr.set("data_type", 1);
    const std::vector<float> samples = {1, 2, 3, 4};
    std::ostringstream whole(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(whole, hdr);
        writer.write_block(samples, 4);
    }
    std::istringstream in(whole.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    REQUIRE(reader.stride_len() == 4);
    std::vector<float> block;
    reader.read_block(0, 1, block);
    REQUIRE(block == samples);
}

TEST_CASE("ReadPlan.nvalues is SizeType not int overflow-prone gulp*stride") {
    auto hdr = header_with_nbits(8, 4, 16);
    std::vector<float> samples(64);
    for (int i = 0; i < 64; ++i) {
        samples[static_cast<std::size_t>(i)] = static_cast<float>(i);
    }
    std::ostringstream whole(std::ios::binary);
    {
        sigproc::FilterbankWriter writer(whole, hdr);
        writer.write_block(samples, 64);
    }
    std::istringstream in(whole.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    const auto plan = reader.get_readplan(5);
    REQUIRE_FALSE(plan.empty());
    REQUIRE(plan.front().nvalues == 5 * 4);
}
