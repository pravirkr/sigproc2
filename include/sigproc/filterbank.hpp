#pragma once

#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <stdexcept>
#include <climits>  // CHAR_BIT (bits_per_byte)

#include "fileIO.hpp"
#include "filterbank_header.hpp"
#include "exceptions.hpp"

using readplan_tuple = std::tuple<int, int, int>;

class SigprocFilterbank {
public:
    SigprocHeader hdr;

    /**
     * Default constructor, takes a filename
     */
    SigprocFilterbank(std::string filename) {
        file_stream.open(filename.c_str(),
                         std::ifstream::in | std::ifstream::binary);
        ErrorChecker::check_file_error(file_stream, filename);
        // Read the header
        hdr.read_fromFile(file_stream);

        nbits      = hdr.get<int>("nbits");
        stride_len = hdr.get<int>("nchans") * hdr.get<int>("nifs");

        bitfact     = FileIO::get_bitfact(nbits);
        itemsize    = FileIO::get_itemsize(nbits);
        stride_size = stride_len * itemsize / bitfact;
    }

    /*!
      \brief Deconstruct a SigprocFilterbank object.

      The deconstructor cleans up memory allocated when
      reading data from file.
    */
    ~SigprocFilterbank() { file_stream.close(); }

    std::vector<readplan_tuple> get_readplan(int gulp, int skipback = 0,
                                             int start = 0, int nsamps = 0) {
        if (nsamps == 0) {
            nsamps = hdr.get<int>("nsamples") - start;
        }
        gulp     = std::min(nsamps, gulp);
        skipback = std::abs(skipback);
        if (skipback >= gulp) {
            std::runtime_error("readsamps must be > skipback value");
        }
        int nreads   = (int)nsamps / (gulp - skipback);
        int lastread = nsamps - (nreads * (gulp - skipback));
        if (lastread < skipback) {
            nreads -= 1;
            lastread = nsamps - (nreads * (gulp - skipback));
        }
        std::vector<readplan_tuple> blocks;
        for (int iread = 0; iread < nreads; ++iread) {
            blocks.push_back(readplan_tuple(iread, gulp * stride_len,
                                            -skipback * stride_len));
        }
        if (lastread != 0) {
            blocks.push_back(readplan_tuple(nreads, lastread * stride_len, 0));
        }
        return blocks;
    }

    void readplan(int block_len, std::vector<float>& block, int skip) {
        FileIO::read_data(file_stream, nbits, block, block_len);
        file_stream.seekg(skip * itemsize / bitfact, std::ios_base::cur);
    }

    void readBlock(int start_sample, int nsamps, std::vector<float>& block) {
        seek_sample(start_sample);
        FileIO::read_data(file_stream, nbits, block, nsamps * stride_len);
    }

    void seek_sample(int sample) {
        // get to the right place in the file stream.
        file_stream.seekg(hdr.get<int>("header_size") + sample * stride_size);
    }

private:
    std::ifstream file_stream;
    std::size_t bitfact;
    std::size_t itemsize;
    std::size_t stride_len;
    std::size_t stride_size;
    int nbits;
};

/*
class FilterbankBlock {
public:
    FilterbankBlock(uint64_t start, uint64_t length, const FilFile* filfile)
        : _start(start), _length(length), _filfile(filfile),
          _nchans(filfile->_nchans), _nifs(filfile->_nifs),
          _raw_length(length * _nchans * _nifs) {
        _data = static_cast<float*>(calloc(sizeof(float), _raw_length));
        //  logmsg("Allocate block %lp",_data);
    }

    ~FilterbankBlock() {
        // logmsg("deallocate block %lp",_data);
        free(_data);
    }
    float* _data;

    const int _nchans;
    const int _nifs;

    const uint64_t _start;
    const uint64_t _length;
    const uint64_t _raw_length;
    const FilFile* _filfile;

private:
};  // class FilterbankBlock

*/