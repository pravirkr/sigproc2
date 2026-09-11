#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>
#include <sigproc/kernels.hpp>
#include <sigproc/timeseries.hpp>

namespace {

sigproc::io::SigprocHeader make_fil_hdr(int nchans, int nsamps, int nbits = 8) {
    sigproc::io::SigprocHeader hdr;
    hdr.set("source_name", std::string("TIM"));
    hdr.set("data_type", 1);
    hdr.set("nchans", nchans);
    hdr.set("nbits", nbits);
    hdr.set("nifs", 1);
    hdr.set("nsamples", nsamps);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -1.0);
    hdr.set("tsamp", 0.01);
    hdr.set("tstart", 50000.0);
    hdr.set("refdm", 12.5);
    return hdr;
}

} // namespace

TEST_CASE("TimeSeries tostream writes data_type=2 nchans=1 nbits=32") {
    auto hdr = make_fil_hdr(8, 4, 32);
    std::vector<float> samples{1.F, 2.F, 3.F, 4.F};
    sigproc::TimeSeries ts(std::move(hdr), samples);

    REQUIRE(ts.hdr().get<int>("data_type") == 2);
    REQUIRE(ts.hdr().get<int>("nchans") == 1);
    REQUIRE(ts.hdr().get<int>("nbits") == 32);
    REQUIRE(ts.hdr().get<int>("nsamples") == 4);
    REQUIRE(ts.hdr().get<double>("fch1") == 1400.0);
    REQUIRE(ts.hdr().get<double>("refdm") == 12.5);
    REQUIRE(ts.hdr().get<std::string>("datatype") == "time series");
    REQUIRE(ts.samples().size() == 4);

    std::ostringstream out(std::ios::binary);
    ts.tostream(out);
    const auto bytes = out.str();
    REQUIRE_FALSE(bytes.empty());

    std::istringstream in(bytes, std::ios::binary);
    sigproc::FilterbankReader reader(in);
    REQUIRE(reader.hdr.get<int>("data_type") == 2);
    REQUIRE(reader.hdr.get<int>("nchans") == 1);
    REQUIRE(reader.hdr.get<int>("nbits") == 32);
    REQUIRE(reader.hdr.get<int>("nsamples") == 4);
    std::vector<float> got;
    reader.read_block(0, 4, got);
    REQUIRE(got == samples);
}

TEST_CASE("TimeSeries tofile round-trips a .tim-shaped file") {
    const auto path =
        std::filesystem::temp_directory_path() / "sigproc2_roundtrip.tim";
    auto hdr = make_fil_hdr(4, 3, 8);
    sigproc::TimeSeries ts(std::move(hdr), {10.F, 20.F, 30.F});
    ts.tofile(path.string());

    sigproc::io::SigprocHeader rh;
    REQUIRE(rh.fromfile(path.string()));
    REQUIRE(rh.get<int>("data_type") == 2);
    REQUIRE(rh.get<std::string>("datatype") == "time series");

    sigproc::FilterbankReader reader(path.string());
    std::vector<float> got;
    reader.read_block(0, 3, got);
    REQUIRE(got == std::vector<float>{10.F, 20.F, 30.F});
    std::filesystem::remove(path);
}

TEST_CASE("make_tim_header collapses a frequency table to fch1") {
    sigproc::io::SigprocHeader hdr;
    hdr.set("data_type", 1);
    hdr.set("nchans", 2);
    hdr.set("nbits", 8);
    hdr.set("nifs", 1);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", 50000.0);
    hdr.set_freq_table({1500.0, 1499.0});
    const auto tim = sigproc::make_tim_header(hdr, 16);
    REQUIRE(tim.get<int>("data_type") == 2);
    REQUIRE(tim.get<int>("nchans") == 1);
    REQUIRE(tim.get<int>("nbits") == 32);
    REQUIRE(tim.get<double>("fch1") == 1500.0);
    REQUIRE_FALSE(tim.has_freq_table());
    REQUIRE(tim.get<int>("nsamples") == 16);
}

