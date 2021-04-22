
#include "fileIO.hpp"

void FileIO::read_data(std::ifstream& stream, int nbits,
                       std::vector<float>& block, int nread) {
    // decide how to read the data based on the number of bits per sample
    // read n/nbits bytes into character block containing n nbits-bit pairs
    std::vector<uint8_t> buffer(nread * FileIO::get_itemsize(nbits));
    stream.read(reinterpret_cast<char*>(buffer.data()),
                buffer.size() / FileIO::get_bitfact(nbits));

    if (nbits == 1 || nbits == 2 || nbits == 4) {
        sigproc::unpackInPlace(buffer.data(), nbits, buffer.size());
    }

    float* buffer_ptr = reinterpret_cast<float*>(buffer.data());

    block.clear();
    block.resize(nread);
    block.assign(buffer_ptr, buffer_ptr + block.size());
}

void FileIO::write_data(std::ofstream& stream, int nbits,
                        const std::vector<float>& block, int nwrite) {
    // decide how to read the data based on the number of bits per sample
    // write n/nbits bytes into character block containing n nbits-bit pairs
    std::vector<uint8_t> buffer(nwrite * sizeof(float)/sizeof(uint8_t));

    const uint8_t* block_ptr = reinterpret_cast<const uint8_t*>(block.data());
    buffer.assign(block_ptr, block_ptr + buffer.size());

    if (nbits == 1 || nbits == 2 || nbits == 4) {
        sigproc::packInPlace(buffer.data(), nbits, buffer.size());
    }

    stream.write(reinterpret_cast<const char*>(buffer.data()),
                 buffer.size() / FileIO::get_bitfact(nbits));
}

std::size_t FileIO::get_itemsize(int nbits) {
    std::size_t itemsize;
    switch (nbits) {
        case 1:
        case 2:
        case 4:
        case 8:
            itemsize = sizeof(uint8_t);
            break;
        case 16:
            itemsize = sizeof(uint16_t);
            break;
        case 32:
            itemsize = sizeof(float);
            break;
    }
    return itemsize;
}

std::size_t FileIO::get_bitfact(int nbits) {
    std::size_t bitfact;
    if (nbits == 1 || nbits == 2 || nbits == 4) {
        bitfact = CHAR_BIT / nbits;
    } else {
        bitfact = 1;
    }
    return bitfact;
}