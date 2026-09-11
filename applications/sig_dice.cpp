/*
 * dice - keep or zap filterbank channels.
 *
 * Default (original force=1): zero dropped channels and keep nchans so the
 * file can still be sub-banded. --collapse drops zapped channels (force=0).
 * Keep-file channel numbers are 1-based.
 */

#include <cstdlib>
#include <format>
#include <fstream>
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

[[nodiscard]] std::vector<int> read_keep_mask(const std::string& path,
                                              int nchans) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("keep file does not exist: " + path);
    }
    std::vector<int> keep(static_cast<std::size_t>(nchans), 0);
    int channum = 0;
    while (in >> channum) {
        if (channum < 1 || channum > nchans) {
            throw std::runtime_error(std::format(
                "keep channel {} is outside 1..{}", channum, nchans));
        }
        keep[static_cast<std::size_t>(channum - 1)] = 1;
    }
    if (in.bad()) {
        throw std::runtime_error("failed to read keep file: " + path);
    }
    return keep;
}

void apply_collapse_header(sigproc::io::SigprocHeader& out_hdr,
                           const std::vector<int>& keep,
                           const std::vector<double>& in_freqs,
                           double foff) {
    std::vector<double> kept_freqs;
    kept_freqs.reserve(in_freqs.size());
    for (std::size_t c = 0; c < in_freqs.size(); ++c) {
        if (keep[c] != 0) {
            kept_freqs.push_back(in_freqs[c]);
        }
    }
    if (kept_freqs.empty()) {
        throw std::runtime_error(
            "dice --collapse: keep file selected no channels");
    }
    const double fch1 = kept_freqs.front();
    bool simple       = true;
    for (std::size_t i = 0; i < kept_freqs.size(); ++i) {
        if (kept_freqs[i] != fch1 + static_cast<double>(i) * foff) {
            simple = false;
            break;
        }
    }
    if (simple) {
        out_hdr.set_freq_table({});
        out_hdr.set("fch1", fch1);
        out_hdr.set("foff", foff);
        out_hdr.set("nchans", static_cast<int>(kept_freqs.size()));
    } else {
        out_hdr.set_freq_table(std::move(kept_freqs));
    }
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"dice - keep listed filterbank channels (1-based keep file)"};
    sigproc::cli::configure_app(app);
    app.footer("Original: dice filterbank_file keep_file. The keep file is a "
               "1-based list of channels to retain. Default zeros dropped "
               "channels and keeps nchans (original force=1). --collapse drops "
               "zapped channels. Default output is stdout.");

    std::string filename;
    std::string keepfile;
    app.add_option("filename", filename, "Input filterbank file")
        ->required()
        ->check([](const std::string& path) -> std::string {
            if (sigproc::cli::is_stdio_path(path)) {
                return {};
            }
            std::ifstream in(path);
            if (!in) {
                return "File does not exist: " + path;
            }
            return {};
        });
    app.add_option("keepfile,--keep-file", keepfile,
                   "1-based list of channels to keep")
        ->required()
        ->check(CLI::ExistingFile);

    bool collapse = false;
    app.add_flag("--collapse", collapse,
                 "Drop zapped channels instead of writing zeros (original "
                 "force=0)");

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
        sigproc::FilterbankReader reader(filename);
        if (reader.hdr.get<int>("data_type") != 1) {
            throw std::runtime_error("input data are not in filterbank format");
        }
        const int nchans = reader.hdr.get<int>("nchans");
        const int nifs   = reader.hdr.get<int>("nifs");
        if (nchans <= 0 || nifs <= 0) {
            throw std::runtime_error("dice: nchans and nifs must be > 0");
        }
        const auto keep = read_keep_mask(keepfile, nchans);

        auto out_hdr = reader.hdr;
        if (collapse) {
            apply_collapse_header(out_hdr, keep, reader.hdr.get_freq_table(),
                                  reader.hdr.get<double>("foff"));
        }

        const int out_nchans = out_hdr.get<int>("nchans");
        sigproc::FilterbankWriter writer(outfile, out_hdr);

        std::vector<float> block;
        std::vector<float> out_block(static_cast<std::size_t>(gulp) *
                                     static_cast<std::size_t>(nifs) *
                                     static_cast<std::size_t>(out_nchans));
        const auto stride = reader.stride_len();
        auto plans        = reader.get_readplan(gulp);
        reader.seek_sample(0);
        for (const auto& plan : plans) {
            while (true) {
                const auto nread =
                    reader.read_plan(plan.nvalues, block, plan.skip_values);
                if (nread == 0) {
                    break;
                }
                const int nsamps   = static_cast<int>(nread / stride);
                const auto out_len = static_cast<std::size_t>(nsamps) *
                                     static_cast<std::size_t>(nifs) *
                                     static_cast<std::size_t>(out_nchans);
                sigproc::kernels::dice_channels(
                    std::span<const float>(block.data(), nread),
                    std::span<float>(out_block.data(), out_len), keep, nchans,
                    nifs, nsamps, collapse);
                writer.write_block(out_block, static_cast<int>(out_len));
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
