#pragma once

#include <climits>  // CHAR_BIT (bits_per_byte)

#include <sigproc/exceptions.hpp>

class BitsInfo {
public:
    BitsInfo(int nbits);

    std::size_t itemsize() const;

    bool packunpack() const;

    int bitfact() const;

    int digi_min() const;

    int digi_max() const;

    float digi_mean() const;

    float digi_scale() const;

private:
    int _nbits;
};
