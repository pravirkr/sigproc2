
#include "fileIO.hpp"

FileIO::FileIO(std::string filename, int nbits)
    : nbits(nbits), bitsinfo(nbits) {
    file_stream.open(filename.c_str(),
                     std::ifstream::in | std::ifstream::binary);
    ErrorChecker::check_file(file_stream, filename);
}

FileIO::~FileIO() { file_stream.close(); }

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

void FileIO::seek_bytes(int nbytes, bool offset) {
    if (offset) {
        file_stream.seekg(nbytes, std::ios_base::cur);
    } else {
        file_stream.seekg(nbytes);
    }
}
