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
    CLI::App app{"fast_fake - create a Gaussian-noise fake filterbank file"};
    app.allow_non_standard_option_names();
    app.set_help_flag("-h,--help", "This help text");

    sigproc::fake::FakeConfig cfg;
    sigproc::fake::apply_fast_defaults(cfg);

    double tobs       = cfg.tobs;
    double tsamp_us   = cfg.tsamp * 1.0e6;
    double tstart     = cfg.tstart;
    double fch1       = cfg.fch1;
    double foff       = cfg.foff;
    int nbits         = cfg.nbits;
    int nchans        = cfg.nchans;
    std::int64_t seed = cfg.seed;
    int telescope_id  = cfg.telescope_id;
    int machine_id    = cfg.machine_id;
    std::string name =
        std::string(sigproc::fake::FastFakeDefaults::source_name);
    std::string outfile;
    bool test_mode = false;

    app.add_option("--tobs,-T", tobs, "Total observation time, s (def=270)");
    app.add_option("--tsamp,-t", tsamp_us, "Sampling interval, us (def=64)");
    app.add_option("--mjd,-m", tstart, "MJD of first sample (def=56000.0)");
    app.add_option("--fch1,-F", fch1,
                   "Frequency of channel 1, MHz (def=1581.804688)");
    app.add_option("--foff,-f", foff, "Channel bandwidth, MHz (def=-0.390625)");
    app.add_option("--nbits,-b", nbits, "Output number of bits (def=2)");
    app.add_option("--nchans,-c", nchans,
                   "Output number of channels (def=1024)");
    app.add_option("--seed,-S", seed, "Random seed (def=time())");
    app.add_option("--name,-s", name, "Source name for header (def=FAKE)");
    app.add_option("--tid", telescope_id, "Telescope ID in header (def=4)");
    app.add_option("--bid", machine_id, "Backend ID in header (def=10)");
    app.add_option("--out,-o", outfile, "Output file (def=stdout)");
    app.add_flag("--test,-0", test_mode,
                 "Write header only; skip payload (original --test)");

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    cfg.tobs         = tobs;
    cfg.tsamp        = tsamp_us * 1.0e-6;
    cfg.tstart       = tstart;
    cfg.fch1         = fch1;
    cfg.foff         = foff;
    cfg.nbits        = nbits;
    cfg.nchans       = nchans;
    cfg.seed         = seed;
    cfg.telescope_id = telescope_id;
    cfg.machine_id   = machine_id;
    cfg.source_name  = name;
    cfg.test_mode    = test_mode;
    cfg.fast         = true;

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
