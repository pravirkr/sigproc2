#pragma once

#include <string>

class ErrorChecker {
public:
    template <class Tstream>
    static void check_stream(Tstream& stream, const std::string& filename);
};
