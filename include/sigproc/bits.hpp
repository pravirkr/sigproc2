#pragma once

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <sigproc/common/types.hpp>

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

namespace sigproc::bits {

class BitsInfo {
public:
    /**
     * @brief Construct a BitsInfo object for a specific bit width
     * @param nbits Number of bits (must be 1, 2, 4, 8, 16, or 32)
     * @throws std::invalid_argument if nbits is not supported
     */
    explicit BitsInfo(SizeType nbits);

    // --- Getters ---
    /// @brief Get the number of bits in this configuration.
    constexpr SizeType get_nbits() const noexcept { return m_nbits; }
    /// @brief Storage size in bytes for this bit configuration.
    SizeType get_itemsize() const noexcept;
    /// @brief Check if this bit configuration requires pack/unpack operations.
    constexpr bool get_can_pack_unpack() const noexcept {
        return m_nbits == 1 || m_nbits == 2 || m_nbits == 4;
    }
    /// @brief Get the bit packing factor (items per byte).
    constexpr SizeType get_bitfact() const noexcept {
        return get_can_pack_unpack() ? CHAR_BIT / m_nbits : 1;
    }
    /// @brief Get the minimum digitised value.
    static constexpr SizeType get_digi_min() noexcept { return 0; }
    /// @brief Get the maximum digitised value for this bit width.
    constexpr SizeType get_digi_max() const noexcept {
        return (1U << m_nbits) - 1;
    }
    /// @brief Get the mean digitised value for this bit configuration.
    float get_digi_mean() const noexcept;
    /// @brief Get the digitised scaling factor.
    float get_digi_scale() const noexcept;
    /// @brief Get the digitised sigma (standard deviation).
    float get_digi_sigma() const noexcept;

private:
    SizeType m_nbits;
    IndexType m_attr_index;

    struct Attribute {
        SizeType itemsize;
        float digi_sigma;
    };

    static constexpr std::array<SizeType, 6> kValidNbits = {1, 2, 4, 8, 16, 32};
    static constexpr std::array<Attribute, 6> kAttributes = {{
        {.itemsize = sizeof(uint8_t), .digi_sigma = 0.5F},  // 1-bit
        {.itemsize = sizeof(uint8_t), .digi_sigma = 1.5F},  // 2-bit
        {.itemsize = sizeof(uint8_t), .digi_sigma = 6.0F},  // 4-bit
        {.itemsize = sizeof(uint8_t), .digi_sigma = 6.0F},  // 8-bit
        {.itemsize = sizeof(uint16_t), .digi_sigma = 6.0F}, // 16-bit
        {.itemsize = sizeof(float), .digi_sigma = 6.0F}     // 32-bit
    }};

    static constexpr IndexType nbits_to_index(SizeType nbits) noexcept {
        for (SizeType i = 0; i < kValidNbits.size(); ++i) {
            if (kValidNbits[i] == nbits) {
                return static_cast<IndexType>(i);
            }
        }
        return -1;
    }
};

/**
 * @brief Unpacks 1, 2, or 4 bit data from 8 bit bytes
 *
 * @param inbuffer Input buffer containing 8 bit bytes
 * @param outbuffer Output buffer to store unpacked data
 * @param nbits Number of bits to unpack
 * @param bitorder Bit order of the input packed data
 * @param parallel Whether to use parallel processing
 */
void unpack(std::span<const uint8_t> inbuffer,
            std::span<uint8_t> outbuffer,
            size_t nbits,
            const std::string& bitorder,
            bool parallel = false);

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
                   std::span<uint8_t> outbuffer,
                   size_t nbits,
                   const std::string& bitorder,
                   bool parallel = false);

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
void unpack_in_place(std::span<uint8_t> inbuffer,
                     size_t nbits,
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
void pack(std::span<const uint8_t> inbuffer,
          std::span<uint8_t> outbuffer,
          size_t nbits,
          const std::string& bitorder,
          bool parallel = false);

/**
 * @brief Packs 1, 2, or 4 bit data into 8 bit bytes in place
 *
 * @param buffer Input buffer containing 8 bit bytes to be packed
 * @param nbits Number of bits to pack
 * @param bitorder Bit order of the output packed data
 */
void pack_inplace(std::span<uint8_t> inbuffer,
                  size_t nbits,
                  const std::string& bitorder);

} // namespace sigproc::bits