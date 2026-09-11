#pragma once

#include <array>
#include <iosfwd>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <sigproc/common/types.hpp>
#include <sigproc/header.hpp>

namespace sigproc::io {

inline constexpr std::string_view kFbh5Class   = "FILTERBANK";
inline constexpr std::string_view kFbh5Version = "2.0";
inline constexpr std::string_view kFbh5Dataset = "data";
inline constexpr std::string_view kHdf5NeedsPath =
    "HDF5 requires a filesystem path";
inline constexpr SizeType kFbh5ChunkGulp = 16384;

/// HDF5 signature: `\x89HDF\r\n\x1a\n`.
inline constexpr std::array<unsigned char, 8> kHdf5Magic = {
    0x89, 'H', 'D', 'F', '\r', '\n', 0x1a, '\n'};

[[nodiscard]] bool is_stdio_name(std::string_view name) noexcept;

/// True when `path` ends in `.h5`, `.hdf5`, or `.fbh5` (case-insensitive).
[[nodiscard]] bool is_hdf5_path(std::string_view path) noexcept;

/// True when the file starts with the HDF5 signature.
[[nodiscard]] bool file_has_hdf5_magic(std::string_view path);

/// Extension match, or an existing file that starts with `\x89HDF`.
[[nodiscard]] bool is_hdf5_file(std::string_view path);

/// Seekable peek of the HDF5 signature. Restores the stream position.
/// Non-seekable streams return false (cannot unread).
[[nodiscard]] bool stream_has_hdf5_magic(std::istream& in);

/// Throws `std::invalid_argument` with `kHdf5NeedsPath` for `""` / `"-"`.
void require_hdf5_filesystem_path(std::string_view path);

/**
 * @brief Reads Breakthrough Listen-style FBH5 (`CLASS=FILTERBANK`).
 *
 * Dataset `data` is rank-3 `(time, feed_id, frequency)` by default. Packed
 * 1/2/4-bit files store `uint8` on the last axis plus an `nbits` attribute.
 */
class Hdf5Reader {
public:
    explicit Hdf5Reader(const std::string& filename);
    ~Hdf5Reader();

    Hdf5Reader(const Hdf5Reader&)            = delete;
    Hdf5Reader& operator=(const Hdf5Reader&) = delete;
    Hdf5Reader(Hdf5Reader&&) noexcept;
    Hdf5Reader& operator=(Hdf5Reader&&) noexcept;

    [[nodiscard]] const SigprocHeader& hdr() const noexcept;
    [[nodiscard]] SizeType nsamps() const noexcept;
    [[nodiscard]] SizeType stride_len() const noexcept;
    [[nodiscard]] SizeType stride_bytes() const noexcept;

    /// Convert up to `nvalues` sample-major floats starting at `start_sample`.
    SizeType read_values(SizeType start_sample,
                         SizeType nvalues,
                         std::vector<float>& block);

    /// Packed SIGPROC payload bytes (same gulp packing as `.fil`).
    void copy_packed(std::ostream& out, SizeType start_sample, SizeType nsamps);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/// Incremental FBH5 writer. Time axis is unlimited and chunked.
class Hdf5Writer {
public:
    Hdf5Writer(const std::string& filename, SigprocHeader& hdr);
    ~Hdf5Writer();

    Hdf5Writer(const Hdf5Writer&)            = delete;
    Hdf5Writer& operator=(const Hdf5Writer&) = delete;
    Hdf5Writer(Hdf5Writer&&) noexcept;
    Hdf5Writer& operator=(Hdf5Writer&&) noexcept;

    void write_block(std::span<const float> samples);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sigproc::io
