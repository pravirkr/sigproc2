/*
 * clip - replace gulp outliers with the gulp median; write a data_type=2
 * time series.
 *
 * Same 1-D gulp as flatten.c / clip.c (`read_block(..., 32768)` values).
 * A sample is replaced when |x - median| > sigma, with sigma the gulp RMS
 * around the mean.
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
        "clip - replace |x-median|>sigma with the gulp median (time series)"};
    sigproc::cli::configure_app(app);
    app.footer(
        "Each gulp is a 1-D time-major vector (default 32768 values). Output "
        "is data_type=2, nchans=1, nbits=32. Omitted -o writes stdout.");

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

        std::vector<float> block;
        const auto nwant = static_cast<sigproc::SizeType>(gulp);
        while (true) {
            const auto nread = reader.read_plan(nwant, block, 0);
            if (nread == 0) {
                break;
            }
            const auto n = static_cast<std::size_t>(nread);
            sigproc::kernels::clip_gulp(std::span<const float>(block.data(), n),
                                        std::span<float>(block.data(), n));
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
