#include <sigproc/filterbank.hpp>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sigproc/bits.hpp>

namespace sigproc {

namespace {

[[nodiscard]] bool is_stdio_name(std::string_view name) {
    return name.empty() || name == "-";
}

} // namespace

void FilterbankReader::init_from_stream(std::istream& in) {
    if (!hdr.fromstream(in)) {
        throw std::runtime_error(
            "Input is not a SIGPROC filterbank (missing HEADER_START)");
    }
    m_nbits = hdr.get<int>("nbits");
    const bits::BitsInfo info(static_cast<SizeType>(m_nbits));
    m_bitfact    = info.get_bitfact();
    m_itemsize   = info.get_itemsize();
    m_stride_len = static_cast<SizeType>(hdr.get<int>("nchans")) *
                   static_cast<SizeType>(hdr.get<int>("nifs"));
    if (m_bitfact == 0) {
        throw std::runtime_error("invalid bit packing factor");
    }
    m_stride_size = (m_stride_len * m_itemsize) / m_bitfact;
    m_fileio      = std::make_unique<io::FileIO>(in, m_nbits);
    m_cur_sample  = 0;
}

FilterbankReader::FilterbankReader(const std::string& filename) {
    if (is_stdio_name(filename)) {
        init_from_stream(std::cin);
        return;
    }
    m_owned = std::make_unique<std::ifstream>(filename,
                                              std::ios::in | std::ios::binary);
    if (!m_owned->is_open()) {
        throw std::runtime_error(std::format("Cannot open file: {}", filename));
    }
    init_from_stream(*m_owned);
}

FilterbankReader::FilterbankReader(std::istream& in) { init_from_stream(in); }

SizeType FilterbankReader::nsamps() const {
    const auto n = hdr.get<int>("nsamples");
    return n < 0 ? 0 : static_cast<SizeType>(n);
}

SizeType FilterbankReader::stride_len() const noexcept { return m_stride_len; }

std::vector<ReadPlan>
FilterbankReader::get_readplan(int gulp, int skipback, int start, int nsamps) {
    if (gulp <= 0) {
        throw std::invalid_argument("get_readplan: gulp must be > 0");
    }
    const auto known = this->nsamps();
    if (known == 0 && nsamps <= 0) {
        if (skipback != 0) {
            throw std::runtime_error("skipback is not supported on "
                                     "unknown-length / non-seekable streams");
        }
        ReadPlan plan;
        plan.index       = 0;
        plan.nvalues     = static_cast<SizeType>(gulp) * m_stride_len;
        plan.skip_values = 0;
        plan.until_eof   = true;
        return {plan};
    }

    const auto start_u = start < 0 ? SizeType{0} : static_cast<SizeType>(start);
    SizeType total     = nsamps > 0 ? static_cast<SizeType>(nsamps)
                                    : (known > start_u ? known - start_u : 0);

    auto gulp_u     = static_cast<SizeType>(gulp);
    gulp_u          = std::min(total == 0 ? gulp_u : total, gulp_u);
    skipback        = std::abs(skipback);
    const auto skip = static_cast<SizeType>(skipback);
    if (skip >= gulp_u) {
        throw std::runtime_error("readsamps must be > skipback value");
    }
    const auto step   = gulp_u - skip;
    SizeType nreads   = step == 0 ? 0 : total / step;
    SizeType lastread = total - (nreads * step);
    if (lastread < skip && nreads > 0) {
        nreads -= 1;
        lastread = total - (nreads * step);
    }

    std::vector<ReadPlan> blocks;
    for (SizeType iread = 0; iread < nreads; ++iread) {
        ReadPlan plan;
        plan.index       = static_cast<int>(iread);
        plan.nvalues     = gulp_u * m_stride_len;
        plan.skip_values = -static_cast<std::int64_t>(skip * m_stride_len);
        blocks.push_back(plan);
    }
    if (lastread != 0) {
        ReadPlan plan;
        plan.index       = static_cast<int>(nreads);
        plan.nvalues     = lastread * m_stride_len;
        plan.skip_values = 0;
        blocks.push_back(plan);
    }
    return blocks;
}

SizeType FilterbankReader::read_plan(SizeType nvalues,
                                     std::vector<float>& block,
                                     std::int64_t skip_values) {
    const auto nread = m_fileio->read_data(
        block,
        static_cast<int>(std::min(
            nvalues, static_cast<SizeType>(std::numeric_limits<int>::max()))));
    if (m_stride_len > 0) {
        m_cur_sample += nread / m_stride_len;
    }
    if (skip_values != 0 && nread > 0) {
        const auto bytes =
            (skip_values * static_cast<std::int64_t>(m_itemsize)) /
            static_cast<std::int64_t>(m_bitfact);
        if (skip_values < 0 && !m_fileio->seekable()) {
            throw std::runtime_error(
                "Cannot skip backwards on a non-seekable stream");
        }
        m_fileio->skip_bytes(bytes);
        if (m_stride_len > 0) {
            if (skip_values < 0) {
                const auto back =
                    static_cast<SizeType>(-skip_values) / m_stride_len;
                m_cur_sample =
                    m_cur_sample > back ? m_cur_sample - back : SizeType{0};
            } else {
                m_cur_sample +=
                    static_cast<SizeType>(skip_values) / m_stride_len;
            }
        }
    }
    return nread;
}

void FilterbankReader::read_block(SizeType start_sample,
                                  SizeType nsamps,
                                  std::vector<float>& block) {
    seek_sample(start_sample);
    m_fileio->read_data(block, static_cast<int>(nsamps * m_stride_len));
    m_cur_sample = start_sample + nsamps;
}

void FilterbankReader::seek_sample(SizeType sample) {
    if (sample == m_cur_sample) {
        return;
    }
    const auto dest_bytes =
        static_cast<std::int64_t>(hdr.get<int>("header_size")) +
        static_cast<std::int64_t>(sample) *
            static_cast<std::int64_t>(m_stride_size);
    if (sample < m_cur_sample) {
        if (!m_fileio->seekable()) {
            throw std::runtime_error(
                "Cannot seek backwards on a non-seekable stream");
        }
        m_fileio->seek_bytes(dest_bytes, false);
    } else if (m_fileio->seekable()) {
        m_fileio->seek_bytes(dest_bytes, false);
    } else {
        const auto nbytes = static_cast<std::int64_t>(sample - m_cur_sample) *
                            static_cast<std::int64_t>(m_stride_size);
        m_fileio->skip_bytes(nbytes);
    }
    m_cur_sample = sample;
}

void FilterbankReader::write_raw_header(std::ostream& out) const {
    const auto raw = hdr.raw_header();
    if (raw.empty()) {
        throw std::runtime_error(
            "write_raw_header: header was not read from a stream");
    }
    out.write(reinterpret_cast<const char*>(raw.data()),
              static_cast<std::streamsize>(raw.size()));
    if (!out.good()) {
        throw std::runtime_error("Failed to write raw SIGPROC header");
    }
}

void FilterbankReader::copy_samples(std::ostream& out,
                                    SizeType start_sample,
                                    SizeType nsamps) {
    seek_sample(start_sample);
    const auto nbytes = static_cast<std::int64_t>(nsamps) *
                        static_cast<std::int64_t>(m_stride_size);
    m_fileio->copy_bytes(out, nbytes);
    m_cur_sample = start_sample + nsamps;
}

void FilterbankWriter::write_header(io::SigprocHeader& hdr) {
    hdr.tostream(*m_out);
}

FilterbankWriter::FilterbankWriter(const std::string& filename,
                                   io::SigprocHeader& hdr)
    : m_nbits(hdr.get<int>("nbits")),
      m_bitsinfo(static_cast<SizeType>(m_nbits)) {
    if (is_stdio_name(filename)) {
        m_out = &std::cout;
    } else {
        m_owned = std::make_unique<std::ofstream>(
            filename, std::ios::out | std::ios::binary);
        if (!m_owned->is_open()) {
            throw std::runtime_error(
                std::format("Cannot open file: {}", filename));
        }
        m_out = m_owned.get();
    }
    write_header(hdr);
}

FilterbankWriter::FilterbankWriter(std::ostream& out, io::SigprocHeader& hdr)
    : m_nbits(hdr.get<int>("nbits")),
      m_bitsinfo(static_cast<SizeType>(m_nbits)),
      m_out(&out) {
    write_header(hdr);
}

void FilterbankWriter::write_block(const std::vector<float>& block,
                                   int block_len) {
    if (block_len <= 0) {
        return;
    }
    const auto nvalues = static_cast<SizeType>(block_len);
    std::vector<std::byte> packed(bits::packed_nbytes(nvalues, m_bitsinfo));
    bits::from_float(std::span<const float>(block.data(), nvalues), packed,
                     m_bitsinfo);
    m_out->write(reinterpret_cast<const char*>(packed.data()),
                 static_cast<std::streamsize>(packed.size()));
    if (!m_out->good()) {
        throw std::runtime_error("Failed to write filterbank block");
    }
}

} // namespace sigproc
