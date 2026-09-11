/*
 * filedit - in-place SIGPROC header edits and time-sample zap.
 *
 * Header keys are patched at the same encoded length (strings space-padded
 * or truncated). `--dry-run` prints old vs new and writes nothing. `-o`
 * writes a copy. Original `-n` remains `--src-name` (K7); dry-run is
 * `--dry-run` only so it does not steal that short flag.
 */

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/bits.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>

#include "cli_utils.hpp"

namespace {

enum class ZapMode { Samples, Gaussian, Zero };

[[nodiscard]] std::string format_value(const sigproc::HeaderValue& value) {
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::same_as<T, std::string>) {
                return v;
            } else if constexpr (std::same_as<T, bool>) {
                return v ? "true" : "false";
            } else {
                return std::format("{}", v);
            }
        },
        value);
}

[[nodiscard]] std::vector<std::pair<std::int64_t, std::int64_t>>
parse_tkill(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open the killfile: " + path);
    }
    std::string line;
    std::getline(in, line); // original skips the first line
    std::vector<std::pair<std::int64_t, std::int64_t>> zaps;
    std::int64_t j = 0;
    int good       = 1;
    while (in >> good) {
        if (good == 0) {
            if (!zaps.empty() && zaps.back().second == j) {
                zaps.back().second = j + 1;
            } else {
                zaps.emplace_back(j, j + 1);
            }
        }
        ++j;
    }
    return zaps;
}

[[nodiscard]] std::pair<std::int64_t, std::int64_t>
parse_time_zap(const std::string& spec) {
    std::istringstream ss(spec);
    std::int64_t start = 0;
    std::int64_t end   = 0;
    if (!(ss >> start >> end)) {
        throw std::runtime_error("time-zap must be two integers \"start end\" "
                                 "(0-based, end exclusive)");
    }
    if (end < start) {
        throw std::runtime_error("time-zap end must be >= start");
    }
    return {start, end};
}

