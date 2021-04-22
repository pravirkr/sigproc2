#pragma once

#include <vector>
#include <fstream>
#include <stdexcept>
#include <climits>  // CHAR_BIT (bits_per_byte)

#include "pack_unpack.hpp"

namespace FileIO {

/* read nread units of data from stream */
void read_data(std::ifstream& stream, int nbits, std::vector<float>& block,
               int nread);

/* write block of data to stream */
void write_data(std::ofstream& stream, int nbits, const std::vector<float>& block,
                int nread);

std::size_t get_itemsize(int nbits);

std::size_t get_bitfact(int nbits);

}  // namespace FileIO