#include <sigproc/fake.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include <sigproc/bits.hpp>
#include <sigproc/common/params.hpp>
#include <sigproc/common/types.hpp>

namespace sigproc::fake {

namespace {

constexpr int kGulpSpectra = 512;

[[nodiscard]] std::uint32_t seed32(std::int64_t seed) {
    if (seed < 0) {
        seed = static_cast<std::int64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
    return static_cast<std::uint32_t>(seed) ^
           static_cast<std::uint32_t>(seed >> 32);
}

class GasDev {
public:
    explicit GasDev(std::uint32_t seed) : m_rng(seed) {}

    float operator()() {
        if (m_has_spare) {
            m_has_spare = false;
            return static_cast<float>(m_spare);
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
        m_spare          = v * mul;
        m_has_spare      = true;
        return static_cast<float>(u * mul);
    }

    [[nodiscard]] double uniform(double lo, double hi) {
        return lo + (hi - lo) * canonical();
    }

    [[nodiscard]] std::uint8_t u8() {
        return static_cast<std::uint8_t>(m_rng() & 0xFFU);
    }

private:
    [[nodiscard]] double canonical() {
        return std::generate_canonical<double, 53>(m_rng);
    }

    std::mt19937 m_rng;
    bool m_has_spare = false;
    double m_spare   = 0.0;
};

void validate(const FakeConfig& cfg) {
    bits::BitsInfo info(static_cast<SizeType>(cfg.nbits));
    (void)info;
    if (cfg.nchans <= 0 || cfg.nchans > params::kMaxNchans) {
        throw std::invalid_argument(
            std::format("fake: nchans={} out of range", cfg.nchans));
    }
    if (cfg.nifs <= 0) {
        throw std::invalid_argument("fake: nifs must be > 0");
    }
    if (cfg.tsamp <= 0.0) {
        throw std::invalid_argument("fake: tsamp must be > 0");
    }
    if (cfg.tobs < 0.0) {
        throw std::invalid_argument("fake: tobs must be >= 0");
    }
}

[[nodiscard]] FakeConfig resolve(FakeConfig cfg, GasDev* rng) {
    if (cfg.evenodd) {
        cfg.nbits = 32;
        cfg.smear = false;
        cfg.dm    = 0.0;
        cfg.fast  = false;
    }
    if (cfg.foff > 0.0) {
        cfg.foff *= -1.0;
    }
    if (!cfg.fast && rng != nullptr) {
        if (cfg.period_s < 0.0) {
            cfg.period_s = rng->uniform(1.0e-3, 1.0);
        }
        if (cfg.dm < 0.0) {
            cfg.dm = rng->uniform(1.0, 1.0e3);
        }
    }
    if (cfg.fast) {
        cfg.smear    = false;
        cfg.period_s = 0.0;
        cfg.dm       = 0.0;
        if (cfg.source_name.empty()) {
            cfg.source_name = std::string(FastFakeDefaults::source_name);
        }
        if (cfg.nbeams <= 0) {
            cfg.nbeams = 1;
        }
        if (cfg.ibeam <= 0) {
            cfg.ibeam = 1;
        }
    } else if (cfg.source_name.empty()) {
        if (cfg.evenodd) {
            cfg.source_name = "Even-Odd channel test";
        } else {
            cfg.source_name = std::format("P: {:.12f} ms, DM: {:.3f}",
                                          cfg.period_s * 1000.0, cfg.dm);
        }
    }
    validate(cfg);
    return cfg;
}

void write_payload_bytes(std::ostream& out,
                         std::span<const float> analog,
                         const FakeConfig& cfg,
                         float qmin,
                         float qmax) {
    const bits::BitsInfo info(static_cast<SizeType>(cfg.nbits));
    const auto nbits = info.get_nbits();
    if (nbits == 32) {
        out.write(reinterpret_cast<const char*>(analog.data()),
                  static_cast<std::streamsize>(analog.size() * sizeof(float)));
        if (!out.good()) {
            throw std::runtime_error("fake: failed to write 32-bit samples");
        }
        return;
    }

    std::vector<float> digitised(analog.size());
    const float vmax = static_cast<float>(info.get_digi_max());
    if (cfg.fast) {
        // analog already holds digitised units (fast_fake mapping).
        std::copy(analog.begin(), analog.end(), digitised.begin());
    } else {
        const float span = qmax - qmin;
        const float inv  = span > 0.0F ? vmax / span : 0.0F;
        for (std::size_t i = 0; i < analog.size(); ++i) {
            digitised[i] = (analog[i] - qmin) * inv;
        }
    }
    std::vector<std::byte> packed(bits::packed_nbytes(analog.size(), info));
    bits::from_float(digitised, packed, info);
    out.write(reinterpret_cast<const char*>(packed.data()),
              static_cast<std::streamsize>(packed.size()));
    if (!out.good()) {
        throw std::runtime_error("fake: failed to write packed samples");
    }
}

void fill_fast_spectrum(std::span<float> spec, GasDev& rng, int nbits) {
    const bits::BitsInfo info(static_cast<SizeType>(nbits));
    const float mean  = info.get_digi_mean();
    const float scale = info.get_digi_scale();
    for (float& x : spec) {
        const float g = rng();
        switch (nbits) {
        case 1:
            x = mean + scale * g;
            break;
        case 2:
            x = g + 1.5F;
            break;
        case 4:
            x = g * 3.0F + 7.5F;
            break;
        case 8:
            x = g * 24.0F + 96.0F;
            break;
        case 16:
            x = mean + scale * g;
            break;
        default:
            x = g;
            break;
        }
    }
}

} // namespace

void apply_fast_defaults(FakeConfig& cfg) {
    cfg.nchans  = FastFakeDefaults::nchans;
    cfg.nbits   = FastFakeDefaults::nbits;
    cfg.tsamp   = FastFakeDefaults::tsamp;
    cfg.tobs    = FastFakeDefaults::tobs;
    cfg.tstart  = FastFakeDefaults::tstart;
    cfg.fch1    = FastFakeDefaults::fch1;
    cfg.foff    = FastFakeDefaults::foff;
    cfg.fast    = true;
    cfg.smear   = false;
    cfg.evenodd = false;
    cfg.nifs    = 1;
    cfg.nbeams  = 1;
    cfg.ibeam   = 1;
    if (cfg.source_name.empty()) {
        cfg.source_name = std::string(FastFakeDefaults::source_name);
    }
}

io::SigprocHeader make_header(const FakeConfig& cfg_in) {
    FakeConfig cfg = cfg_in;
    if (cfg.evenodd) {
        cfg.nbits = 32;
    }
    if (cfg.foff > 0.0) {
        cfg.foff *= -1.0;
    }
    if (cfg.source_name.empty()) {
        if (cfg.fast) {
            cfg.source_name = std::string(FastFakeDefaults::source_name);
        } else if (cfg.evenodd) {
            cfg.source_name = "Even-Odd channel test";
        } else {
            const double period = cfg.period_s < 0.0 ? 0.0 : cfg.period_s;
            const double dm     = cfg.dm < 0.0 ? 0.0 : cfg.dm;
            cfg.source_name =
                std::format("P: {:.12f} ms, DM: {:.3f}", period * 1000.0, dm);
        }
    }

    io::SigprocHeader hdr;
    hdr.set("source_name", cfg.source_name);
    hdr.set("machine_id", cfg.machine_id);
    hdr.set("telescope_id", cfg.telescope_id);
    if (cfg.nchans > 1) {
        hdr.set("data_type", 1);
    } else {
        hdr.set("data_type", 2);
        hdr.set("refdm", cfg.dm < 0.0 ? 0.0 : cfg.dm);
    }
    hdr.set("fch1", cfg.fch1);
    hdr.set("foff", cfg.foff);
    hdr.set("nchans", cfg.nchans);
    hdr.set("nbits", cfg.nbits);
    hdr.set("tstart", cfg.tstart);
    hdr.set("tsamp", cfg.tsamp);
    hdr.set("nifs", cfg.nifs);
    if (cfg.fast || cfg.nbeams > 0) {
        hdr.set("nbeams", cfg.nbeams > 0 ? cfg.nbeams : 1);
        hdr.set("ibeam", cfg.ibeam > 0 ? cfg.ibeam : 1);
    }
    if (cfg.fast && cfg.nbits == 8) {
        hdr.set("signed", false);
    }
    return hdr;
}

void generate(std::ostream& out, FakeConfig cfg) {
    GasDev rng(seed32(cfg.seed));
    cfg = resolve(std::move(cfg), &rng);
    spdlog::debug("fake: nchans={} nbits={} tsamp={} tobs={} fast={}",
                  cfg.nchans, cfg.nbits, cfg.tsamp, cfg.tobs, cfg.fast);

    if (!cfg.headerless) {
        auto hdr = make_header(cfg);
        hdr.tostream(out);
    }
    if (cfg.test_mode) {
        return;
    }

    const auto nchans = static_cast<SizeType>(cfg.nchans);
    const auto nifs   = static_cast<SizeType>(cfg.nifs);
    const auto stride = nchans * nifs;
    const auto nsamp =
        static_cast<std::int64_t>(std::llround(cfg.tobs / cfg.tsamp));
    if (nsamp <= 0) {
        return;
    }

    double duty = cfg.duty;
    if (cfg.smear && cfg.period_s > 0.0) {
        const double tdm = kFakeSmearConst * cfg.dm * cfg.foff /
                           (cfg.fch1 * cfg.fch1 * cfg.fch1);
        double width     = duty * cfg.period_s;
        width = std::sqrt(tdm * tdm + cfg.tsamp * cfg.tsamp + width * width);
        duty  = width / cfg.period_s;
    }
    const double rising   = 0.5 - duty / 2.0;
    const double trailing = 0.5 + duty / 2.0;

    std::vector<double> shift(nchans, 0.0);
    if (!cfg.fast && !cfg.evenodd && cfg.period_s > 0.0) {
        for (int c = 0; c < cfg.nchans; ++c) {
            shift[static_cast<SizeType>(c)] = fake_dmdelay(
                cfg.fch1, cfg.fch1 + static_cast<double>(c) * cfg.foff, cfg.dm);
        }
    }

    float snr = static_cast<float>(cfg.snrpeak);
    if (!cfg.fast && !cfg.evenodd) {
        snr /= std::sqrt(static_cast<float>(cfg.nchans));
    }
    float qmin = -4.0F;
    float qmax = 4.0F;
    if (snr > 1.0F) {
        qmax *= snr;
    }

    std::vector<float> block(static_cast<SizeType>(kGulpSpectra) * stride);
    std::int64_t done = 0;
    while (done < nsamp) {
        const auto gulp = std::min<std::int64_t>(kGulpSpectra, nsamp - done);
        const auto nval = static_cast<SizeType>(gulp) * stride;
        block.resize(nval);

        if (cfg.evenodd) {
            for (std::int64_t s = 0; s < gulp; ++s) {
                for (SizeType i = 0; i < nifs; ++i) {
                    for (int c = 0; c < cfg.nchans; ++c) {
                        const auto idx = static_cast<SizeType>(s) * stride +
                                         i * nchans + static_cast<SizeType>(c);
                        block[idx]     = static_cast<float>(c % 2);
                    }
                }
            }
        } else if (cfg.fast) {
            for (std::int64_t s = 0; s < gulp; ++s) {
                for (SizeType i = 0; i < nifs; ++i) {
                    auto spec = std::span<float>(
                        block.data() + static_cast<SizeType>(s) * stride +
                            i * nchans,
                        nchans);
                    fill_fast_spectrum(spec, rng, cfg.nbits);
                }
            }
        } else {
            for (std::int64_t s = 0; s < gulp; ++s) {
                const double faketime =
                    static_cast<double>(done + s + 1) * cfg.tsamp;
                for (SizeType i = 0; i < nifs; ++i) {
                    for (int c = 0; c < cfg.nchans; ++c) {
                        const auto idx = static_cast<SizeType>(s) * stride +
                                         i * nchans + static_cast<SizeType>(c);
                        float pulse    = 0.0F;
                        if (cfg.period_s > 0.0) {
                            double phase =
                                (faketime + shift[static_cast<SizeType>(c)]) /
                                cfg.period_s;
                            phase -= std::floor(phase);
                            if (phase >= rising && phase <= trailing) {
                                pulse = snr;
                            }
                        }
                        block[idx] = rng() + pulse;
                    }
                }
            }
        }

        write_payload_bytes(out, std::span<const float>(block.data(), nval),
                            cfg, qmin, qmax);
        done += gulp;
    }
}

} // namespace sigproc::fake
