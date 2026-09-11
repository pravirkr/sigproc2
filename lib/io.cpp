#include <sigproc/io.hpp>

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <sigproc/bits.hpp>

#include "sigproc/exceptions.hpp"
#include "sigproc/utils.hpp"

namespace sigproc::io {

namespace fs = std::filesystem;

FileBase::FileBase(const std::vector<std::string>& filenames, std::string mode)
    : m_filenames(filenames),
      m_mode(std::move(mode)) {
    if (filenames.empty()) {
        throw std::invalid_argument("Empty file list");
    }
    open_file(0);
}
FileBase::~FileBase() { close_current(); }

bool FileBase::eos() {
    // First check if we are at the end of the current file
    bool eof = m_file_stream.tellg() == fs::file_size(m_filenames[m_ifileCur]);
    // Now check if we are at the end of the list of files
    bool eol = m_ifileCur == m_filenames.size() - 1;
    return eof && eol;
}

void FileBase::open_file(size_t ifile) {
    if (ifile < 0 || ifile >= m_filenames.size()) {
        throw std::out_of_range(std::format("Invalid file index: {}", ifile));
    }
    if (!fs::exists(m_filenames[ifile])) {
        throw std::invalid_argument(
            std::format("File does not exist: {}", m_filenames[ifile]));
    }

    if (ifile != m_ifileCur) {
        close_current();
        auto file_mode = detail::map_utils::get_value(m_modeMap, m_mode);
        m_file_stream.open(m_filenames[ifile].c_str(), file_mode);
        error_check::check_stream(m_file_stream, m_filenames[ifile]);
        m_ifileCur = ifile;
    }
}

void FileBase::close_current() {
    if (m_file_stream.is_open()) {
        m_file_stream.close();
    }
}

FileIO::FileIO(const std::string& filename, int nbits)
    : m_nbits(static_cast<SizeType>(nbits)),
      m_bitsinfo(static_cast<SizeType>(nbits)) {
    m_file_stream.open(filename.c_str(),
                       std::ifstream::in | std::ifstream::binary);
    error_check::check_file(m_file_stream, filename);
}

FileIO::~FileIO() { m_file_stream.close(); }

// Default bit order used when packing/unpacking sub-byte samples.
namespace {
constexpr std::string_view kBitOrder = "big";
} // namespace

/* read nread units of data from stream */
void FileIO::read_data(std::vector<float>& block, int nread) {
    // decide how to read the data based on the number of bits per sample
    // read n/nbits bytes into character block containing n nbits-bit pairs
    std::vector<uint8_t> buffer(nread * m_bitsinfo.get_itemsize());
    m_file_stream.read(
        reinterpret_cast<char*>(buffer.data()),
        static_cast<std::streamsize>(buffer.size() / m_bitsinfo.get_bitfact()));

    if (m_bitsinfo.get_can_pack_unpack()) {
        bits::unpack_in_place(buffer, m_nbits, std::string(kBitOrder));
    }

    float* buffer_ptr = reinterpret_cast<float*>(buffer.data());

    block.clear();
    block.resize(nread);
    block.assign(buffer_ptr, buffer_ptr + block.size());
}

/* write block of data to stream */
void FileIO::write_data(const std::vector<float>& block, int nwrite) {
    // decide how to read the data based on the number of bits per sample
    // write n/nbits bytes into character block containing n nbits-bit pairs
    std::vector<uint8_t> buffer(nwrite * sizeof(float) / sizeof(uint8_t));

    const uint8_t* block_ptr = reinterpret_cast<const uint8_t*>(block.data());
    buffer.assign(block_ptr, block_ptr + buffer.size());

    if (m_bitsinfo.get_can_pack_unpack()) {
        bits::pack_inplace(buffer, m_nbits, std::string(kBitOrder));
    }

    m_file_stream.write(
        reinterpret_cast<const char*>(buffer.data()),
        static_cast<std::streamsize>(buffer.size() / m_bitsinfo.get_bitfact()));
}

/* get to the right place in the file stream. */
void FileIO::seek_bytes(int nbytes, bool offset) {
    if (offset) {
        m_file_stream.seekg(nbytes, std::ios_base::cur);
    } else {
        m_file_stream.seekg(nbytes);
    }
}

} // namespace sigproc::io