/*
 * flatten - gulp-median flatten a filterbank (or .tim) to a data_type=2
 * time series.
 *
 * Each gulp is a 1-D time-major vector of `gulp` values (default 32768),
 * not a per-channel sliding window. Scale is the median of the first gulp
 * whose median is nonzero. Original flatten.c left `median0` uninitialized;
 * all-zero input writes zeros instead of that UB.
 */

#include <cstddef>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/filterbank.hpp>
#include <sigproc/kernels.hpp>
#include <sigproc/timeseries.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{
        "flatten - gulp-median flatten to a SIGPROC time series (data_type=2)"};
    sigproc::cli::configure_app(app);
    app.footer(
        "Gulp is a 1-D vector of filterbank values in time-major order "
        "(not a per-channel filter). scale = median of the first gulp with "
        "median != 0; later gulps write (x - median) / scale. All-zero input "
        "writes zeros. Output is data_type=2, nchans=1, nbits=32.");

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    int gulp = sigproc::kernels::kTimGulpValues;
    app.add_option("-g,--gulp", gulp,
                   "Values per gulp (def=32768); 1-D time-major, not spectra");

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    if (gulp <= 0) {
        spdlog::error("gulp must be > 0");
        return EXIT_FAILURE;
    }

    try {
        sigproc::FilterbankReader reader(filename);
        const int nchans = reader.hdr.get<int>("nchans");
        const int nifs   = reader.hdr.get<int>("nifs");
        const int nsamps = reader.hdr.get<int>("nsamples");
        int out_nsamps   = 0;
        if (nsamps > 0 && nchans > 0 && nifs > 0) {
            out_nsamps = nsamps * nchans * nifs;
        }
        auto hdr = reader.hdr;
        if (out_nsamps > 0) {
            hdr.set("nsamples", out_nsamps);
        }
        sigproc::TimeSeries ts(std::move(hdr), {});
        sigproc::cli::OutputStream out(outfile);
        ts.write_header(out.get());

        float scale = 0.0F;
        std::vector<float> block;
        const auto nwant = static_cast<sigproc::SizeType>(gulp);
        while (true) {
            const auto nread = reader.read_plan(nwant, block, 0);
            if (nread == 0) {
                break;
            }
            const auto n       = static_cast<std::size_t>(nread);
            const auto gulp_in = std::span<const float>(block.data(), n);
            const float median = sigproc::kernels::gulp_median(gulp_in);
            if (scale == 0.0F && median != 0.0F) {
                scale = median;
            }
            sigproc::kernels::flatten_gulp(gulp_in, block, median, scale);
            sigproc::TimeSeries::write_samples(
                out.get(), std::span<const float>(block.data(), n));
            if (nread < nwant) {
                break;
            }
        }
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
