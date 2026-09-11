/*
 * splice - concatenate filterbank files that share a start time along the
 * channel axis. Always writes a FREQUENCY_START table (original splice.c).
 */

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/filterbank.hpp>
#include <sigproc/kernels.hpp>

#include "cli_utils.hpp"

namespace {

[[nodiscard]] double first_channel_mhz(const sigproc::io::SigprocHeader& hdr) {
    const auto freqs = hdr.get_freq_table();
    if (freqs.empty()) {
        throw std::runtime_error("splice: input has no channel frequencies");
    }
    return freqs.front();
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"splice - concatenate filterbank files along the channel axis "
                 "(same tstart, descending fch1)"};
    sigproc::cli::configure_app(app);
    app.footer(
        "Original: splice file1 file2 ... [-o outfile]. Files must share "
        "tstart and nbits and be ordered in descending first-channel "
        "frequency. The output header always uses FREQUENCY_START "
        "(never fch1/foff). Default output is stdout.");

    std::vector<std::string> files;
    app.add_option("files", files,
                   "Input filterbank files (same tstart, descending fch1)")
        ->required()
        ->check(CLI::ExistingFile);

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    int gulp     = sigproc::cli::kDefaultGulp;
    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_gulp_flag(app, gulp);
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    if (gulp <= 0) {
        spdlog::error("gulp must be > 0");
        return EXIT_FAILURE;
    }

    try {
        std::vector<std::unique_ptr<sigproc::FilterbankReader>> readers;
        readers.reserve(files.size());
        for (const auto& path : files) {
            readers.push_back(
                std::make_unique<sigproc::FilterbankReader>(path));
        }

        const auto& first = readers.front()->hdr;
        if (first.get<int>("data_type") != 1) {
            throw std::runtime_error("input data are not in filterbank format");
        }
        const int nbits     = first.get<int>("nbits");
        const int nifs      = first.get<int>("nifs");
        const double tstart = first.get<double>("tstart");
        const double tsamp  = first.get<double>("tsamp");
        double prev_fch1    = first_channel_mhz(first);

        std::vector<int> nchans(readers.size());
        std::vector<double> freqs;
        nchans[0] = first.get<int>("nchans");
        if (nchans[0] <= 0 || nifs <= 0) {
            throw std::runtime_error("splice: nchans and nifs must be > 0");
        }
        {
            const auto table = first.get_freq_table();
            freqs.insert(freqs.end(), table.begin(), table.end());
        }

        sigproc::SizeType out_nsamps = readers.front()->nsamps();
        bool nsamps_known            = out_nsamps > 0;

        for (std::size_t i = 1; i < readers.size(); ++i) {
            const auto& hdr = readers[i]->hdr;
            if (hdr.get<int>("data_type") != 1) {
                throw std::runtime_error(
                    "input data are not in filterbank format");
            }
            if (hdr.get<double>("tstart") != tstart) {
                throw std::runtime_error(
                    "start times in input files are not identical");
            }
            if (hdr.get<int>("nbits") != nbits) {
                throw std::runtime_error(
                    "number of bits per sample in input files not identical");
            }
            if (hdr.get<int>("nifs") != nifs) {
                throw std::runtime_error(
                    "number of IFs in input files not identical");
            }
            if (hdr.get<double>("tsamp") != tsamp) {
                throw std::runtime_error(
                    "sampling times in input files are not identical");
            }
            const double fch1 = first_channel_mhz(hdr);
            if (fch1 > prev_fch1) {
                throw std::runtime_error(
                    "input files not ordered in descending frequency");
            }
            prev_fch1 = fch1;
            nchans[i] = hdr.get<int>("nchans");
            if (nchans[i] <= 0) {
                throw std::runtime_error("splice: nchans must be > 0");
            }
            const auto table = hdr.get_freq_table();
            freqs.insert(freqs.end(), table.begin(), table.end());
            const auto ns = readers[i]->nsamps();
            if (ns == 0) {
                nsamps_known = false;
            } else if (nsamps_known) {
                out_nsamps = std::min(out_nsamps, ns);
            }
        }

        auto out_hdr = first;
        out_hdr.set("data_type", 1);
        out_hdr.set("nbits", nbits);
        out_hdr.set("nifs", nifs);
        out_hdr.set_freq_table(std::move(freqs));
        if (nsamps_known) {
            out_hdr.set("nsamples", static_cast<int>(out_nsamps));
        }

        int out_nchans = 0;
        for (int nc : nchans) {
            out_nchans += nc;
        }

        sigproc::FilterbankWriter writer(outfile, out_hdr);
        const int nfiles = static_cast<int>(readers.size());
        std::vector<std::vector<float>> blocks(
            static_cast<std::size_t>(nfiles));
        std::vector<const float*> ptrs(static_cast<std::size_t>(nfiles));
        std::vector<float> out_block(static_cast<std::size_t>(gulp) *
                                     static_cast<std::size_t>(nifs) *
                                     static_cast<std::size_t>(out_nchans));

        while (true) {
            sigproc::SizeType nsamps_out =
                std::numeric_limits<sigproc::SizeType>::max();
            bool any_eof   = false;
            bool any_short = false;
            for (int i = 0; i < nfiles; ++i) {
                const auto stride =
                    readers[static_cast<std::size_t>(i)]->stride_len();
                const auto nread =
                    readers[static_cast<std::size_t>(i)]->read_plan(
                        static_cast<sigproc::SizeType>(gulp) * stride,
                        blocks[static_cast<std::size_t>(i)], 0);
                if (nread == 0) {
                    any_eof = true;
                    break;
                }
                const auto nsamps_i = nread / stride;
                nsamps_out          = std::min(nsamps_out, nsamps_i);
                if (nsamps_i < static_cast<sigproc::SizeType>(gulp)) {
                    any_short = true;
                }
                ptrs[static_cast<std::size_t>(i)] =
                    blocks[static_cast<std::size_t>(i)].data();
            }
            if (any_eof || nsamps_out == 0 ||
                nsamps_out == std::numeric_limits<sigproc::SizeType>::max()) {
                break;
            }
            const auto out_len = nsamps_out *
                                 static_cast<sigproc::SizeType>(nifs) *
                                 static_cast<sigproc::SizeType>(out_nchans);
            sigproc::kernels::concat_channels(
                std::span<const float* const>(ptrs.data(), ptrs.size()), nchans,
                std::span<float>(out_block.data(), out_len), nifs,
                static_cast<int>(nsamps_out));
            writer.write_block(out_block, static_cast<int>(out_len));
            if (any_short) {
                break;
            }
        }
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
