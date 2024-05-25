#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#ifdef USE_OPENMP
#include <omp.h>
#endif

#include <sigproc/numbits.hpp>

constexpr unsigned char kHI4BITS    = 240;
constexpr unsigned char kLO4BITS    = 15;
constexpr unsigned char kHI2BITS    = 192;
constexpr unsigned char kUPMED2BITS = 48;
constexpr unsigned char kLOMED2BITS = 12;
constexpr unsigned char kLO2BITS    = 3;

// Lookup table for bit unpacking
template <size_t NBits> struct LookupTableGenerate {
    static constexpr size_t kSize     = 256;
    static constexpr size_t kElements = 8 / NBits;
    alignas(64) std::array<std::array<uint8_t, kElements>,
                           kSize> table_big{}; // 256 * 8/NBits bytes
    alignas(64) std::array<std::array<uint8_t, kElements>,
                           kSize> table_little{}; // 256 * 8/NBits bytes

    constexpr LookupTableGenerate() {
        for (size_t ii = 0; ii < kSize; ii++) {
            for (size_t jj = 0; jj < kElements; jj++) {
                size_t unpacked = (ii >> (jj * NBits)) & ((1 << NBits) - 1);
                table_big[ii][kElements - 1 - jj] = unpacked;
                table_little[ii][jj]              = unpacked;
            }
        }
    }
};

// Compile-time lookup table initialization
constexpr LookupTableGenerate<1> kLookup1bit{};
constexpr LookupTableGenerate<2> kLookup2bit{};
constexpr LookupTableGenerate<4> kLookup4bit{};

template <bool Parallel, bool BigEndian>
void unpack_1bit_lookup(std::span<const uint8_t> inbuffer,
                        std::span<uint8_t> outbuffer) {
    const auto& table =
        BigEndian ? kLookup1bit.table_big : kLookup1bit.table_little;
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer, table)
#endif
    for (size_t ii = 0; ii < inbuffer.size(); ii++) {
        std::copy_n(table[inbuffer[ii]].data(), 8, outbuffer.data() + ii * 8);
    }
}

template <bool Parallel, bool BigEndian>
void unpack_2bit_lookup(std::span<const uint8_t> inbuffer,
                        std::span<uint8_t> outbuffer) {
    const auto& table =
        BigEndian ? kLookup2bit.table_big : kLookup2bit.table_little;
#ifdef USE_OPENMP
#pragma omp parallel for if (parallel) default(none)                           \
    shared(inbuffer, outbuffer, table)
#endif
    for (size_t ii = 0; ii < inbuffer.size(); ii++) {
        std::copy_n(table[inbuffer[ii]].data(), 4, outbuffer.data() + ii * 4);
    }
}

template <bool Parallel, bool BigEndian>
void unpack_4bit_lookup(std::span<const uint8_t> inbuffer,
                        std::span<uint8_t> outbuffer) {
    const auto& table =
        BigEndian ? kLookup4bit.table_big : kLookup4bit.table_little;
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer, table)
#endif
    for (size_t ii = 0; ii < inbuffer.size(); ii++) {
        std::copy_n(table[inbuffer[ii]].data(), 2, outbuffer.data() + ii * 2);
    }
}

template <bool Parallel, bool BigEndian>
void unpack_1bit(std::span<const uint8_t> inbuffer,
                 std::span<uint8_t> outbuffer) {
    size_t pos{};
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer) firstprivate(pos)
#endif
    for (size_t ii = 0; ii < inbuffer.size(); ii++) {
        pos = ii << 3;
        for (size_t jj = 0; jj < 8; jj++) {
            if constexpr (BigEndian) {
                outbuffer[pos + (7 - jj)] = (inbuffer[ii] >> jj) & 1;
            } else {
                outbuffer[pos + jj] = (inbuffer[ii] >> jj) & 1;
            }
        }
    }
}

