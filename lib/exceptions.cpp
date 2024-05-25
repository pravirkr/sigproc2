#include <format>
#include <stdexcept>

#include <sigproc/exceptions.hpp>

template <class Tstream>
void ErrorChecker::check_stream(Tstream& stream, const std::string& filename) {
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
