/*
 * extract - slice filterbank samples (1-based) and rewrite tstart.
 *
 * tstart_out = tstart_in + (start_sample - 1) * tsamp / 86400.0  (MJD days)
 * Payload is a packed byte-copy via FilterbankReader::copy_samples (0-based).
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/filterbank.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{
        "extract - copy a 1-based sample range from a filterbank file"};
    sigproc::cli::configure_app(app);
    app.footer("start_sample is 1-based. Output tstart is shifted by "
               "(start_sample-1)*tsamp/86400 MJD days. The output header is "
               "re-encoded (not a byte-copy). Payload is copied packed.");

    std::string filename;
    int start_sample    = 0;
    int samples_to_read = 0;
    app.add_option("filename", filename, "Input filterbank file")->required();
    app.add_option("start_sample", start_sample,
                   "First sample to copy (1-based)")
        ->required();
    app.add_option("samples_to_read", samples_to_read,
                   "Number of time samples to copy")
        ->required();

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    if (start_sample < 1) {
        spdlog::error(
            "start_sample must be >= 1 (1-based, original extract.c)");
        return EXIT_FAILURE;
    }
    if (samples_to_read < 0) {
        spdlog::error("samples_to_read must be >= 0");
        return EXIT_FAILURE;
    }

    try {
        sigproc::FilterbankReader reader(filename);
        if (reader.hdr.get<int>("data_type") != 1) {
            throw std::runtime_error(
                "extract can currently only work with filterbank data");
        }

        const double tsamp  = reader.hdr.get<double>("tsamp");
        const double tstart = reader.hdr.get<double>("tstart");
        const double tstart_out =
            tstart + static_cast<double>(start_sample - 1) * tsamp / 86400.0;
        reader.hdr.set("tstart", tstart_out);
        reader.hdr.set("nsamples", samples_to_read);

        std::ostream* out = &std::cout;
        std::ofstream owned;
        if (!sigproc::cli::is_stdio_path(outfile)) {
            owned.open(outfile, std::ios::binary);
            if (!owned) {
                throw std::runtime_error("Cannot open output file: " + outfile);
            }
            out = &owned;
        }

        reader.hdr.tostream(*out);
        if (samples_to_read > 0) {
            reader.copy_samples(
                *out, static_cast<sigproc::SizeType>(start_sample - 1),
                static_cast<sigproc::SizeType>(samples_to_read));
        }
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