template <bool Parallel, bool BigEndian>
void unpack_2bit(std::span<const uint8_t> inbuffer,
                 std::span<uint8_t> outbuffer) {
    size_t pos{};
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer) firstprivate(pos)
#endif
    for (size_t ii = 0; ii < inbuffer.size(); ii++) {
        pos = ii << 2;
        if constexpr (BigEndian) {
            outbuffer[pos + 3] = inbuffer[ii] & kLO2BITS;
            outbuffer[pos + 2] = (inbuffer[ii] & kLOMED2BITS) >> 2;
            outbuffer[pos + 1] = (inbuffer[ii] & kUPMED2BITS) >> 4;
            outbuffer[pos + 0] = (inbuffer[ii] & kHI2BITS) >> 6;
        } else {
            outbuffer[pos + 0] = inbuffer[ii] & kLO2BITS;
            outbuffer[pos + 1] = (inbuffer[ii] & kLOMED2BITS) >> 2;
            outbuffer[pos + 2] = (inbuffer[ii] & kUPMED2BITS) >> 4;
            outbuffer[pos + 3] = (inbuffer[ii] & kHI2BITS) >> 6;
        }
    }
}

template <bool Parallel, bool BigEndian>
void unpack_4bit(std::span<const uint8_t> inbuffer,
                 std::span<uint8_t> outbuffer) {
    size_t pos{};
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer) firstprivate(pos)
#endif
    for (size_t ii = 0; ii < inbuffer.size(); ii++) {
        pos = ii << 1;
        if constexpr (BigEndian) {
            outbuffer[pos + 1] = inbuffer[ii] & kLO4BITS;
            outbuffer[pos + 0] = (inbuffer[ii] & kHI4BITS) >> 4;
        } else {
            outbuffer[pos + 0] = inbuffer[ii] & kLO4BITS;
            outbuffer[pos + 1] = (inbuffer[ii] & kHI4BITS) >> 4;
        }
    }
}

template <bool BigEndian> void unpack_1bit_inplace(std::span<uint8_t> buffer) {
    size_t lastsamp = buffer.size() / 8;
    size_t pos{};
    uint8_t temp{};
    for (size_t ii = lastsamp - 1; ii != 0; ii--) {
        temp = buffer[ii];
        pos  = ii << 3;
        for (size_t jj = 0; jj < 8; jj++) {
            if constexpr (BigEndian) {
                buffer[pos + jj] = (temp >> jj) & 1;
            } else {
                buffer[pos + (7 - jj)] = (temp >> jj) & 1;
            }
        }
    }
}

template <bool BigEndian> void unpack_2bit_inplace(std::span<uint8_t> buffer) {
    size_t lastsamp = buffer.size() / 4;
    size_t pos{};
    uint8_t temp{};
    for (size_t ii = lastsamp - 1; ii != 0; ii--) {
        temp = buffer[ii];
        pos  = ii << 2;
        if constexpr (BigEndian) {
            buffer[pos + 3] = temp & kLO2BITS;
            buffer[pos + 2] = (temp & kLOMED2BITS) >> 2;
            buffer[pos + 1] = (temp & kUPMED2BITS) >> 4;
            buffer[pos + 0] = (temp & kHI2BITS) >> 6;
        } else {
            buffer[pos + 0] = temp & kLO2BITS;
            buffer[pos + 1] = (temp & kLOMED2BITS) >> 2;
            buffer[pos + 2] = (temp & kUPMED2BITS) >> 4;
            buffer[pos + 3] = (temp & kHI2BITS) >> 6;
        }
    }
}

template <bool BigEndian> void unpack_4bit_inplace(std::span<uint8_t> buffer) {
    size_t lastsamp = buffer.size() / 2;
    size_t pos{};
    uint8_t temp{};
    for (size_t ii = lastsamp - 1; ii != 0; ii--) {
        temp = buffer[ii];
        pos  = ii << 1;
        if constexpr (BigEndian) {
            buffer[pos + 1] = temp & kLO4BITS;
            buffer[pos + 0] = (temp & kHI4BITS) >> 4;
        } else {
            buffer[pos + 0] = temp & kLO4BITS;
            buffer[pos + 1] = (temp & kHI4BITS) >> 4;
        }
    }
}

