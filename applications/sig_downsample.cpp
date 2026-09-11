/*
 * downsample - time-only downsample to 32-bit filterbank data.
 *
 * Distinct from sig_decimate (which can also add channels and choose nbits).
 * Original: downsample infile outfile nadd
 */

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/filterbank.hpp>
#include <sigproc/kernels.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{"downsample - reduce time resolution of filterbank data "
                 "(32-bit out, time only)"};
    sigproc::cli::configure_app(app);
    app.footer("Original: downsample infile outfile nadd. Time only; output "
               "nbits is 32. extras: -t/--numsamps, -o/--out.");

    std::string infile;
    std::string outfile;
    int nadd = 0;

    app.add_option("infile", infile, "Input filterbank file")->required();
    // One option: positional outfile, -o, and --out. Do not also call
    // add_output_file() — CLI11 would see a second --outfile.
    app.add_option("-o,--out,out_fil", outfile,
                   "Output file (positional outfile, -o, or --out; default "
                   "stdout; '-' is stdout)")
        ->option_text("outfile");
    app.add_option("-t,--numsamps,nadd", nadd, "Time samples to add")
        ->option_text("nadd");

    int gulp = sigproc::cli::kDefaultGulp;
    sigproc::cli::add_gulp_flag(app, gulp);

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    if (nadd < 1) {
        spdlog::error("nadd must be >= 1 (got {})", nadd);
        return EXIT_FAILURE;
    }

    try {
        sigproc::FilterbankReader reader(infile);
        const int in_nchans = reader.hdr.get<int>("nchans");
        const int in_nifs   = reader.hdr.get<int>("nifs");
        const int in_nsamp  = reader.hdr.get<int>("nsamples");
        const int in_stride = in_nchans * in_nifs;

        gulp = static_cast<int>(std::ceil(static_cast<double>(gulp) / nadd) *
                                nadd);

        // Copy the header for output. Mutating reader.hdr.nsamples before
        // get_readplan would truncate the input gulp.
        auto out_hdr = reader.hdr;
        out_hdr.set("tsamp", reader.hdr.get<double>("tsamp") *
                                 static_cast<double>(nadd));
        out_hdr.set("nbits", 32);
        if (in_nsamp > 0) {
            out_hdr.set("nsamples", in_nsamp / nadd);
        }

        sigproc::FilterbankWriter writer(outfile, out_hdr);
        std::vector<float> out_arr(
            static_cast<std::size_t>(gulp / nadd * in_stride));
        std::vector<float> block;
        auto plans = reader.get_readplan(gulp);
        reader.seek_sample(0);
        for (const auto& plan : plans) {
            while (true) {
                const auto nread =
                    reader.read_plan(plan.nvalues, block, plan.skip_values);
                if (nread == 0) {
                    break;
                }
                const int nsamps = static_cast<int>(nread / in_stride);
                const int usable = (nsamps / nadd) * nadd;
                if (usable <= 0) {
                    break;
                }
                // Time-only: treat nchans*nifs as the spectral stride
                // (ffactor=1).
                sigproc::kernels::downsample(block, out_arr, nadd, 1, in_stride,
                                             usable);
                writer.write_block(out_arr, (usable / nadd) * in_stride);
                if (!plan.until_eof || nread < plan.nvalues) {
                    break;
                }
            }
        }
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
