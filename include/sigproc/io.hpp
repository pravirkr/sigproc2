#pragma once

#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <stdexcept>
#include <climits>  // CHAR_BIT (bits_per_byte)

#include <sigproc/fileIO.hpp>
#include <sigproc/header.hpp>

using readplan_tuple = std::tuple<int, int, int>;

class FilReader {
public:
    FilReader(std::string filename);

    ~FilReader();

    std::vector<readplan_tuple> get_readplan(int gulp, int skipback = 0,
                                             int start = 0, int nsamps = 0);

    void read_plan(int block_len, std::vector<float>& block, int skip);

    void read_block(int start_sample, int nsamps, std::vector<float>& block);

    void seek_sample(int sample);

private:
    std::size_t bitfact;
    std::size_t itemsize;
    std::size_t stride_len;
    std::size_t stride_size;
    int nbits;

    FileReader* fileio;
    SigprocHeader* hdr;
};

class FilterbankWriter {
public:
    FilterbankWriter(std::string filename, SigprocHeader& hdr) {
        nbits = hdr.get<int>("nbits");
        // Write the header
        hdr.tofile(filename);
        FileIO fileio(filename, nbits);
    }

    ~FilterbankWriter() {}

    void write_block(const std::vector<float>& block, int block_len) {
        fileio.write_data(block, block_len);
    }

private:
    int nbits;
};