template <bool Parallel, bool BigEndian>
void pack_1bit(std::span<const uint8_t> inbuffer,
               std::span<uint8_t> outbuffer) {
    size_t pos{};
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer) firstprivate(pos)
#endif
    for (size_t ii = 0; ii < inbuffer.size() / 8; ii++) {
        pos = ii << 3;
        if constexpr (BigEndian) {
            outbuffer[ii] =
                (inbuffer[pos + 0] << 7) | (inbuffer[pos + 1] << 6) |
                (inbuffer[pos + 2] << 5) | (inbuffer[pos + 3] << 4) |
                (inbuffer[pos + 4] << 3) | (inbuffer[pos + 5] << 2) |
                (inbuffer[pos + 6] << 1) | inbuffer[pos + 7];
        } else {
            outbuffer[ii] =
                inbuffer[pos + 0] | (inbuffer[pos + 1] << 1) |
                (inbuffer[pos + 2] << 2) | (inbuffer[pos + 3] << 3) |
                (inbuffer[pos + 4] << 4) | (inbuffer[pos + 5] << 5) |
                (inbuffer[pos + 6] << 6) | (inbuffer[pos + 7] << 7);
        }
    }
}

template <bool Parallel, bool BigEndian>
void pack_2bit(std::span<const uint8_t> inbuffer,
               std::span<uint8_t> outbuffer) {
    size_t pos{};
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer) firstprivate(pos)
#endif
    for (size_t ii = 0; ii < inbuffer.size() / 4; ii++) {
        pos = ii << 2;
        if constexpr (BigEndian) {
            outbuffer[ii] = (inbuffer[pos + 0] << 6) |
                            (inbuffer[pos + 1] << 4) |
                            (inbuffer[pos + 2] << 2) | inbuffer[pos + 3];
        } else {
            outbuffer[ii] = inbuffer[pos + 0] | (inbuffer[pos + 1] << 2) |
                            (inbuffer[pos + 2] << 4) | (inbuffer[pos + 3] << 6);
        }
    }
}

template <bool Parallel, bool BigEndian>
void pack_4bit(std::span<const uint8_t> inbuffer,
               std::span<uint8_t> outbuffer) {
    size_t pos{};
#ifdef USE_OPENMP
#pragma omp parallel for if (Parallel) default(none)                           \
    shared(inbuffer, outbuffer) firstprivate(pos)
#endif
    for (size_t ii = 0; ii < inbuffer.size() / 2; ii++) {
        pos = ii << 1;
        if constexpr (BigEndian) {
            outbuffer[ii] = (inbuffer[pos] << 4) | inbuffer[pos + 1];
        } else {
            outbuffer[ii] = inbuffer[pos] | (inbuffer[pos + 1] << 4);
        }
    }
}

template <bool BigEndian> void pack_1bit_inplace(std::span<uint8_t> buffer) {
    size_t lastsamp = buffer.size() / 8;
    size_t pos{};
    uint8_t temp{};
    for (size_t ii = 0; ii < lastsamp; ii++) {
        pos = ii << 3;
        if constexpr (BigEndian) {
            temp = (buffer[pos + 0] << 7) | (buffer[pos + 1] << 6) |
                   (buffer[pos + 2] << 5) | (buffer[pos + 3] << 4) |
                   (buffer[pos + 4] << 3) | (buffer[pos + 5] << 2) |
                   (buffer[pos + 6] << 1) | buffer[pos + 7];
        } else {
            temp = buffer[pos + 0] | (buffer[pos + 1] << 1) |
                   (buffer[pos + 2] << 2) | (buffer[pos + 3] << 3) |
                   (buffer[pos + 4] << 4) | (buffer[pos + 5] << 5) |
                   (buffer[pos + 6] << 6) | (buffer[pos + 7] << 7);
        }
        buffer[ii] = temp;
    }
}

