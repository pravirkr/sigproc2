#pragma once

#include <cstdint>
#include <span>

namespace sigproc::kernels {

void add_channels(std::span<const float> inbuffer,
                  std::span<float> outbuffer,
                  int chan_start,
                  int nchans,
                  int nsamps,
                  int index);

void add_samples(std::span<const float> inbuffer,
                 std::span<double> outbuffer,
                 int nchans,
                 int nsamps,
                 int nifs);

void get_bpass(std::span<const float> inbuffer,
               std::span<double> outbuffer,
               int nchans,
               int nsamps);
void downsample(std::span<const float> inbuffer,
                std::span<float> outbuffer,
                int tfactor,
                int ffactor,
                int nchans,
                int nsamps);

/**
 * @brief Concatenate channel axes of N filterbank gulps.
 *
 * Layout is sample-major `[t][if][chan]`. For each time sample and IF,
 * channels from `inputs[i]` (`nchans[i]` wide) are copied in file order
 * into `out` (`sum(nchans)` wide).
 *
 * @param inputs Pointer to each input gulp (`nsamps * nifs * nchans[i]`).
 * @param nchans Channel count per input file.
 * @param out    Output gulp (`nsamps * nifs * sum(nchans)`).
 * @param nifs   Number of IFs (must match every input).
 * @param nsamps Number of time samples in the gulp.
 */
void concat_channels(std::span<const float* const> inputs,
                     std::span<const int> nchans,
                     std::span<float> out,
                     int nifs,
                     int nsamps);

/**
 * @brief Keep or zero filterbank channels (sample-major `[t][if][chan]`).
 *
 * `keep[c]` is nonzero to retain 0-based channel `c`. When `collapse` is
 * false (original `dice` `force=1`), `out` has the same `nchans` and dropped
 * channels are set to 0. When `collapse` is true, `out` has only the kept
 * channels in increasing channel-index order.
 */
void dice_channels(std::span<const float> in,
                   std::span<float> out,
                   std::span<const int> keep,
                   int nchans,
                   int nifs,
                   int nsamps,
                   bool collapse);

/// Original `flatten.c` / `clip.c` gulp length in **values**, not spectra.
inline constexpr int kTimGulpValues = 32768;

/**
 * @brief Median of a 1-D gulp. Does not mutate `in`.
 *
 * Even-length gulps use the mean of the two central order statistics.
 * Empty input returns 0. This is not Numerical Recipes `nrselect`.
 */
[[nodiscard]] float gulp_median(std::span<const float> in);

/**
 * @brief Gulp-median flatten: `out[i] = (in[i] - median) / scale`.
 *
 * If `scale == 0`, writes zeros (all-zero file / original uninitialized
 * `median0`). `in` and `out` may be the same buffer. Layout is a 1-D
 * time-major vector, not a per-channel sliding window.
 */
void flatten_gulp(std::span<const float> in,
                  std::span<float> out,
                  float median,
                  float scale);

/**
 * @brief Replace values with `|x - median| > sigma` by the gulp median.
 *
 * `sigma` is the gulp RMS around the mean (`sqrt(mean(x²) - mean²)`).
 * `in` and `out` may be the same buffer.
 */
void clip_gulp(std::span<const float> in, std::span<float> out);

/**
 * @brief Pulse phase of 0-based sample `index` (original `blanker.c`).
 *
 * Original increments `turn += tsamp/period` **before** using the sample, so
 * sample 0 has phase `frac(tsamp/period)`.
 */
[[nodiscard]] double
pulse_phase(std::int64_t index, double tsamp, double period);

} // namespace sigproc::kernels
