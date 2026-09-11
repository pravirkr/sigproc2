/*
 * chop_fil - chop a filterbank file up in time (byte-copy header and payload)
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <sigproc/filterbank.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{"chop_fil: splits a fil file in time (byte-copy, no tstart "
                 "rewrite)"};
    sigproc::cli::configure_app(app);

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    double skip_sec = 0.0;
    app.add_option("-s,--skip", skip_sec, "Skip this many seconds (def=0)");

    double read_sec = 1.0;
    app.add_option("-r,--read,-t,--total", read_sec,
                   "Read this many seconds (def=1). Does not rewrite tstart.");

    bool force_urandom = false;
    app.add_flag("-f,--force-urandom", force_urandom,
                 "Past EOF, fill remaining payload from /dev/urandom (original "
                 "-f; not a CSPRNG API)");

    int gulp = sigproc::cli::kDefaultGulp;
    sigproc::cli::add_gulp_flag(app, gulp);
    (void)gulp;

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    sigproc::FilterbankReader reader(filename);
    const double tsamp = reader.hdr.get<double>("tsamp");
    if (tsamp <= 0.0) {
        throw std::runtime_error("tsamp must be positive");
    }

    const auto skip_samps =
        static_cast<sigproc::SizeType>(std::llround(skip_sec / tsamp));
    const auto read_samps =
        static_cast<sigproc::SizeType>(std::llround(read_sec / tsamp));

    std::ostream* out = &std::cout;
    std::ofstream owned;
    if (!sigproc::cli::is_stdio_path(outfile)) {
        owned.open(outfile, std::ios::binary);
        if (!owned) {
            throw std::runtime_error("Cannot open output file: " + outfile);
        }
        out = &owned;
    }

    reader.write_raw_header(*out);

    const auto known            = reader.nsamps();
    sigproc::SizeType available = read_samps;
    if (known > 0) {
        const auto remain =
            known > skip_samps ? known - skip_samps : sigproc::SizeType{0};
        if (remain < read_samps) {
            if (!force_urandom) {
                available = remain;
            }
        }
    }

    const auto copy_n =
        (known > 0 && !force_urandom)
            ? std::min(available, known > skip_samps ? known - skip_samps
                                                     : sigproc::SizeType{0})
            : read_samps;
    if (copy_n > 0) {
        try {
            reader.copy_samples(*out, skip_samps, copy_n);
        } catch (const std::runtime_error&) {
            if (!force_urandom) {
                throw;
            }
        }
    }

    if (force_urandom &&
        (known == 0 || skip_samps + copy_n < skip_samps + read_samps)) {
        const auto need = read_samps > copy_n ? read_samps - copy_n : 0;
        if (need > 0) {
            std::cerr << "Past end of file, reading from /dev/urandom\n";
            const auto nbytes =
                static_cast<std::uint64_t>(need) *
                static_cast<std::uint64_t>(reader.stride_len()) *
                1; // packed below
            (void)nbytes;
            const int nbits = reader.hdr.get<int>("nbits");
            const auto stride_bits =
                static_cast<std::uint64_t>(reader.stride_len()) *
                static_cast<std::uint64_t>(nbits);
            const auto bytes_needed = (stride_bits * need + 7) / 8;
            std::ifstream rnd("/dev/urandom", std::ios::binary);
            if (!rnd) {
                throw std::runtime_error("Cannot open /dev/urandom");
            }
            std::vector<char> buf(4096);
            auto remaining = bytes_needed;
            while (remaining > 0) {
                const auto chunk = std::min(remaining, buf.size());
                rnd.read(buf.data(), static_cast<std::streamsize>(chunk));
                const auto got = static_cast<std::size_t>(rnd.gcount());
                if (got == 0) {
                    throw std::runtime_error("Failed to read /dev/urandom");
                }
                out->write(buf.data(), static_cast<std::streamsize>(got));
                remaining -= got;
            }
        }
    }
    return 0;
}