template <bool BigEndian> void pack_2bit_inplace(std::span<uint8_t> buffer) {
    size_t lastsamp = buffer.size() / 4;
    size_t pos{};
    uint8_t temp{};
    for (size_t ii = 0; ii < lastsamp; ii++) {
        pos = ii << 2;
        if constexpr (BigEndian) {
            temp = (buffer[pos + 0] << 6) | (buffer[pos + 1] << 4) |
                   (buffer[pos + 2] << 2) | buffer[pos + 3];
        } else {
            temp = buffer[pos + 0] | (buffer[pos + 1] << 2) |
                   (buffer[pos + 2] << 4) | (buffer[pos + 3] << 6);
        }
        buffer[ii] = temp;
    }
}

template <bool BigEndian> void pack_4bit_inplace(std::span<uint8_t> buffer) {
    size_t lastsamp = buffer.size() / 2;
    size_t pos{};
    uint8_t temp{};
    for (size_t ii = 0; ii < lastsamp; ii++) {
        pos = ii << 1;
        if constexpr (BigEndian) {
            temp = (buffer[pos] << 4) | buffer[pos + 1];
        } else {
            temp = buffer[pos] | (buffer[pos + 1] << 4);
        }
        buffer[ii] = temp;
    }
}

using PackUnpackFunc = void (*)(std::span<const uint8_t>, std::span<uint8_t>);
using PackUnpackFuncInPlace = void (*)(std::span<uint8_t>);

constexpr std::array<std::array<std::array<PackUnpackFunc, 2>, 2>, 3>
    kUnpackLookupDispatcher = {{
        {{
            {{unpack_1bit_lookup<false, false>,
              unpack_1bit_lookup<true, false>}},
            {{unpack_1bit_lookup<false, true>, unpack_1bit_lookup<true, true>}},
        }},
        {{
            {{unpack_2bit_lookup<false, false>,
              unpack_2bit_lookup<true, false>}},
            {{unpack_2bit_lookup<false, true>, unpack_2bit_lookup<true, true>}},
        }},
        {{
            {{unpack_4bit_lookup<false, false>,
              unpack_4bit_lookup<true, false>}},
            {{unpack_4bit_lookup<false, true>, unpack_4bit_lookup<true, true>}},
        }},
    }};
constexpr std::array<std::array<std::array<PackUnpackFunc, 2>, 2>, 3>
    kUnpackDispatcher = {{
        {{
            {{unpack_1bit<false, false>, unpack_1bit<true, false>}},
            {{unpack_1bit<false, true>, unpack_1bit<true, true>}},
        }},
        {{
            {{unpack_2bit<false, false>, unpack_2bit<true, false>}},
            {{unpack_2bit<false, true>, unpack_2bit<true, true>}},
        }},
        {{
            {{unpack_4bit<false, false>, unpack_4bit<true, false>}},
            {{unpack_4bit<false, true>, unpack_4bit<true, true>}},
        }},
    }};

constexpr std::array<std::array<PackUnpackFuncInPlace, 2>, 3>
    kUnpackInPlaceDispatcher = {{
        {{unpack_1bit_inplace<false>, unpack_1bit_inplace<true>}},
        {{unpack_2bit_inplace<false>, unpack_2bit_inplace<true>}},
        {{unpack_4bit_inplace<false>, unpack_4bit_inplace<true>}},
    }};

constexpr std::array<std::array<std::array<PackUnpackFunc, 2>, 2>, 3>
    kPackDispatcher = {{
        {{
            {{pack_1bit<false, false>, pack_1bit<true, false>}},
            {{pack_1bit<false, true>, pack_1bit<true, true>}},
        }},
        {{
            {{pack_2bit<false, false>, pack_2bit<true, false>}},
            {{pack_2bit<false, true>, pack_2bit<true, true>}},
        }},
        {{
            {{pack_4bit<false, false>, pack_4bit<true, false>}},
            {{pack_4bit<false, true>, pack_4bit<true, true>}},
        }},
    }};