void write_spectrum(std::fstream& file,
                    std::int64_t header_size,
                    std::int64_t sample,
                    std::int64_t packed_stride,
                    std::span<const float> spectrum,
                    const sigproc::bits::BitsInfo& info) {
    std::vector<std::byte> packed(
        sigproc::bits::packed_nbytes(spectrum.size(), info));
    sigproc::bits::from_float(spectrum, packed, info);
    file.seekp(header_size + sample * packed_stride, std::ios::beg);
    file.write(reinterpret_cast<const char*>(packed.data()),
               static_cast<std::streamsize>(packed.size()));
    if (!file) {
        throw std::runtime_error("failed to write zapped spectrum");
    }
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"filedit - modify a .fil file in place"};
    sigproc::cli::configure_app(app);
    app.footer("Header edits keep the encoded length (strings pad/truncate). "
               "--dry-run prints old vs new keys and writes nothing. "
               "Omitted -o edits in place. Original -n is --src-name.");

    std::string filename;
    app.add_option("filename", filename, "Filterbank file to edit")
        ->required()
        ->check([](const std::string& path) -> std::string {
            if (sigproc::cli::is_stdio_path(path)) {
                return "filedit requires a filesystem path (not stdin)";
            }
            std::ifstream in(path);
            if (!in) {
                return "File does not exist: " + path;
            }
            return {};
        });

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    bool dry_run = false;
    app.add_flag("--dry-run", dry_run,
                 "Print old vs new header keys and write nothing");

    std::optional<double> new_ra;
    app.add_option("--ra,-r", new_ra, "Modify RA (hhmmss.xxx)");
    std::optional<double> new_dec;
    app.add_option("--dec,-d", new_dec, "Modify Dec (ddmmss.xxx)");
    std::optional<std::string> new_name;
    app.add_option("--src-name,-n", new_name, "Modify the source name");
    std::optional<double> new_tstart;
    app.add_option("--tstart,-T", new_tstart, "Modify the start MJD");
    std::optional<int> new_ibeam;
    app.add_option("--beam,-b", new_ibeam, "Modify the beam number");
    std::optional<int> new_nbeams;
    app.add_option("--nbeams,-B", new_nbeams, "Modify number of beams");
    std::optional<int> new_nchan;
    app.add_option("--nchan,-c", new_nchan, "Modify number of channels");
    std::optional<double> new_fch1;
    app.add_option("--fch1,-f", new_fch1, "Modify fch1");
    std::optional<double> new_foff;
    app.add_option("--foff,-F", new_foff, "Modify foff");
    std::optional<int> new_nbits;
    app.add_option("--nbits,-i", new_nbits, "Modify nbits");
    std::optional<double> new_tsamp;
    app.add_option("--tsamp,-p", new_tsamp, "Modify tsamp (seconds)");

    std::vector<std::string> time_zaps;
    app.add_option("--time-zap,-t", time_zaps,
                   "Zap samples between start and end (\"s e\", 0-based)")
        ->allow_extra_args(false);

    std::string killfile;
    app.add_option("--tkill,-k", killfile,
                   "Kill file: skip first line, then 0/1 per sample");

    float mean  = 0.0F;
    float sigma = 1.0F;
    app.add_option("--mean,-m", mean, "Gaussian replacement mean (def=0)");
    app.add_option("--sigma,-s", sigma, "Gaussian replacement sigma (def=1)");

    ZapMode zap_mode = ZapMode::Samples;
    app.add_flag(
        "--replace-gaussian,-G",
        [&](std::int64_t) { zap_mode = ZapMode::Gaussian; },
        "Replace zapped samples with Gaussian noise");
    app.add_flag(
        "--replace-samples,-S",
        [&](std::int64_t) { zap_mode = ZapMode::Samples; },
        "Replace zapped samples with random other samples (default)");
    app.add_flag(
        "--replace-zero,-Z", [&](std::int64_t) { zap_mode = ZapMode::Zero; },
        "Replace zapped samples with zeros");

    std::int64_t seed = -1;
    app.add_option("--seed", seed, "RNG seed for zap replacement (extra)");

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    try {
        std::map<std::string, sigproc::HeaderValue> updates;
        if (new_ra) {
            updates.emplace("src_raj", *new_ra);
        }
        if (new_dec) {
            updates.emplace("src_dej", *new_dec);
        }
        if (new_name) {
            updates.emplace("source_name", *new_name);
        }
        if (new_tstart) {
            updates.emplace("tstart", *new_tstart);
        }
        if (new_ibeam) {
            updates.emplace("ibeam", *new_ibeam);
        }
        if (new_nbeams) {
            updates.emplace("nbeams", *new_nbeams);
        }
        if (new_nchan) {
            updates.emplace("nchans", *new_nchan);
        }
        if (new_fch1) {
            updates.emplace("fch1", *new_fch1);
        }
        if (new_foff) {
            updates.emplace("foff", *new_foff);
        }
        if (new_nbits) {
            updates.emplace("nbits", *new_nbits);
        }
        if (new_tsamp) {
            updates.emplace("tsamp", *new_tsamp);
        }

        std::vector<std::pair<std::int64_t, std::int64_t>> zaps;
        for (const auto& spec : time_zaps) {
            zaps.push_back(parse_time_zap(spec));
        }
        if (!killfile.empty()) {
            auto killed = parse_tkill(killfile);
            zaps.insert(zaps.end(), killed.begin(), killed.end());
        }

        sigproc::io::SigprocHeader hdr;
        if (!hdr.fromfile(filename)) {
            throw std::runtime_error("not a SIGPROC filterbank: " + filename);
        }

        if (dry_run) {
            for (const auto& [key, value] : updates) {
                if (!hdr.is_present(key)) {
                    throw std::invalid_argument(std::format(
                        "Cannot add header key '{}' (header length must not "
                        "change)",
                        key));
                }
                std::string old_s;
                if (std::holds_alternative<std::string>(value)) {
                    old_s = hdr.get<std::string>(key);
                } else if (std::holds_alternative<double>(value)) {
                    old_s = std::format("{}", hdr.get<double>(key));
                } else if (std::holds_alternative<int>(value)) {
                    old_s = std::format("{}", hdr.get<int>(key));
                } else if (std::holds_alternative<bool>(value)) {
                    old_s = hdr.get<bool>(key) ? "true" : "false";
                }
                const auto new_s = format_value(value);
                if (old_s != new_s) {
                    std::cout << key << ": " << old_s << " -> " << new_s
                              << '\n';
                }
            }
            for (const auto& [s, e] : zaps) {
                std::cout << "zap samples [" << s << ", " << e << ")\n";
            }
            return EXIT_SUCCESS;
        }

        std::string target = filename;
        if (!sigproc::cli::is_stdio_path(outfile)) {
            std::filesystem::copy_file(
                filename, outfile,
                std::filesystem::copy_options::overwrite_existing);
            target = outfile;
        }

        std::vector<std::byte> patched;
        if (!updates.empty()) {
            patched = hdr.patched_raw_header(updates);
            if (patched.size() != hdr.raw_header().size()) {
                throw std::runtime_error("refusing header length change");
            }
        }

        std::fstream file(target,
                          std::ios::in | std::ios::out | std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file '" + target + "'.");
        }
        if (!patched.empty()) {
            file.seekp(0, std::ios::beg);
            file.write(reinterpret_cast<const char*>(patched.data()),
                       static_cast<std::streamsize>(patched.size()));
            if (!file) {
                throw std::runtime_error("failed to write patched header");
            }
            file.flush();
        }

        if (zaps.empty()) {
            return EXIT_SUCCESS;
        }

        const int nbits  = hdr.get<int>("nbits");
        const int nchans = hdr.get<int>("nchans");
        const int nifs   = hdr.get<int>("nifs");
        const auto header_size =
            static_cast<std::int64_t>(hdr.raw_header().size());
        const auto stride =
            static_cast<std::int64_t>(nchans) * static_cast<std::int64_t>(nifs);
        const sigproc::bits::BitsInfo info(
            static_cast<sigproc::SizeType>(nbits));
        const auto packed_stride =
            static_cast<std::int64_t>(sigproc::bits::packed_nbytes(
                static_cast<sigproc::SizeType>(stride), info));
        auto nsamps = static_cast<std::int64_t>(hdr.get<int>("nsamples"));
        if (nsamps <= 0) {
            file.seekg(0, std::ios::end);
            const auto fsize = static_cast<std::int64_t>(file.tellg());
            if (packed_stride > 0 && fsize > header_size) {
                nsamps = (fsize - header_size) / packed_stride;
            }
        }

        std::mt19937 rng(seed >= 0
                             ? static_cast<std::mt19937::result_type>(seed)
                             : std::random_device{}());
        std::normal_distribution<float> gauss(mean, sigma);
        std::uniform_int_distribution<std::int64_t> pick(0, 0);

        constexpr std::int64_t kPool = 32;
        std::vector<float> pool;
        const auto pool_nsamp =
            std::min(kPool, std::max<std::int64_t>(nsamps, 1));
        if (zap_mode == ZapMode::Samples && nsamps > 0) {
            sigproc::FilterbankReader rdr(target);
            std::vector<float> block;
            rdr.read_block(0, static_cast<sigproc::SizeType>(pool_nsamp),
                           block);
            pool = std::move(block);
            pick =
                std::uniform_int_distribution<std::int64_t>(0, pool_nsamp - 1);
        }

        std::vector<float> spectrum(static_cast<std::size_t>(stride));
        for (const auto& [z0, z1] : zaps) {
            const auto start = std::max<std::int64_t>(z0, 0);
            const auto end   = std::min(z1, nsamps);
            for (std::int64_t s = start; s < end; ++s) {
                if (zap_mode == ZapMode::Zero) {
                    std::fill(spectrum.begin(), spectrum.end(), 0.0F);
                } else if (zap_mode == ZapMode::Gaussian) {
                    for (float& x : spectrum) {
                        x = gauss(rng);
                    }
                } else {
                    if (pool.empty()) {
                        std::fill(spectrum.begin(), spectrum.end(), 0.0F);
                    } else {
                        const auto src = pick(rng);
                        const auto off = static_cast<std::size_t>(src * stride);
                        std::copy(pool.begin() +
                                      static_cast<std::ptrdiff_t>(off),
                                  pool.begin() +
                                      static_cast<std::ptrdiff_t>(off + stride),
                                  spectrum.begin());
                    }
                }
                write_spectrum(file, header_size, s, packed_stride, spectrum,
                               info);
            }
        }
    } catch (const std::exception& ex) {
        spdlog::error("{}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
