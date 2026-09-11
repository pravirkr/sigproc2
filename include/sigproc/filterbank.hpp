#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <sigproc/bits.hpp>
#include <sigproc/common/types.hpp>
#include <sigproc/header.hpp>
#include <sigproc/io.hpp>

namespace sigproc {

/**
 * @brief One gulp of a filterbank read loop.
 *
 * `until_eof` is set when `nsamples` is unknown (stdin / omitted key). The
 * caller then repeats `read_plan` until a short read.
 */
struct ReadPlan {
    int index{};
    SizeType nvalues{};
    std::int64_t skip_values{};
    bool until_eof = false;
};

/**
 * @brief Reads SIGPROC filterbank data (header followed by samples).
 *
 * Filename empty or "-" selects stdin. HDF5 paths are rejected until the
 * FBH5 backend is wired (PR-19).
 */
class FilterbankReader {
public:
    explicit FilterbankReader(const std::string& filename);
    explicit FilterbankReader(std::istream& in);

    /**
     * @brief Build a plan describing how to iterate over the data in blocks.
     *
     * @param gulp     Number of time samples to read per block.
     * @param skipback Number of time samples to rewind between blocks.
     * @param start    First time sample to read (0-based).
     * @param nsamps   Total time samples to read (0 => until the end / EOF).
     */
    std::vector<ReadPlan>
    get_readplan(int gulp, int skipback = 0, int start = 0, int nsamps = 0);

    /**
     * @brief Read a planned block of data, then apply `skip_values`.
     * @return Number of floats converted this call (0 = EOF).
     */
    SizeType read_plan(SizeType nvalues,
                       std::vector<float>& block,
                       std::int64_t skip_values);

    /// @brief Read `nsamps` time samples starting from a 0-based sample index.
    void read_block(SizeType start_sample,
                    SizeType nsamps,
                    std::vector<float>& block);

    /// @brief Seek the data stream to the start of a given sample index.
    void seek_sample(SizeType sample);

    /// Exact bytes of [HEADER_START … HEADER_END] as read. For chop.
    void write_raw_header(std::ostream& out) const;

    /**
     * @brief Copy packed payload samples (no unpack).
     *
     * `start_sample` is 0-based. `sig_extract` (1-based CLI) calls
     * `copy_samples(start_sample - 1, n)`.
     */
    void
    copy_samples(std::ostream& out, SizeType start_sample, SizeType nsamps);

    /// @brief Number of time samples, or 0 if unknown (stdin, no key).
    [[nodiscard]] SizeType nsamps() const;

    /// @brief `nchans * nifs`.
    [[nodiscard]] SizeType stride_len() const noexcept;

    /// @brief The parsed SIGPROC header (public for convenient inspection).
    io::SigprocHeader hdr;

private:
    void init_from_stream(std::istream& in);

    std::unique_ptr<std::ifstream> m_owned;
    std::unique_ptr<io::FileIO> m_fileio;
    int m_nbits{};
    SizeType m_bitfact{};
    SizeType m_itemsize{};
    SizeType m_stride_len{};
    SizeType m_stride_size{};
    SizeType m_cur_sample{};
};

/**
 * @brief Writes SIGPROC filterbank data (header followed by samples).
 *
 * Filename empty or "-" selects stdout (binary).
 */
class FilterbankWriter {
public:
    FilterbankWriter(const std::string& filename, io::SigprocHeader& hdr);
    FilterbankWriter(std::ostream& out, io::SigprocHeader& hdr);

    /// @brief Write `block_len` float samples, quantized to the header nbits.
    void write_block(const std::vector<float>& block, int block_len);

private:
    void write_header(io::SigprocHeader& hdr);

    int m_nbits{};
    bits::BitsInfo m_bitsinfo;
    std::unique_ptr<std::ofstream> m_owned;
    std::ostream* m_out = nullptr;
};

} // namespace sigproc
