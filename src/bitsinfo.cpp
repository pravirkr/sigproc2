
#include "bitsinfo.hpp"

BitsInfo::BitsInfo(int nbits) {
    ErrorChecker::check_bits(nbits);
    _nbits = nbits;
}

std::size_t BitsInfo::itemsize() const {
    std::size_t itemsize;
    switch (_nbits) {
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

bool BitsInfo::packunpack() const {
    if (_nbits == 1 || _nbits == 2 || _nbits == 4) {
        return true;
    } else {
        return false;
    }
}

int BitsInfo::bitfact() const {
    int bitfact;
    if (BitsInfo::packunpack()) {
        bitfact = CHAR_BIT / _nbits;
    } else {
        bitfact = 1;
    }
    return bitfact;
}

int BitsInfo::digi_min() const {
    if (_nbits == 32) {
        throw std::invalid_argument("No digi_min value for 32 bit.");
    }
    return 0;
}

int BitsInfo::digi_max() const {
    if (_nbits == 32) {
        throw std::invalid_argument("No digi_max value for 32 bit.");
    }
    return (1 << _nbits) - 1;
}

float BitsInfo::digi_mean() const {
    if (_nbits == 32) {
        throw std::invalid_argument("No digi_mean value for 32 bit.");
    }
    return (1 << (_nbits - 1)) - 0.5;
}

float BitsInfo::digi_scale() const {
    if (_nbits == 32) {
        throw std::invalid_argument("No digi_scale value for 32 bit.");
    }
    return BitsInfo::digi_mean() / self.digi_sigma;
}