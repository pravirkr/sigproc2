/*
 * blanker - blank pulse phases in a 32-bit SIGPROC time series.
 *
 * Original blanker.c: -s/-f phase window, -P constant period (seconds).
 * Polyco (`-p`) is out of scope. On-pulse samples are replaced with
 * Gaussian noise matched to the off-pulse mean/rms of the first 1024
 * off-pulse points.
 */

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
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

namespace {

class GasDev {
public:
    explicit GasDev(std::uint32_t seed) : m_rng(seed) {}

    float operator()() {
        if (m_has_spare) {
            m_has_spare = false;
            return m_spare;
        }
        double u = 0.0;
        double v = 0.0;
        double s = 0.0;
        do {
            u = 2.0 * canonical() - 1.0;
            v = 2.0 * canonical() - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);
        const double mul = std::sqrt(-2.0 * std::log(s) / s);
        m_spare          = static_cast<float>(v * mul);
        m_has_spare      = true;
        return static_cast<float>(u * mul);
    }

private:
    [[nodiscard]] double canonical() {
        return std::generate_canonical<double, 53>(m_rng);
    }

    std::mt19937 m_rng;
    bool m_has_spare = false;
    float m_spare    = 0.0F;
};

constexpr int kOffPulseTarget = 1024;
constexpr int kBlankerGulp    = 8192;

void blank_gulp(std::span<float> samples,
                std::int64_t origin,
                double tsamp,
                double period,
                double phase_start,
                double phase_end,
                float mean,
                float sigma,
                GasDev& noise) {
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double phase = sigproc::kernels::pulse_phase(
            origin + static_cast<std::int64_t>(i), tsamp, period);
        if (phase >= phase_start && phase <= phase_end) {
            samples[i] = noise() * sigma + mean;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"blanker - blank pulse phases in a 32-bit time series"};
    sigproc::cli::configure_app(app);
    app.footer(
        "Requires -s/--phase-start, -f/--phase-end, and -P/--period "
        "(seconds). No polyco. On-pulse samples are replaced with Gaussian "
        "noise from the off-pulse mean/rms of the first 1024 off-pulse "
        "points. Output is data_type=2, nbits=32.");

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    double phase_start = 0.0;
    double phase_end   = 0.0;
    double period      = 0.0;
    app.add_option("-s,--phase-start", phase_start,
                   "Starting pulse phase to blank (0->1)")
        ->required();
    app.add_option("-f,--phase-end", phase_end,
                   "Final pulse phase to blank (0->1)")
        ->required();
    app.add_option("-P,--period", period,
                   "Folding period in seconds (required; no polyco)")
        ->required();

    int gulp = kBlankerGulp;
    app.add_option("-g,--gulp", gulp, "Time samples per gulp (def=8192)");

    std::uint32_t seed = 0;
    app.add_option("--seed", seed, "RNG seed for replacement noise (def=0)");

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    if (phase_start == phase_end) {
        spdlog::error("start and end phases must be different");
        return EXIT_FAILURE;
    }
    if (phase_start > phase_end) {
        spdlog::error("start phase must be less than end phase");
        return EXIT_FAILURE;
    }
    if (period <= 0.0) {
        spdlog::error("period must be > 0");
        return EXIT_FAILURE;
    }
    if (gulp <= 0) {
        spdlog::error("gulp must be > 0");
        return EXIT_FAILURE;
    }

    try {
        sigproc::FilterbankReader reader(filename);
        if (reader.hdr.get<int>("nbits") != 32) {
            throw std::runtime_error(
                "blanker currently only works for 32-bit data");
        }
        const double tsamp = reader.hdr.get<double>("tsamp");
        if (tsamp <= 0.0) {
            throw std::runtime_error("tsamp must be > 0");
        }

        const int nsamps = reader.hdr.get<int>("nsamples");
        auto hdr         = reader.hdr;
        if (nsamps > 0) {
            const int nchans = reader.hdr.get<int>("nchans");
            const int nifs   = reader.hdr.get<int>("nifs");
            hdr.set("nsamples", nsamps * nchans * nifs);
        }
        sigproc::TimeSeries ts(std::move(hdr), {});
        sigproc::cli::OutputStream out(outfile);
        ts.write_header(out.get());

        const auto nwant = static_cast<sigproc::SizeType>(gulp);
        std::vector<float> first;
        const auto nfirst = reader.read_plan(nwant, first, 0);
        if (nfirst == 0) {
            return EXIT_SUCCESS;
        }

        double sum   = 0.0;
        double sumsq = 0.0;
        int noff     = 0;
        for (std::size_t i = 0;
             i < static_cast<std::size_t>(nfirst) && noff < kOffPulseTarget;
             ++i) {
            const double phase = sigproc::kernels::pulse_phase(
                static_cast<std::int64_t>(i), tsamp, period);
            if (phase < phase_start || phase > phase_end) {
                const double x = static_cast<double>(first[i]);
                sum += x;
                sumsq += x * x;
                ++noff;
            }
        }
        float mean  = 0.0F;
        float sigma = 0.0F;
        if (noff > 0) {
            const double inv  = 1.0 / static_cast<double>(noff);
            const double mu   = sum * inv;
            const double mnsq = sumsq * inv;
            const double var  = mnsq - mu * mu;
            mean              = static_cast<float>(mu);
            sigma = var > 0.0 ? static_cast<float>(std::sqrt(var)) : 0.0F;
        }
        spdlog::info("blanker off-pulse mean={} sigma={} ({} samples)", mean,
                     sigma, noff);

        GasDev noise(seed);
        std::int64_t origin = 0;
        blank_gulp(
            std::span<float>(first.data(), static_cast<std::size_t>(nfirst)),
            origin, tsamp, period, phase_start, phase_end, mean, sigma, noise);
        sigproc::TimeSeries::write_samples(
            out.get(), std::span<const float>(
                           first.data(), static_cast<std::size_t>(nfirst)));
        origin += static_cast<std::int64_t>(nfirst);
        if (nfirst < nwant) {
            return EXIT_SUCCESS;
        }

        std::vector<float> block;
        while (true) {
            const auto nread = reader.read_plan(nwant, block, 0);
            if (nread == 0) {
                break;
            }
            const auto n = static_cast<std::size_t>(nread);
            blank_gulp(std::span<float>(block.data(), n), origin, tsamp, period,
                       phase_start, phase_end, mean, sigma, noise);
            sigproc::TimeSeries::write_samples(
                out.get(), std::span<const float>(block.data(), n));
            origin += static_cast<std::int64_t>(nread);
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
