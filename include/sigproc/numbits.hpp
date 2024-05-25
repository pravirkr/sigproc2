#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

/**
 * @file numbits.hpp
 * @brief Functions for unpacking and packing 1, 2, or 4 bit data from 8 bit
 * bytes
 *
 * nbits:4, pack two integers into a single char containing two 4-bit words
 * first integer passed (i) takes the lower four bits of the char,
 * whilst the second integer passed (j) takes the higher four bits.
 * unpack: reverse operation of the above routine - recovers original packed
 * integers from within a character. i - low ; j - high as above.
 */

namespace sigproc {

/**
 * @brief Unpacks 1, 2, or 4 bit data from 8 bit bytes
 *
 * @param inbuffer Input buffer containing 8 bit bytes
 * @param outbuffer Output buffer to store unpacked data
 * @param nbits Number of bits to unpack
 * @param bitorder Bit order of the input packed data
 * @param parallel Whether to use parallel processing
 */
void unpack(std::span<const uint8_t> inbuffer, std::span<uint8_t> outbuffer,
            size_t nbits, const std::string& bitorder, bool parallel = false);

/**
 * @brief Unpacks 1, 2, or 4 bit data from 8 bit bytes using a lookup table
 *
 * @param inbuffer Input buffer containing 8 bit bytes
 * @param outbuffer Output buffer to store unpacked data
 * @param nbits Number of bits to unpack
 * @param bitorder Bit order of the input packed data
 * @param parallel Whether to use parallel processing
 */
void unpack_lookup(std::span<const uint8_t> inbuffer,
                   std::span<uint8_t> outbuffer, size_t nbits,
                   const std::string& bitorder, bool parallel = false);

/**
 * @brief Unpacks 1, 2, or 4 bit data from 8 bit bytes in place
 *
 * This is done by unpacking the bytes backwards so as not to overwrite any of
 * the data.
 *
 * @param inbuffer Input buffer containing 8 bit bytes to be unpacked
 * @param nbits  Number of bits to unpack
 * @param bitorder  Bit order of the input packed data
 */
void unpack_in_place(std::span<uint8_t> inbuffer, size_t nbits,
                     const std::string& bitorder);

/**
 * @brief Packs 1, 2, or 4 bit data into 8 bit bytes
 *
 * @param inbuffer Input buffer containing 1, 2, or 4 bit data
 * @param outbuffer Output buffer to store packed data
 * @param nbits Number of bits to pack
 * @param bitorder Bit order of the output packed data
 * @param parallel Whether to use parallel processing
 */
void pack(std::span<const uint8_t> inbuffer, std::span<uint8_t> outbuffer,
          size_t nbits, const std::string& bitorder, bool parallel = false);

/**
 * @brief Packs 1, 2, or 4 bit data into 8 bit bytes in place
 *
 * @param buffer Input buffer containing 8 bit bytes to be packed
 * @param nbits Number of bits to pack
 * @param bitorder Bit order of the output packed data
 */
void pack_inplace(std::span<uint8_t> inbuffer, size_t nbits,
                  const std::string& bitorder);

} // namespace sigproc