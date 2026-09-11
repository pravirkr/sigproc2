/*
    BANDPASS - outputs the band pass from a filterbank file
*/

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <sigproc/filterbank.hpp>
#include <sigproc/kernels.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{"bandpass - outputs the band pass from a filterbank file"};

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    int ndumps = 0;
    app.add_option("-d,--dumps", ndumps,
                   "Average this many spectra then emit a dump (repeat)");

    double secs_per_dump = 0.0;
    app.add_option("-t,--seconds-per-dump", secs_per_dump,
                   "Set dump length in seconds (ndumps = rint(secs/tsamp))");

    double tstart = 0.0;
    app.add_option("-s,--start", tstart, "Start processing at this time (s)");

    int gulp = sigproc::cli::kDefaultGulp;
    sigproc::cli::add_gulp_flag(app, gulp);

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    sigproc::FilterbankReader reader(filename);
    const double tsamp = reader.hdr.get<double>("tsamp");
    if (secs_per_dump > 0.0) {
        ndumps = static_cast<int>(std::rint(secs_per_dump / tsamp));
    }

    const int nstart = static_cast<int>(std::rint(tstart / tsamp));
    const int nchans = reader.hdr.get<int>("nchans");
    const auto freqs = reader.hdr.get_freq_table();

    std::ostream* out = &std::cout;
    std::ofstream owned;
    if (!sigproc::cli::is_stdio_path(outfile)) {
        owned.open(outfile);
        if (!owned) {
            throw std::runtime_error("Cannot open output file: " + outfile);
        }
        out = &owned;
    }

    std::vector<double> dump_bpass(static_cast<std::size_t>(nchans), 0.0);
    std::vector<double> total_bpass(static_cast<std::size_t>(nchans), 0.0);

    auto emit_dump = [&](const std::vector<double>& bp, int navg, bool tagged) {
        if (navg <= 0) {
            return;
        }
        if (tagged) {
            *out << "#START\n";
        }
        for (int ichan = 0; ichan < nchans; ++ichan) {
            *out << std::format("{:.4f}\t{:.4f}\n",
                                freqs[static_cast<std::size_t>(ichan)],
                                bp[static_cast<std::size_t>(ichan)] /
                                    static_cast<double>(navg));
        }
        if (tagged) {
            *out << "#STOP\n";
        }
    };

    auto plans = reader.get_readplan(gulp, 0, nstart, 0);
    reader.seek_sample(static_cast<sigproc::SizeType>(nstart));

    std::vector<float> block;
    int dump_count  = 0;
    int total_count = 0;
    for (const auto& plan : plans) {
        while (true) {
            const auto nread =
                reader.read_plan(plan.nvalues, block, plan.skip_values);
            if (nread == 0) {
                break;
            }
            const int nsamps = static_cast<int>(nread / reader.stride_len());
            if (nsamps <= 0) {
                break;
            }
            if (ndumps > 0) {
                // Process spectrum-by-spectrum so dumps can end mid-gulp.
                for (int isamp = 0; isamp < nsamps; ++isamp) {
                    std::vector<double> one(static_cast<std::size_t>(nchans),
                                            0.0);
                    sigproc::kernels::get_bpass(
                        std::span<const float>(
                            block.data() +
                                static_cast<std::size_t>(isamp * nchans),
                            static_cast<std::size_t>(nchans)),
                        one, nchans, 1);
                    for (int c = 0; c < nchans; ++c) {
                        dump_bpass[static_cast<std::size_t>(c)] +=
                            one[static_cast<std::size_t>(c)];
                    }
                    ++dump_count;
                    if (dump_count == ndumps) {
                        emit_dump(dump_bpass, ndumps, true);
                        std::fill(dump_bpass.begin(), dump_bpass.end(), 0.0);
                        dump_count = 0;
                    }
                }
            } else {
                sigproc::kernels::get_bpass(block, total_bpass, nchans, nsamps);
                total_count += nsamps;
            }
            if (!plan.until_eof || nread < plan.nvalues) {
                break;
            }
        }
    }

    if (ndumps == 0) {
        emit_dump(total_bpass, total_count, false);
    }
    return 0;
}
