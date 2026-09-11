/*
 * zerodm - subtract the per-spectrum mean from 8-bit filterbank data.
 *
 * Original `zerodm.c` copies the header bytes, then recenters each spectrum
 * around 64 with a random dither. This rewrite is deterministic (K34):
 * isub = round(mean); sample = clamp(x - isub + 64, 0, 255). Do not claim
 * payload bit-identity with original mjk_rand output.
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/bits.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/kernels.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{
        "zerodm - apply deterministic zerodm to 8-bit filterbank data"};
    sigproc::cli::configure_app(app);
    app.footer("Copies the input header bytes (tstart/nsamples unchanged). "
               "Per-spectrum: isub=round(mean); clamp(x-isub+64, 0, 255). "
               "No dither. Omitted -o writes stdout.");

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    double skip_sec = 0.0;
    app.add_option("-s,--skip", skip_sec, "Skip this many seconds (def=0)");

    std::optional<double> read_sec;
    app.add_option("-r,--read", read_sec,
                   "Read this many seconds (def=all remaining)");

    bool as_float = false;
    app.add_flag("--float", as_float,
                 "Allow nbits!=8: subtract mean, recenter 0, no 8-bit clamp");

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
    if (skip_sec < 0.0) {
        spdlog::error("-s skip must be >= 0");
        return EXIT_FAILURE;
    }

    try {
        sigproc::FilterbankReader reader(filename);
        const int nbits = reader.hdr.get<int>("nbits");
        if (!as_float && nbits != 8) {
            throw std::runtime_error(
                "zerodm requires 8-bit data (use --float for other nbits)");
        }
        const double tsamp = reader.hdr.get<double>("tsamp");
        if (tsamp <= 0.0) {
            throw std::runtime_error("tsamp must be positive");
        }

        const auto skip_samps =
            static_cast<sigproc::SizeType>(std::llround(skip_sec / tsamp));
        sigproc::SizeType read_samps = 0;
        if (read_sec.has_value()) {
            if (*read_sec < 0.0) {
                throw std::runtime_error("-r read must be >= 0");
            }
            read_samps =
                static_cast<sigproc::SizeType>(std::llround(*read_sec / tsamp));
        }

        const auto stride = static_cast<int>(reader.stride_len());
        if (stride <= 0) {
            throw std::runtime_error("nchans*nifs must be > 0");
        }

        const float recenter = as_float ? 0.0F : 64.0F;
        const float clip_lo =
            as_float ? -std::numeric_limits<float>::infinity() : 0.0F;
        const float clip_hi =
            as_float ? std::numeric_limits<float>::infinity() : 255.0F;

        sigproc::cli::OutputStream out(outfile);
        reader.write_raw_header(out.get());

        if (skip_samps > 0) {
            reader.seek_sample(skip_samps);
        }

        const sigproc::bits::BitsInfo info(
            static_cast<sigproc::SizeType>(nbits));
        std::vector<float> block;
        sigproc::SizeType written = 0;
        while (true) {
            auto nwant = static_cast<sigproc::SizeType>(gulp) *
                         static_cast<sigproc::SizeType>(stride);
            if (read_sec.has_value()) {
                if (written >= read_samps) {
                    break;
                }
                const auto remain = read_samps - written;
                const auto cap =
                    remain * static_cast<sigproc::SizeType>(stride);
                nwant = std::min(nwant, cap);
            }
            if (nwant == 0) {
                break;
            }
            const auto nread = reader.read_plan(nwant, block, 0);
            if (nread == 0) {
                break;
            }
            const auto n = static_cast<std::size_t>(nread);
            sigproc::kernels::zerodm_spectra(
                std::span<const float>(block.data(), n),
                std::span<float>(block.data(), n), stride, recenter, clip_lo,
                clip_hi);
            const auto packed_n = sigproc::bits::packed_nbytes(nread, info);
            std::vector<std::byte> packed(packed_n);
            sigproc::bits::from_float(std::span<const float>(block.data(), n),
                                      packed, info);
            out.get().write(reinterpret_cast<const char*>(packed.data()),
                            static_cast<std::streamsize>(packed.size()));
            if (!out.get()) {
                throw std::runtime_error("failed to write zerodm payload");
            }
            written += nread / static_cast<sigproc::SizeType>(stride);
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