constexpr std::array<std::array<PackUnpackFuncInPlace, 2>, 3>
    kPackInPlaceDispatcher = {{
        {{pack_1bit_inplace<false>, pack_1bit_inplace<true>}},
        {{pack_2bit_inplace<false>, pack_2bit_inplace<true>}},
        {{pack_4bit_inplace<false>, pack_4bit_inplace<true>}},
    }};

size_t get_bitorder_index(const std::string& bitorder) {
    if (bitorder.empty() || (bitorder[0] != 'l' && bitorder[0] != 'b')) {
        throw std::invalid_argument(
            "Invalid bitorder. Must begin with 'l' or 'b'.");
    }
    return (bitorder[0] == 'b') ? 1 : 0;
}

void sigproc::unpack(std::span<const uint8_t> inbuffer,
                     std::span<uint8_t> outbuffer, size_t nbits,
                     const std::string& bitorder, bool parallel) {
    if (nbits != 1 && nbits != 2 && nbits != 4) {
        throw std::invalid_argument("Number of bits must be 1, 2, or 4");
    }
    const size_t bitorder_index = get_bitorder_index(bitorder);
    const size_t nbits_index    = nbits >> 1;
    const size_t parallel_index = parallel ? 1 : 0;
    kUnpackDispatcher[nbits_index][bitorder_index][parallel_index](inbuffer,
                                                                   outbuffer);
}

void sigproc::unpack_lookup(std::span<const uint8_t> inbuffer,
                            std::span<uint8_t> outbuffer, size_t nbits,
                            const std::string& bitorder, bool parallel) {
    if (nbits != 1 && nbits != 2 && nbits != 4) {
        throw std::invalid_argument("Number of bits must be 1, 2, or 4");
    }
    const size_t bitorder_index = get_bitorder_index(bitorder);
    const size_t nbits_index    = nbits >> 1;
    const size_t parallel_index = parallel ? 1 : 0;
    kUnpackLookupDispatcher[nbits_index][bitorder_index][parallel_index](
        inbuffer, outbuffer);
}

void sigproc::unpack_in_place(std::span<uint8_t> inbuffer, size_t nbits,
                              const std::string& bitorder) {
    if (nbits != 1 && nbits != 2 && nbits != 4) {
        throw std::invalid_argument("Number of bits must be 1, 2, or 4");
    }
    const size_t bitorder_index = get_bitorder_index(bitorder);
    const size_t nbits_index    = nbits >> 1;
    kUnpackInPlaceDispatcher[nbits_index][bitorder_index](inbuffer);
}

void sigproc::pack(std::span<const uint8_t> inbuffer,
                   std::span<uint8_t> outbuffer, size_t nbits,
                   const std::string& bitorder, bool parallel) {
    if (nbits != 1 && nbits != 2 && nbits != 4) {
        throw std::invalid_argument("Number of bits must be 1, 2, or 4");
    }
    const size_t bitorder_index = get_bitorder_index(bitorder);
    const size_t nbits_index    = nbits >> 1;
    const size_t parallel_index = parallel ? 1 : 0;
    kPackDispatcher[nbits_index][bitorder_index][parallel_index](inbuffer,
                                                                 outbuffer);
}

void sigproc::pack_inplace(std::span<uint8_t> inbuffer, size_t nbits,
                           const std::string& bitorder) {
    if (nbits != 1 && nbits != 2 && nbits != 4) {
        throw std::invalid_argument("Number of bits must be 1, 2, or 4");
    }
    const size_t bitorder_index = get_bitorder_index(bitorder);
    const size_t nbits_index    = nbits >> 1;
    kPackInPlaceDispatcher[nbits_index][bitorder_index](inbuffer);
}
