
#pragma once
#include <sigproc/filfile.hpp>

class OutputFilFile : public FilFile {
public:
    OutputFilFile(const char* filename) : FilFile(filename) {}
    OutputFilFile(const char* filename, const FilFile& copy) : FilFile(copy) {
        _filename = filename;
    }

    void initialise() {
        thisToGlobal();
        // enable all IFs for writing
        for (int i = 0; i < _nifs; ++i) {
            ifstream[i] = 'Y';
        }
        obits    = _nbits;
        _rawfile = fopen(_filename, "wb");
        filterbank_header(_rawfile);
    }

    void writeBlock(const FilterbankBlock* block) {
        switch (_nbits) {
            case 32:
                fwrite(block->_data, sizeof(float), block->_raw_length,
                       _rawfile);
                break;
            default:
                logerr("Unfortunately writing %d data not compatible with this "
                       "code",
                       _nbits);
                std::exit(1);
                break;
        }
        _current_sample += block->_length;
    }

    FilterbankBlock* createBlock(int length) {
        return new sigproc::FilterbankBlock(_current_sample, length, this);
    }

    void setNbits(int nb) { _nbits = nb; }

    bool expandTo(const FilFile& other) {
        if (other._foff != _foff)
            return false;
        if (other._tsamp != _tsamp)
            return false;
        if (other._tstart != _tstart)
            return false;
        if (other._foff * _foff < 0)
            return false;

        double start_chan = (other._fch1 - _fch1) / _foff;
        if (fabs(start_chan - round(start_chan)) > 0.01) {
            return false;
        }

        int istart_chan = (int)(round(start_chan));

        if (start_chan < 0) {
            // we need to start at an earlier channel.
            _fch1 += start_chan * _foff;
            _nchans -= start_chan;
        }

        double end_chan
            = (other._fch1 + other._nchans * other._foff - _fch1) / _foff;
        if (fabs(end_chan - round(end_chan)) > 0.01) {
            return false;
        }

        int iend_chan = (int)(round(end_chan));
        if (iend_chan > _nchans) {
            _nchans = iend_chan;
        }

        return true;
    }

private:
    int _current_sample;

};  // class OutputFilFile