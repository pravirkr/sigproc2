#pragma once

#include <cstdint>

#define HI4BITS 240
#define LO4BITS 15
#define HI2BITS 192
#define UPMED2BITS 48
#define LOMED2BITS 12
#define LO2BITS 3

/*
     pack two integers into a single char containing two 4-bit words
     first integer passed (i) takes the lower four bits of the char,
     whilst the second integer passed (j) takes the higher four bits.
*/

/*
    reverse operation of the above routine - recovers original packed
    integers from within a character. i - low ; j - high as above.
*/

namespace sigproc {

/**
 * Function to unpack 1,2 and 4 bit data
 * data is unpacked into an empty buffer
 * Note: Only unpacks big endian bit ordering
 */
void unpack(uint8_t* inbuffer, uint8_t* outbuffer, int nbits, int nbytes);

/**
 * Function to unpack 1,2 and 4 bit data
 * Data is unpacked into the same buffer. This is done by unpacking the bytes
 * backwards so as not to overwrite any of the data. This is old code that is
 * no longer used should the filterbank reader ever be changed from using
 * np.fromfile this may once again become useful
 * Note: Only set up for big endian bit ordering
 */
void unpackInPlace(uint8_t* buffer, int nbits, int nbytes);

/**
 * Function to pack bit data into an empty buffer
 */
void pack(uint8_t* inbuffer, uint8_t* outbuffer, int nbits, int nbytes);

/**
 * Function to pack bit data into the same buffer
 */
void packInPlace(uint8_t* buffer, int nbits, int nbytes);

}  // namespace sigproc