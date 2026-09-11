#pragma once

#include <iosfwd>
#include <span>
#include <string_view>
#include <vector>

#include <sigproc/header.hpp>

namespace sigproc {

/**
 * @brief SIGPROC time series (`data_type=2`, `nchans=1`, `nbits=32`).
 *
 * Flatten / clip / blanker write this shape. `tostream` / `tofile` emit a
 * sparse SIGPROC header followed by native `float32` samples. Filename `""`
 * or `"-"` selects stdout.
 */
class TimeSeries {
public:
    /**
     * @brief Take ownership of a header and sample vector.
     *
     * The stored header is converted to a time-series write-set (`data_type=2`,
     * `nchans=1`, `nbits=32`). Nonempty `samples` sets `nsamples`.
     */
    TimeSeries(io::SigprocHeader hdr, std::vector<float> samples);

    [[nodiscard]] io::SigprocHeader const& hdr() const noexcept;

    [[nodiscard]] std::span<float> samples() noexcept;
    [[nodiscard]] std::span<const float> samples() const noexcept;

    /// @brief Write the time-series header then every stored sample.
    void tostream(std::ostream& out) const;

    /// @brief `""` / `"-"` → `tostream(std::cout)`; otherwise a binary file.
    void tofile(std::string_view path) const;

    /// @brief Write only the SIGPROC header (for gulp-wise streaming).
    void write_header(std::ostream& out) const;

    /// @brief Append native `float32` samples (no header).
    static void write_samples(std::ostream& out,
                              std::span<const float> samples);

private:
    io::SigprocHeader m_hdr;
    std::vector<float> m_samples;
};

/**
 * @brief Build a `data_type=2` header from a filterbank or time-series header.
 *
 * Copies source / telescope / machine / coords / `tstart` / `tsamp` / `fch1`
 * / `refdm` / `nifs` when present. Frequency tables collapse to `fch1` of
 * the first channel. `nsamples > 0` is written; `0` omits the key unless it
 * was already present on `in`.
 */
[[nodiscard]] io::SigprocHeader make_tim_header(const io::SigprocHeader& in,
                                                int nsamples = 0);

} // namespace sigproc