TEST_CASE("gulp_median ignores a single spike") {
    const std::vector<float> v{2.F, 2.F, 2.F, 2.F, 2.F, 2.F, 2.F, 100.F};
    REQUIRE(sigproc::kernels::gulp_median(v) == 2.F);
}

TEST_CASE("flatten_gulp constant nonzero gulp is zeros") {
    const std::vector<float> in(8, 5.F);
    std::vector<float> out(8, 99.F);
    const float med = sigproc::kernels::gulp_median(in);
    REQUIRE(med == 5.F);
    sigproc::kernels::flatten_gulp(in, out, med, med);
    REQUIRE(out == std::vector<float>(8, 0.F));
}

TEST_CASE("flatten_gulp scale 0 writes zeros (no UB)") {
    const std::vector<float> in(4, 0.F);
    std::vector<float> out(4, 7.F);
    sigproc::kernels::flatten_gulp(in, out, 0.F, 0.F);
    REQUIRE(out == std::vector<float>(4, 0.F));
}

TEST_CASE("flatten_gulp spike does not move the median") {
    const std::vector<float> in{2.F, 2.F, 2.F, 2.F, 2.F, 2.F, 2.F, 100.F};
    std::vector<float> out(8);
    const float med = sigproc::kernels::gulp_median(in);
    REQUIRE(med == 2.F);
    sigproc::kernels::flatten_gulp(in, out, med, med);
    for (int i = 0; i < 7; ++i) {
        REQUIRE(out[static_cast<std::size_t>(i)] == 0.F);
    }
    REQUIRE_THAT(out[7], Catch::Matchers::WithinAbs(49.F, 1e-5F));
}

TEST_CASE("clip_gulp replaces one spike with the median") {
    const std::vector<float> in{1.F, 1.F, 1.F, 1.F, 1.F, 1.F, 1.F, 100.F};
    std::vector<float> out(8);
    sigproc::kernels::clip_gulp(in, out);
    REQUIRE(out == std::vector<float>(8, 1.F));
}

TEST_CASE("pulse_phase matches blanker.c turn += tsamp/period") {
    const double tsamp  = 0.01;
    const double period = 1.0;
    REQUIRE_THAT(sigproc::kernels::pulse_phase(0, tsamp, period),
                 Catch::Matchers::WithinAbs(0.01, 1e-12));
    REQUIRE_THAT(sigproc::kernels::pulse_phase(24, tsamp, period),
                 Catch::Matchers::WithinAbs(0.25, 1e-12));
    REQUIRE_THAT(sigproc::kernels::pulse_phase(49, tsamp, period),
                 Catch::Matchers::WithinAbs(0.50, 1e-12));
    REQUIRE_THAT(sigproc::kernels::pulse_phase(50, tsamp, period),
                 Catch::Matchers::WithinAbs(0.51, 1e-12));
}

TEST_CASE("zerodm_spectra all-100 spectrum becomes all 64") {
    const std::vector<float> in(8, 100.F);
    std::vector<float> out(8, -1.F);
    sigproc::kernels::zerodm_spectra(in, out, 4);
    REQUIRE(out == std::vector<float>(8, 64.F));
}

TEST_CASE("zerodm_spectra clamps to 0 and 255") {
    std::vector<float> in{0.F, 0.F, 0.F, 255.F};
    std::vector<float> out(4);
    sigproc::kernels::zerodm_spectra(in, out, 4);
    // mean = 63.75, round = 64; 0-64+64=0; 255-64+64=255
    REQUIRE(out[0] == 0.F);
    REQUIRE(out[3] == 255.F);
}

TEST_CASE("zerodm_spectra --float path subtracts mean with no clamp") {
    const std::vector<float> in{10.F, 20.F, 30.F, 40.F};
    std::vector<float> out(4);
    sigproc::kernels::zerodm_spectra(in, out, 4, 0.0F, -1.0e30F, 1.0e30F);
    REQUIRE_THAT(out[0], Catch::Matchers::WithinAbs(-15.F, 1e-5F));
    REQUIRE_THAT(out[3], Catch::Matchers::WithinAbs(15.F, 1e-5F));
}
