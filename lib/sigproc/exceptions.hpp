#pragma once

#include <format>
#include <stdexcept>
#include <string>

namespace sigproc::error_check {

template <class Tstream>
void check_stream(Tstream& stream, const std::string& filename) {
    if (!stream.good()) {
        auto error_msg = std::format("File {} could not be opened: ", filename);
        if (stream.eof()) {
            error_msg += "Reached end of file unexpectedly";
        } else if (stream.fail()) {
            error_msg += "Logical error on i/o operation";
        } else if (stream.bad()) {
            error_msg += "Read/writing error on i/o operation";
        } else {
            error_msg += "Unknown error occurred while processing file";
        }
        throw std::runtime_error(error_msg);
    }
}

/// @brief Alias for check_stream: verify a file stream opened successfully.
template <class Tstream>
void check_file(Tstream& stream, const std::string& filename) {
    check_stream(stream, filename);
}

} // namespace sigproc::error_check
