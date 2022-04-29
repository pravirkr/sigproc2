#pragma once

#include <vector>
#include <fstream>

#include <sigproc/exceptions.hpp>
#include "bitsinfo.hpp"
#include "pack_unpack.hpp"

class FileIO {
public:
    int nbits;
    BitsInfo bitsinfo;
    /**
     * @brief Construct a new File IO object
     *
     * @param filename The name of filename to read/write
     * @param nbits number of bits in the data
     */
    FileIO(std::string filename, int nbits);

    /**
     * @brief Destroy the File IO object
     *
     */
    ~FileIO();

    /* read nread units of data from stream */
    void read_data(std::vector<float>& block, int nread);

    /* write block of data to stream */
    void write_data(const std::vector<float>& block, int nread);

    /* get to the right place in the file stream. */
    void seek_bytes(int nbytes, bool offset = false);

private:
    std::fstream file_stream;
};
