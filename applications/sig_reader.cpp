/*
 * reader - dump SIGPROC filterbank / time-series samples as ASCII.
 *
 * Original `reader.c` plus readchunk `-t/--time -w/--width` (window around
 * a centre time). `-c` / `-i` are 1-based and repeatable; omitted means all.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/filterbank.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{"reader - look at filterbank data in ASCII format"};
    sigproc::cli::configure_app(app);
    app.footer(
        "filename is the filterbank data file (def=stdin)\n"
        "-c c selects 1-based frequency channels (def=all); repeatable.\n"
        "-i i selects 1-based IF channels (def=all); repeatable.\n"
        "-t/-w (readchunk) keep only samples in [time-width/2, time+width/2].");

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    std::vector<int> chans;
    app.add_option("-c,--chan", chans,
                   "Output only frequency channel c (1...nchans) (def=all)")
        ->allow_extra_args(false);

    std::vector<int> ifs;
    app.add_option("-i,--if", ifs,
                   "Output only IF channel i (1...nifs) (def=all)")
        ->allow_extra_args(false);

    bool numerate = false;
    app.add_flag("-numerate,--numerate", numerate,
                 "Precede each dump with sample number (def=time)");

    bool noindex = false;
    app.add_flag("-noindex,--noindex", noindex,
                 "Do not precede each dump with number/time");

    bool stream = false;
    app.add_flag("-stream,--stream", stream,
                 "Stream of numbers with START/STOP boundaries");

    bool byte_out = false;
    app.add_flag("-byte,--byte", byte_out,
                 "Write samples as truncated bytes (original -byte)");

    std::optional<double> time_of_pulse;
    app.add_option("-t,--time", time_of_pulse,
                   "readchunk: centre time in seconds");

    std::optional<double> width_of_pulse;
    app.add_option("-w,--width", width_of_pulse,
                   "readchunk: window width in seconds");

    int gulp = sigproc::cli::kDefaultGulp;
    sigproc::cli::add_gulp_flag(app, gulp);

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    if (gulp <= 0) {
        spdlog::error("gulp must be > 0");
        return EXIT_FAILURE;
    }
    if (time_of_pulse.has_value() != width_of_pulse.has_value()) {
        spdlog::error("-t/--time and -w/--width must be given together");
        return EXIT_FAILURE;
    }

    try {
        sigproc::FilterbankReader reader(filename);
        const int nchans   = reader.hdr.get<int>("nchans");
        const int nifs     = reader.hdr.get<int>("nifs");
        const double tsamp = reader.hdr.get<double>("tsamp");
        if (nchans <= 0 || nifs <= 0) {
            throw std::runtime_error("nchans and nifs must be > 0");
        }
        if (tsamp <= 0.0) {
            throw std::runtime_error("tsamp must be positive");
        }

        std::vector<char> frchan(static_cast<std::size_t>(nchans), 0);
        std::vector<char> ifchan(static_cast<std::size_t>(nifs), 0);
        for (int c : chans) {
            if (c < 1 || c > nchans) {
                throw std::runtime_error("channel -c is outside 1..nchans");
            }
            frchan[static_cast<std::size_t>(c - 1)] = 1;
        }
        for (int i : ifs) {
            if (i < 1 || i > nifs) {
                throw std::runtime_error("IF -i is outside 1..nifs");
            }
            ifchan[static_cast<std::size_t>(i - 1)] = 1;
        }
        if (chans.empty()) {
            std::fill(frchan.begin(), frchan.end(), 1);
        }
        if (ifs.empty()) {
            std::fill(ifchan.begin(), ifchan.end(), 1);
        }

        const bool indexing = !noindex;
        if (stream && indexing) {
            numerate = true;
        }

        std::optional<double> t0;
        std::optional<double> t1;
        sigproc::SizeType start_sample = 0;
        if (time_of_pulse.has_value()) {
            t0 = *time_of_pulse - *width_of_pulse / 2.0;
            t1 = *time_of_pulse + *width_of_pulse / 2.0;
            if (*t0 > 0.0) {
                start_sample = static_cast<sigproc::SizeType>(
                    std::ceil(*t0 / tsamp - 1e-12));
            }
            if (start_sample > 0) {
                reader.seek_sample(start_sample);
            }
        }

        const int stride = nchans * nifs;
        const auto nwant = static_cast<sigproc::SizeType>(gulp) *
                           static_cast<sigproc::SizeType>(stride);
        std::vector<float> block;
        std::int64_t sample_index = static_cast<std::int64_t>(start_sample);

        while (true) {
            const auto nread = reader.read_plan(nwant, block, 0);
            if (nread == 0) {
                break;
            }
            const auto n = static_cast<int>(nread);
            for (int v = 0; v < n; ++v) {
                const int within  = v % stride;
                const int ifnum   = within / nchans;
                const int chnum   = within % nchans;
                const int dump_i  = v / stride;
                const auto l      = sample_index + dump_i;
                const double time = tsamp * static_cast<double>(l);

                if (t1.has_value() && time > *t1) {
                    return EXIT_SUCCESS;
                }
                const bool in_window =
                    !t0.has_value() || (time >= *t0 && time <= *t1);

                if (byte_out) {
                    const auto byte =
                        static_cast<char>(block[static_cast<std::size_t>(v)]);
                    std::cout.put(byte);
                    continue;
                }

                const bool first_of_dump = within == 0;
                if (first_of_dump && in_window) {
                    if (stream) {
                        std::cout << "#START\n";
                    } else if (indexing) {
                        if (numerate) {
                            std::cout << l << ' ';
                        } else {
                            std::cout << std::format("{:.6f} ", time);
                        }
                    }
                }

                int nsamps_in_dump = within;
                if (ifchan[static_cast<std::size_t>(ifnum)] != 0 &&
                    frchan[static_cast<std::size_t>(chnum)] != 0 && in_window) {
                    if (stream && numerate) {
                        std::cout << nsamps_in_dump << ' ';
                    }
                    std::cout << std::format(
                        "{:.6f} ", block[static_cast<std::size_t>(v)]);
                    if (stream) {
                        std::cout << '\n';
                    }
                }

                if (stream && in_window && ((within + 1) % nchans) == 0) {
                    std::cout << "#STOP\n";
                }
                if (!stream && in_window && within + 1 == stride) {
                    std::cout << '\n';
                }
            }
            sample_index += nread / static_cast<sigproc::SizeType>(stride);
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
