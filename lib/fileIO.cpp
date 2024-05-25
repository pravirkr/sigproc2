#include <filesystem>
#include <format>
#include <utility>

#include <sigproc/exceptions.hpp>
#include <sigproc/numbits.hpp>
#include <sigproc/utils.hpp>

#include <sigproc/fileIO.hpp>

namespace fs = std::filesystem;

FileBase::FileBase(const std::vector<std::string>& filenames, std::string mode)
    : filenames(filenames), mode(std::move(mode)) {
    if (filenames.empty()) {
        throw std::invalid_argument("Empty file list");
    }
    openFile(0);
}
FileBase::~FileBase() { closeCurrent(); }

bool FileBase::eos() const {
    // First check if we are at the end of the current file
    bool eof = file_stream.tellg() == fs::file_size(filenames[ifileCur]);
    // Now check if we are at the end of the list of files
    bool eol = ifileCur == filenames.size() - 1;
    return eof && eol;
}

void FileBase::openFile(size_t ifile) {
    if (ifile < 0 || ifile >= filenames.size()) {
        throw std::out_of_range(std::format("Invalid file index: {}", ifile));
    }
    if (!fs::exists(filenames[ifile])) {
        throw std::invalid_argument(
            std::format("File does not exist: {}", filenames[ifile]));
    }

    if (ifile != ifileCur) {
        closeCurrent();
        auto file_mode = MapUtils::get_value(modeMap, mode);
        file_stream.open(filenames[ifile].c_str(), file_mode);
        ErrorChecker::check_stream(file_stream, filenames[ifile]);
        ifileCur = ifile;
    }
}

void FileBase::closeCurrent() {
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

FileIO::FileIO(const std::string& filename, int nbits)
    : nbits(nbits), bitsinfo(nbits) {
    file_stream.open(filename.c_str(),
                     std::ifstream::in | std::ifstream::binary);
    ErrorChecker::check_file(file_stream, filename);
}

FileIO::~FileIO() { file_stream.close(); }

/* read nread units of data from stream */
void FileIO::read_data(std::vector<float>& block, int nread) {
    // decide how to read the data based on the number of bits per sample
    // read n/nbits bytes into character block containing n nbits-bit pairs
    std::vector<uint8_t> buffer(nread * bitsinfo.itemsize());
    file_stream.read(reinterpret_cast<char*>(buffer.data()),
                     buffer.size() / bitsinfo.bitfact());

    if (bitsinfo.packunpack()) {
        sigproc::unpackInPlace(buffer.data(), nbits, buffer.size());
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

    if (bitsinfo.packunpack()) {
        sigproc::packInPlace(buffer.data(), nbits, buffer.size());
    }

    file_stream.write(reinterpret_cast<const char*>(buffer.data()),
                      buffer.size() / bitsinfo.bitfact());
}

/* get to the right place in the file stream. */
void FileIO::seek_bytes(int nbytes, bool offset = false) {
    if (offset) {
        file_stream.seekg(nbytes, std::ios_base::cur);
    } else {
        file_stream.seekg(nbytes);
    }
}
