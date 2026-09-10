#include "sigproc/filterbank.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sigproc {

namespace {
// Default bit order used when packing sub-byte samples on write.
constexpr std::string_view kBitOrder = "big";
} // namespace

io::SigprocHeader FilterbankReader::read_header(const std::string& filename) {
    io::SigprocHeader header;
    header.fromfile(filename);
    return header;
}

FilterbankReader::FilterbankReader(const std::string& filename)
    : hdr(read_header(filename)),
      m_nbits(hdr.get<int>("nbits")),
      m_bitfact(bits::BitsInfo(static_cast<SizeType>(m_nbits)).get_bitfact()),
      m_itemsize(bits::BitsInfo(static_cast<SizeType>(m_nbits)).get_itemsize()),
      m_stride_len(static_cast<SizeType>(hdr.get<int>("nchans")) *
                   static_cast<SizeType>(hdr.get<int>("nifs"))),
      m_stride_size(m_bitfact == 0 ? 0 : m_stride_len * m_itemsize / m_bitfact),
      m_fileio(filename, m_nbits) {
    // Position the data stream at the first sample (just past the header).
    seek_sample(0);
}

std::vector<ReadPlanTuple>
FilterbankReader::get_readplan(int gulp, int skipback, int start, int nsamps) {
    if (nsamps == 0) {
        nsamps = hdr.get<int>("nsamples") - start;
    }
    gulp     = std::min(nsamps, gulp);
    skipback = std::abs(skipback);
    if (skipback >= gulp) {
        throw std::runtime_error("readsamps must be > skipback value");
    }
    int nreads   = nsamps / (gulp - skipback);
    int lastread = nsamps - (nreads * (gulp - skipback));
    if (lastread < skipback) {
        nreads -= 1;
        lastread = nsamps - (nreads * (gulp - skipback));
    }

    std::vector<ReadPlanTuple> blocks;
    const int stride = static_cast<int>(m_stride_len);
    for (int iread = 0; iread < nreads; ++iread) {
        blocks.emplace_back(iread, gulp * stride, -skipback * stride);
    }
    if (lastread != 0) {
        blocks.emplace_back(nreads, lastread * stride, 0);
    }
    return blocks;
}

void FilterbankReader::read_plan(int block_len,
                                 std::vector<float>& block,
                                 int skip) {
    m_fileio.read_data(block, block_len);
    const int bitfact = static_cast<int>(m_bitfact);
    const int skip_bytes =
        bitfact == 0 ? 0 : skip * static_cast<int>(m_itemsize) / bitfact;
    m_fileio.seek_bytes(skip_bytes, true);
}

void FilterbankReader::read_block(int start_sample,
                                  int nsamps,
                                  std::vector<float>& block) {
    seek_sample(start_sample);
    m_fileio.read_data(block, nsamps * static_cast<int>(m_stride_len));
}

void FilterbankReader::seek_sample(int sample) {
    const int header_size = hdr.get<int>("header_size");
    const int offset =
        header_size +
        static_cast<int>(static_cast<SizeType>(sample) * m_stride_size);
    m_fileio.seek_bytes(offset, false);
}

FilterbankWriter::FilterbankWriter(const std::string& filename,
                                   io::SigprocHeader& hdr)
    : m_nbits(hdr.get<int>("nbits")),
      m_bitsinfo(static_cast<SizeType>(m_nbits)),
      m_stream(filename, std::ios::out | std::ios::binary) {
    if (!m_stream.is_open()) {
        throw std::runtime_error(std::format("Cannot open file: {}", filename));
    }
    // Write the header first; the stream stays open for the sample data.
    hdr.tostream(m_stream);
}

void FilterbankWriter::write_block(const std::vector<float>& block,
                                   int block_len) {
    std::vector<uint8_t> buffer(static_cast<SizeType>(block_len) *
                                sizeof(float));
    const auto* block_ptr = reinterpret_cast<const uint8_t*>(block.data());
    buffer.assign(block_ptr, block_ptr + buffer.size());

    if (m_bitsinfo.get_can_pack_unpack()) {
        bits::pack_inplace(buffer, static_cast<SizeType>(m_nbits),
                           std::string(kBitOrder));
    }
    m_stream.write(
        reinterpret_cast<const char*>(buffer.data()),
        static_cast<std::streamsize>(buffer.size() / m_bitsinfo.get_bitfact()));
}

} // namespace sigproc
