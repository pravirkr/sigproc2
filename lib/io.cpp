#include <sigproc/io.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sigproc/bits.hpp>

#include "sigproc/exceptions.hpp"
#include "sigproc/header_codec.hpp"

namespace sigproc::io {

namespace {

[[nodiscard]] bool is_stdio_name(const std::string& filename) {
    return filename.empty() || filename == "-";
}

} // namespace

FileIO::FileIO(const std::string& filename, int nbits)
    : m_nbits(static_cast<SizeType>(nbits)),
      m_bitsinfo(static_cast<SizeType>(nbits)) {
    if (is_stdio_name(filename)) {
        m_in       = &std::cin;
        m_seekable = detail::header_codec::is_istream_seekable(std::cin);
        return;
    }
    m_owned_in = std::make_unique<std::ifstream>(
        filename, std::ios::in | std::ios::binary);
    if (!m_owned_in->is_open()) {
        throw std::runtime_error(std::format("Cannot open file: {}", filename));
    }
    error_check::check_file(*m_owned_in, filename);
    m_in       = m_owned_in.get();
    m_seekable = true;
}

FileIO::FileIO(std::istream& in, int nbits)
    : m_nbits(static_cast<SizeType>(nbits)),
      m_bitsinfo(static_cast<SizeType>(nbits)),
      m_in(&in),
      m_seekable(detail::header_codec::is_istream_seekable(in)) {}

FileIO::~FileIO() = default;

bool FileIO::seekable() const noexcept { return m_seekable; }

std::int64_t FileIO::tell_bytes() {
    if (m_in == nullptr) {
        throw std::runtime_error("FileIO has no input stream");
    }
    const auto pos = m_in->tellg();
    if (!*m_in || pos == std::streampos(-1)) {
        m_in->clear();
        throw std::runtime_error("FileIO position is not available");
    }
    return static_cast<std::int64_t>(pos);
}

SizeType FileIO::read_data(std::vector<float>& block, int nread) {
    if (m_in == nullptr) {
        throw std::runtime_error("FileIO has no input stream");
    }
    if (nread <= 0) {
        block.clear();
        return 0;
    }
    const auto want_values = static_cast<SizeType>(nread);
    const auto want_bytes  = bits::packed_nbytes(want_values, m_bitsinfo);
    std::vector<std::byte> packed(want_bytes);
    m_in->read(reinterpret_cast<char*>(packed.data()),
               static_cast<std::streamsize>(want_bytes));
    const auto got_bytes = static_cast<SizeType>(m_in->gcount());
    if (m_in->eof()) {
        m_in->clear(m_in->rdstate() & ~std::ios::failbit);
    }
    const auto got_values = bits::nvalues_from_nbytes(got_bytes, m_bitsinfo);
    const auto nvalues    = std::min(want_values, got_values);
    packed.resize(bits::packed_nbytes(nvalues, m_bitsinfo));
    block.resize(nvalues);
    if (nvalues == 0) {
        return 0;
    }

    const auto nbits = m_bitsinfo.get_nbits();
    if (nbits == 1 || nbits == 2 || nbits == 4) {
        bits::unpack_to_float(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(packed.data()),
                packed.size()),
            block, nbits);
    } else if (nbits == 8) {
        bits::u8_to_float(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(packed.data()), nvalues),
            block);
    } else if (nbits == 16) {
        bits::u16_to_float(
            std::span<const std::uint16_t>(
                reinterpret_cast<const std::uint16_t*>(packed.data()), nvalues),
            block);
    } else if (nbits == 32) {
        bits::f32_copy(
            std::span<const float>(
                reinterpret_cast<const float*>(packed.data()), nvalues),
            block);
    } else {
        throw std::invalid_argument(
            std::format("Unsupported nbits: {}", nbits));
    }
    return nvalues;
}

void FileIO::write_data(const std::vector<float>& block, int nwrite) {
    if (m_out == nullptr) {
        throw std::runtime_error("FileIO has no output stream");
    }
    if (nwrite <= 0) {
        return;
    }
    const auto nvalues = static_cast<SizeType>(nwrite);
    std::vector<std::byte> packed(bits::packed_nbytes(nvalues, m_bitsinfo));
    bits::from_float(std::span<const float>(block.data(), nvalues), packed,
                     m_bitsinfo);
    m_out->write(reinterpret_cast<const char*>(packed.data()),
                 static_cast<std::streamsize>(packed.size()));
    if (!m_out->good()) {
        throw std::runtime_error("Failed to write sample payload");
    }
}

void FileIO::seek_bytes(std::int64_t nbytes, bool offset) {
    if (m_in == nullptr) {
        throw std::runtime_error("FileIO has no input stream");
    }
    if (!m_seekable) {
        if (!offset && nbytes == 0) {
            return;
        }
        if (offset && nbytes == 0) {
            return;
        }
        throw std::runtime_error("Cannot seek on a non-seekable stream");
    }
    if (offset) {
        m_in->seekg(static_cast<std::streamoff>(nbytes), std::ios_base::cur);
    } else {
        m_in->seekg(static_cast<std::streamoff>(nbytes), std::ios_base::beg);
    }
    if (!*m_in) {
        throw std::runtime_error("FileIO seek failed");
    }
}

void FileIO::skip_bytes(std::int64_t nbytes) {
    if (nbytes < 0) {
        seek_bytes(nbytes, true);
        return;
    }
    if (nbytes == 0) {
        return;
    }
    if (m_seekable) {
        seek_bytes(nbytes, true);
        return;
    }
    std::vector<char> discard(static_cast<std::size_t>(
        std::min(nbytes, static_cast<std::int64_t>(1 << 16))));
    auto remaining = nbytes;
    while (remaining > 0) {
        const auto chunk =
            std::min(remaining, static_cast<std::int64_t>(discard.size()));
        m_in->read(discard.data(), static_cast<std::streamsize>(chunk));
        const auto got = static_cast<std::int64_t>(m_in->gcount());
        if (got <= 0) {
            throw std::runtime_error("Unexpected EOF while skipping bytes");
        }
        remaining -= got;
    }
}

void FileIO::copy_bytes(std::ostream& out, std::int64_t nbytes) {
    if (nbytes < 0) {
        throw std::invalid_argument("copy_bytes: negative length");
    }
    std::vector<char> buf(static_cast<std::size_t>(
        std::min(nbytes, static_cast<std::int64_t>(1 << 16))));
    auto remaining = nbytes;
    while (remaining > 0) {
        const auto chunk =
            std::min(remaining, static_cast<std::int64_t>(buf.size()));
        m_in->read(buf.data(), static_cast<std::streamsize>(chunk));
        const auto got = static_cast<std::int64_t>(m_in->gcount());
        if (got <= 0) {
            throw std::runtime_error("Unexpected EOF while copying samples");
        }
        out.write(buf.data(), static_cast<std::streamsize>(got));
        if (!out.good()) {
            throw std::runtime_error("Failed while copying sample payload");
        }
        remaining -= got;
    }
}

} // namespace sigproc::io
