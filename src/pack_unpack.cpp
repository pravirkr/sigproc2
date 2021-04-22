
#include "pack_unpack.hpp"

void sigproc::unpack(uint8_t* inbuffer, uint8_t* outbuffer, int nbits,
                     int nbytes) {
    switch (nbits) {
        case 1:
            for (int ii = 0; ii < nbytes; ii++) {
                for (int jj = 0; jj < 8; jj++) {
                    outbuffer[(ii * 8) + jj] = (inbuffer[ii] >> jj) & 1;
                }
            }
            break;
        case 2:
            for (int ii = 0; ii < nbytes; ++ii) {
                outbuffer[(ii * 4) + 3] = inbuffer[ii] & LO2BITS;
                outbuffer[(ii * 4) + 2] = (inbuffer[ii] & LOMED2BITS) >> 2;
                outbuffer[(ii * 4) + 1] = (inbuffer[ii] & UPMED2BITS) >> 4;
                outbuffer[(ii * 4) + 0] = (inbuffer[ii] & HI2BITS) >> 6;
            }
            break;
        case 4:
            for (int ii = 0; ii < nbytes; ii++) {
                outbuffer[(ii * 2) + 1] = inbuffer[ii] & LO4BITS;
                outbuffer[(ii * 2) + 0] = (inbuffer[ii] & HI4BITS) >> 4;
            }
            break;
    }
}

void sigproc::unpackInPlace(uint8_t* buffer, int nbits, int nbytes) {
    int pos;
    int lastsamp = nbits * nbytes / 8;
    uint8_t temp;

    switch (nbits) {
        case 1:
            for (int ii = lastsamp - 1; ii > -1; ii--) {
                temp = buffer[ii];
                pos  = ii * 8;
                for (int jj = 0; jj < 8; jj++) {
                    buffer[pos + jj] = (temp >> jj) & 1;
                }
            }
            break;
        case 2:
            for (int ii = lastsamp - 1; ii > -1; ii--) {
                temp            = buffer[ii];
                pos             = ii * 4;
                buffer[pos + 3] = temp & LO2BITS;
                buffer[pos + 2] = (temp & LOMED2BITS) >> 2;
                buffer[pos + 1] = (temp & UPMED2BITS) >> 4;
                buffer[pos + 0] = (temp & HI2BITS) >> 6;
            }
            break;
        case 4:
            for (int ii = lastsamp - 1; ii > -1; ii--) {
                temp            = buffer[ii];
                pos             = ii * 2;
                buffer[pos + 0] = temp & LO4BITS;
                buffer[pos + 1] = (temp & HI4BITS) >> 4;
            }
            break;
    }
}

void sigproc::pack(uint8_t* inbuffer, uint8_t* outbuffer, int nbits,
                   int nbytes) {
    int pos;
    int bitfact = 8 / nbits;
    uint8_t val;

    switch (nbits) {
        case 1:
            for (int ii = 0; ii < nbytes / bitfact; ii++) {
                pos = ii * 8;
                val = (inbuffer[pos + 7] << 7) | (inbuffer[pos + 6] << 6)
                      | (inbuffer[pos + 5] << 5) | (inbuffer[pos + 4] << 4)
                      | (inbuffer[pos + 3] << 3) | (inbuffer[pos + 2] << 2)
                      | (inbuffer[pos + 1] << 1) | inbuffer[pos + 0];
                outbuffer[ii] = val;
            }
            break;
        case 2:
            for (int ii = 0; ii < nbytes / bitfact; ii++) {
                pos = ii * 4;
                val = (inbuffer[pos] << 6) | (inbuffer[pos + 1] << 4)
                      | (inbuffer[pos + 2] << 2) | inbuffer[pos + 3];
                outbuffer[ii] = val;
            }
            break;
        case 4:
            for (int ii = 0; ii < nbytes / bitfact; ii++) {
                pos = ii * 2;
                val = (inbuffer[pos] << 4) | inbuffer[pos + 1];

                outbuffer[ii] = val;
            }
            break;
    }
}

void sigproc::packInPlace(uint8_t* buffer, int nbits, int nbytes) {
    int pos;
    int bitfact = 8 / nbits;
    uint8_t val;

    switch (nbits) {
        case 1:
            for (int ii = 0; ii < nbytes / bitfact; ii++) {
                pos = ii * 8;
                val = (buffer[pos + 7] << 7) | (buffer[pos + 6] << 6)
                      | (buffer[pos + 5] << 5) | (buffer[pos + 4] << 4)
                      | (buffer[pos + 3] << 3) | (buffer[pos + 2] << 2)
                      | (buffer[pos + 1] << 1) | buffer[pos + 0];
                buffer[ii] = val;
            }
            break;
        case 2:
            for (int ii = 0; ii < nbytes / bitfact; ii++) {
                pos = ii * 4;
                val = (buffer[pos] << 6) | (buffer[pos + 1] << 4)
                      | (buffer[pos + 2] << 2) | buffer[pos + 3];
                buffer[ii] = val;
            }
            break;
        case 4:
            for (int ii = 0; ii < nbytes / bitfact; ii++) {
                pos = ii * 2;
                val = (buffer[pos] << 4) | buffer[pos + 1];

                buffer[ii] = val;
            }
            break;
    }
}