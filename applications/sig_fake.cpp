#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/fake.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{"fake - produce fake filterbank format data for testing "
                 "downstream code"};
    sigproc::cli::configure_app(app);
    app.footer("Original short flags keep original units. -period is ms, "
               "-tsamp is microseconds, -width is percent. Positive -foff is "
               "negated. Binary-orbit flags are not supported.");

    int nchans       = 128;
    auto* nchans_opt = app.add_option(
        "-nchans", nchans, "Number of filterbank channels (def=128)");

    int nbits = 4;
    auto* nbits_opt =
        app.add_option("-nbits", nbits, "Number of bits per sample (def=4)");

    int nifs       = 1;
    auto* nifs_opt = app.add_option("-nifs", nifs, "Number of IFs (def=1)");

    double tsamp_us = 80.0;
    auto* tsamp_opt =
        app.add_option("-tsamp", tsamp_us, "Sampling time in us (def=80)");

    double tobs = 10.0;
    auto* tobs_opt =
        app.add_option("-tobs", tobs, "Observation time in s (def=10)");

    double tstart    = 50000.0;
    auto* tstart_opt = app.add_option(
        "-tstart", tstart, "MJD time stamp of first sample (def=50000.0)");

    double fch1    = 433.968;
    auto* fch1_opt = app.add_option(
        "-fch1", fch1, "Frequency of channel 1 in MHz (def=433.968)");

    double foff    = -0.062;
    auto* foff_opt = app.add_option(
        "-foff", foff,
        "Channel bandwidth in MHz (def=-0.062). Positive values are negated.");

    double period_ms = -1.0;
    app.add_option("-period", period_ms,
                   "Period of fake pulsar in ms (def=random)");

    double width_pct = 4.0;
    app.add_option("-width", width_pct, "Pulse width in percent (def=4)");

    double snrpeak = 1.0;
    app.add_option("-snrpeak", snrpeak,
                   "Signal-to-noise ratio of single pulse (def=1.0)");

    double dm = -1.0;
    app.add_option("-dm", dm, "Dispersion measure of fake pulsar (def=random)");

    std::int64_t seed = -1;
    app.add_option("-seed", seed, "Random seed (def=seconds since epoch)");

    bool nosmear = false;
    app.add_flag("-nosmear,--nosmear", nosmear,
                 "Do not add dispersion/sampling smearing (def=add)");

    bool headerless = false;
    app.add_flag("-headerless,--headerless", headerless,
                 "Do not write header info at start of file (def=header)");

    bool evenodd = false;
    app.add_flag("-evenodd,--evenodd", evenodd,
                 "Even channels=0, odd channels=1 (32-bit; def=noise+signal)");

    bool fast = false;
    app.add_flag("--fast", fast,
                 "Use fast_fake defaults and noise-only generation");

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    sigproc::fake::FakeConfig cfg;
    if (fast) {
        sigproc::fake::apply_fast_defaults(cfg);
    }
    auto take = [&](auto* opt, auto& dst, const auto& val) {
        if (!fast || opt->count() > 0) {
            dst = val;
        }
    };
    take(nchans_opt, cfg.nchans, nchans);
    take(nbits_opt, cfg.nbits, nbits);
    take(nifs_opt, cfg.nifs, nifs);
    if (!fast || tsamp_opt->count() > 0) {
        cfg.tsamp = tsamp_us * 1.0e-6;
    }
    take(tobs_opt, cfg.tobs, tobs);
    take(tstart_opt, cfg.tstart, tstart);
    take(fch1_opt, cfg.fch1, fch1);
    take(foff_opt, cfg.foff, foff);
    cfg.seed       = seed;
    cfg.snrpeak    = snrpeak;
    cfg.duty       = width_pct / 100.0;
    cfg.smear      = !nosmear;
    cfg.headerless = headerless;
    cfg.evenodd    = evenodd;
    cfg.fast       = fast;
    if (period_ms >= 0.0) {
        cfg.period_s = period_ms * 1.0e-3;
    }
    if (dm >= 0.0) {
        cfg.dm = dm;
    }

    try {
        if (sigproc::cli::is_stdio_path(outfile)) {
            sigproc::fake::generate(std::cout, cfg);
        } else {
            std::ofstream out(outfile, std::ios::binary);
            if (!out) {
                throw std::runtime_error("Cannot open output file: " + outfile);
            }
            sigproc::fake::generate(out, cfg);
        }
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
