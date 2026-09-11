#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

#include <sigproc/header.hpp>

namespace sigproc::fake {

/// Intra-channel smear constant from original `fake.c` (`8.3e3 * DM * foff /
/// fch1³`). Do not use `sigproc::kDispConst` (L&K) here.
inline constexpr double kFakeSmearConst = 8.3e3;

/// Inter-channel delay constant from original `dmdelay.c`.
/// `delay = kFakeDelayConst * (1/f1² − 1/f2²) * DM`.
inline constexpr double kFakeDelayConst = 4148.741601;

/**
 * @brief Configuration for `generate()`.
 *
 * Slow-path defaults match original `fake.c`. Set `fast` (or call
 * `apply_fast_defaults`) for `fast_fake.c` / `sig_fast_fake` defaults.
 * `period_s < 0` and `dm < 0` draw uniform random values (unless `fast`).
 */
struct FakeConfig {
    int nchans    = 128;
    int nbits     = 4;
    int nifs      = 1;
    double tsamp  = 80e-6;
    double tobs   = 10.0;
    double tstart = 50000.0;
    double fch1   = 433.968;
    double foff = -0.062; ///< Already negative; a user-supplied positive value
                          ///< is negated.
    int telescope_id  = 4;
    int machine_id    = 10;
    int nbeams        = 0;
    int ibeam         = 0;
    std::int64_t seed = -1;
    double period_s   = -1.0;
    double dm         = -1.0;
    double snrpeak    = 1.0;
    double duty = 0.04; ///< Pulse width as a duty-cycle fraction (CLI `-width`
                        ///< is percent).
    bool smear  = true;
    bool headerless = false;
    bool evenodd    = false;
    bool fast       = false;
    bool test_mode = false; ///< Fast-fake `--test`: write header, skip payload.
    std::string source_name;
};

/// Defaults applied when `fast==true` / `sig_fast_fake`.
struct FastFakeDefaults {
    static constexpr int nchans                   = 1024;
    static constexpr int nbits                    = 2;
    static constexpr double tsamp                 = 64e-6;
    static constexpr double tobs                  = 270.0;
    static constexpr double tstart                = 56000.0;
    static constexpr double fch1                  = 1581.804688;
    static constexpr double foff                  = -0.390625;
    static constexpr std::string_view source_name = "FAKE";
};

/**
 * @brief Original `dmdelay(f1, f2, dm)`: seconds between sky frequencies in
 * MHz.
 */
[[nodiscard]] inline double
fake_dmdelay(double f1_mhz, double f2_mhz, double dm) noexcept {
    return kFakeDelayConst *
           ((1.0 / (f1_mhz * f1_mhz)) - (1.0 / (f2_mhz * f2_mhz))) * dm;
}

/// Overlay `FastFakeDefaults` and mark the config as the fast (noise-only)
/// path.
void apply_fast_defaults(FakeConfig& cfg);

/**
 * @brief Build the SIGPROC header that `generate()` would write.
 *
 * Resolves evenodd (32-bit), positive-`foff` negation, and `source_name`.
 * Does not draw random period/DM; callers that need those should call
 * `generate()` or set them first.
 */
[[nodiscard]] io::SigprocHeader make_header(const FakeConfig& cfg);

/**
 * @brief Write a synthetic filterbank to `out`.
 *
 * Pulse path (not `fast`): top-hat of width `duty`, Gaussian noise, optional
 * intra-channel smear (`kFakeSmearConst`) and inter-channel `shift[]`
 * (`kFakeDelayConst`). Fast path: quantized Gaussian only (no DM / pulse).
 * Does not write `nsamples` (original `fake.c`). Binary orbit is out of scope.
 */
void generate(std::ostream& out, FakeConfig cfg);

} // namespace sigproc::fake
