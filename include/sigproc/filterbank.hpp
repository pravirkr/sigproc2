#pragma once

#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include <sigproc/bits.hpp>
#include <sigproc/common/types.hpp>
#include <sigproc/header.hpp>
#include <sigproc/io.hpp>

namespace sigproc {

/// @brief A single read-plan entry: (read index, block length, skip length).
using ReadPlanTuple = std::tuple<int, int, int>;

/**
 * @brief Reads SIGPROC filterbank data (header followed by samples).
 */
class FilterbankReader {
public:
    explicit FilterbankReader(const std::string& filename);

    /**
     * @brief Build a plan describing how to iterate over the data in blocks.
     *
     * @param gulp     Number of samples to read per block.
     * @param skipback Number of samples to rewind between blocks.
     * @param start    First sample to read.
     * @param nsamps   Total number of samples to read (0 => until the end).
     */
    std::vector<ReadPlanTuple>
    get_readplan(int gulp, int skipback = 0, int start = 0, int nsamps = 0);

    /// @brief Read a planned block of data, then rewind by skip samples.
    void read_plan(int block_len, std::vector<float>& block, int skip);

    /// @brief Read nsamps samples starting from a given sample index.
    void read_block(int start_sample, int nsamps, std::vector<float>& block);

    /// @brief Seek the data stream to the start of a given sample index.
    void seek_sample(int sample);

    /// @brief The parsed SIGPROC header (public for convenient inspection).
    io::SigprocHeader hdr;

private:
    static io::SigprocHeader read_header(const std::string& filename);

    int m_nbits;
    SizeType m_bitfact;
    SizeType m_itemsize;
    SizeType m_stride_len;  // nchans * nifs
    SizeType m_stride_size; // bytes occupied by one sample across the stream
    io::FileIO m_fileio;
};

/**
 * @brief Writes SIGPROC filterbank data (header followed by samples).
 */
class FilterbankWriter {
public:
    FilterbankWriter(const std::string& filename, io::SigprocHeader& hdr);

    /// @brief Write a block of samples to the output file.
    void write_block(const std::vector<float>& block, int block_len);

private:
    int m_nbits;
    bits::BitsInfo m_bitsinfo;
    std::ofstream m_stream;
};

} // namespace sigproc
