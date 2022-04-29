#pragma once

#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <fmt/core.h>

#include <sigproc/params.hpp>
#include <sigproc/utils.hpp>

namespace ErrorChecker {

template <class Tstream>
static void check_file(Tstream& stream, std::string filename) {
    if (!stream.good()) {
        std::stringstream error_msg;
        error_msg << "File " << filename << " could not be opened: ";

        if ((stream.rdstate() & std::ifstream::failbit) != 0)
            error_msg << "Logical error on i/o operation" << std::endl;

        if ((stream.rdstate() & std::ifstream::badbit) != 0)
            error_msg << "Read/writing error on i/o operation" << std::endl;

        if ((stream.rdstate() & std::ifstream::eofbit) != 0)
            error_msg << "End-of-File reached on input operation" << std::endl;

        throw std::runtime_error(error_msg.str());
    }
}

void check_bits(int nbits) {
    if (!utils::exists_key(nbits_to_sigmas, nbits)) {
        std::string msg = fmt::format("nbits = {} not supported.", nbits);
        throw std::invalid_argument(msg);
    }
}

};  // namespace ErrorChecker