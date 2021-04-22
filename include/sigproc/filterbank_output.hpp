#pragma once

#include <fstream>
#include <vector>

#include "fileIO.hpp"
#include "filterbank_header.hpp"
#include "exceptions.hpp"

class SigprocOutFile {
public:
    SigprocOutFile(std::string filename, SigprocHeader& hdr) {
        file_stream.open(filename.c_str(),
                         std::ofstream::out | std::ofstream::binary);
        ErrorChecker::check_file_error(file_stream, filename);
        // Write the header
        hdr.write_toFile(file_stream);

        nbits = hdr.get<int>("nbits");
    }

    ~SigprocOutFile() { file_stream.close(); }

    void writeBlock(const std::vector<float>& block, int block_len) {
        FileIO::write_data(file_stream, nbits, block, block_len);
    }

private:
    std::ofstream file_stream;
    int nbits;
};