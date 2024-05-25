#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <stdexcept>
#include <climits> 

#include <sigproc/io.hpp>

using readplan_tuple = std::tuple<int, int, int>;

FilReader::FilReader(std::string filename) {
    hdr.fromfile(filename);
    nbits       = hdr.get<int>("nbits");
    bitfact     = fileio.bitsinfo.bitfact();
    itemsize    = fileio.bitsinfo.itemsize();
    stride_len  = hdr.get<int>("nchans") * hdr.get<int>("nifs");
    stride_size = stride_len * itemsize / bitfact;
}

FilReader::~FilReader() { delete fileio; }

std::vector<readplan_tuple> FilReader::get_readplan(int gulp, int skipback = 0,
                                                    int start  = 0,
                                                    int nsamps = 0) {
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
        blocks.push_back(
            readplan_tuple(iread, gulp * stride_len, -skipback * stride_len));
    }
    if (lastread != 0) {
        blocks.push_back(readplan_tuple(nreads, lastread * stride_len, 0));
    }
    return blocks;
}

void FilReader::read_plan(int block_len, std::vector<float>& block, int skip) {
    fileio.read_data(block, block_len);
    fileio.seek_bytes(skip * itemsize / bitfact, offset = true);
}

void FilReader::read_block(int start_sample, int nsamps,
                           std::vector<float>& block) {
    seek_sample(start_sample);
    fileio.read_data(block, nsamps * stride_len);
}

void FilReader::seek_sample(int sample) {
    fileio.seek_bytes(hdr.get<int>("header_size") + start_sample * stride_size);
}

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