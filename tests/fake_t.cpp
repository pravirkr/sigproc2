#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <sigproc/fake.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>

TEST_CASE("foff default is already negative 0.062") {
    const sigproc::fake::FakeConfig cfg;
    REQUIRE(cfg.foff == -0.062);
    REQUIRE_FALSE(cfg.fast);
}

TEST_CASE("fake_dmdelay matches dmdelay.c constant") {
    const double f0  = 1400.0;
    const double f1  = 1399.0;
    const double dm  = 42.0;
    const double got = sigproc::fake::fake_dmdelay(f0, f1, dm);
    const double exp =
        4148.741601 * ((1.0 / (f0 * f0)) - (1.0 / (f1 * f1))) * dm;
    REQUIRE(got == exp);
    REQUIRE_THAT(got, Catch::Matchers::WithinAbs(exp, 1e-18));
}

TEST_CASE("generated header has core keys and parses") {
    sigproc::fake::FakeConfig cfg;
    cfg.nchans   = 8;
    cfg.nbits    = 8;
    cfg.nifs     = 1;
    cfg.tsamp    = 0.001;
    cfg.tobs     = 0.016;
    cfg.tstart   = 50000.0;
    cfg.fch1     = 1400.0;
    cfg.foff     = -1.0;
    cfg.seed     = 1;
    cfg.period_s = 0.01;
    cfg.dm       = 10.0;
    cfg.smear    = false;

    std::ostringstream out(std::ios::binary);
    sigproc::fake::generate(out, cfg);
    const auto bytes = out.str();
    REQUIRE_FALSE(bytes.empty());

    std::istringstream in(bytes, std::ios::binary);
    sigproc::FilterbankReader reader(in);
    REQUIRE(reader.hdr.get<int>("nchans") == 8);
    REQUIRE(reader.hdr.get<int>("nbits") == 8);
    REQUIRE(reader.hdr.get<int>("nifs") == 1);
    REQUIRE(reader.hdr.get<int>("machine_id") == 10);
    REQUIRE(reader.hdr.get<int>("telescope_id") == 4);
    REQUIRE(reader.hdr.get<int>("data_type") == 1);
    REQUIRE(reader.hdr.get<double>("tstart") == 50000.0);
    REQUIRE(reader.hdr.get<double>("tsamp") == 0.001);
    REQUIRE(reader.hdr.get<double>("fch1") == 1400.0);
    REQUIRE(reader.hdr.get<double>("foff") == -1.0);
    REQUIRE(reader.hdr.is_present("source_name"));
    REQUIRE_FALSE(reader.hdr.is_present("nsamples"));
}

TEST_CASE("seed-stable noise") {
    sigproc::fake::FakeConfig cfg;
    cfg.nchans   = 4;
    cfg.nbits    = 8;
    cfg.tsamp    = 0.001;
    cfg.tobs     = 0.008;
    cfg.seed     = 12345;
    cfg.period_s = 0.0;
    cfg.dm       = 0.0;
    cfg.smear    = false;

    std::ostringstream a(std::ios::binary);
    std::ostringstream b(std::ios::binary);
    sigproc::fake::generate(a, cfg);
    sigproc::fake::generate(b, cfg);
    REQUIRE(a.str() == b.str());

    cfg.seed = 999;
    std::ostringstream c(std::ios::binary);
    sigproc::fake::generate(c, cfg);
    REQUIRE(c.str() != a.str());
}

TEST_CASE("evenodd writes 0/1 on even/odd channels") {
    sigproc::fake::FakeConfig cfg;
    cfg.nchans  = 4;
    cfg.nbits   = 8;
    cfg.tsamp   = 0.001;
    cfg.tobs    = 0.002;
    cfg.evenodd = true;
    cfg.seed    = 1;

    std::ostringstream out(std::ios::binary);
    sigproc::fake::generate(out, cfg);
    std::istringstream in(out.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    REQUIRE(reader.hdr.get<int>("nbits") == 32);
    std::vector<float> block;
    reader.read_block(0, 2, block);
    REQUIRE(block.size() == 8);
    for (int s = 0; s < 2; ++s) {
        for (int c = 0; c < 4; ++c) {
            REQUIRE(block[static_cast<std::size_t>(s * 4 + c)] ==
                    static_cast<float>(c % 2));
        }
    }
}

TEST_CASE("fast 2-bit values clip to [0,3]") {
    sigproc::fake::FakeConfig cfg;
    cfg.fast   = true;
    cfg.nchans = 16;
    cfg.nbits  = 2;
    cfg.tsamp  = 64e-6;
    cfg.tobs   = 64e-6 * 8;
    cfg.tstart = 56000.0;
    cfg.seed   = 7;
    cfg.fch1   = 1581.804688;
    cfg.foff   = -0.390625;
    cfg.nbeams = 1;
    cfg.ibeam  = 1;

    std::ostringstream out(std::ios::binary);
    sigproc::fake::generate(out, cfg);
    std::istringstream in(out.str(), std::ios::binary);
    sigproc::FilterbankReader reader(in);
    REQUIRE(reader.hdr.get<int>("nbits") == 2);
    REQUIRE(reader.hdr.get<double>("tstart") == 56000.0);
    std::vector<float> block;
    const auto n = reader.read_plan(16 * 8, block, 0);
    REQUIRE(n > 0);
    for (float v : block) {
        REQUIRE(v >= 0.0F);
        REQUIRE(v <= 3.0F);
    }
}

TEST_CASE("positive foff is negated") {
    sigproc::fake::FakeConfig cfg;
    cfg.nchans   = 2;
    cfg.nbits    = 8;
    cfg.tsamp    = 0.001;
    cfg.tobs     = 0.001;
    cfg.foff     = 0.062;
    cfg.period_s = 0.0;
    cfg.dm       = 0.0;
    cfg.seed     = 1;
    std::ostringstream out(std::ios::binary);
    sigproc::fake::generate(out, cfg);
    std::istringstream in(out.str(), std::ios::binary);
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromstream(in));
    REQUIRE(hdr.get<double>("foff") == -0.062);
}
