#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>

#include "fil_test_utils.hpp"

#ifndef SIG_BIN_DIR
#define SIG_BIN_DIR "."
#endif

#ifndef SIG_TEST_DATA_DIR
#define SIG_TEST_DATA_DIR "."
#endif

namespace {

std::string run_cmd(const std::string& cmd) {
    const std::string tmp =
        (std::filesystem::temp_directory_path() / "sigproc2_cli_out.txt")
            .string();
    const int rc = std::system((cmd + " > " + tmp + " 2>/dev/null").c_str());
    std::ifstream in(tmp);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::filesystem::remove(tmp);
    REQUIRE(rc == 0);
    return ss.str();
}

std::filesystem::path bin(std::string_view name) {
    return std::filesystem::path(SIG_BIN_DIR) / name;
}

} // namespace

TEST_CASE("sig_header -tsamp prints microseconds and -k tsamp prints seconds") {
    const auto fil = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    if (!std::filesystem::exists(bin("sig_header")) ||
        !std::filesystem::exists(fil)) {
        SKIP("sig_header or tiny.fil not available");
    }
    const auto tsamp_us =
        run_cmd(bin("sig_header").string() + " -tsamp " + fil.string());
    REQUIRE(tsamp_us.find("1000.00000") != std::string::npos);

    const auto tsamp_s =
        run_cmd(bin("sig_header").string() + " -k tsamp " + fil.string());
    REQUIRE(tsamp_s.find("0.001") != std::string::npos);

    const auto fch1 =
        run_cmd(bin("sig_header").string() + " -fch1 " + fil.string());
    REQUIRE(fch1.find("1400") != std::string::npos);
}

TEST_CASE("sig_header --help lists original short flags") {
    if (!std::filesystem::exists(bin("sig_header"))) {
        SKIP("sig_header not available");
    }
    const auto help = run_cmd(bin("sig_header").string() + " --help");
    REQUIRE(help.find("-tsamp") != std::string::npos);
    REQUIRE(help.find("-fch1") != std::string::npos);
    REQUIRE(help.find("-scan_number") != std::string::npos);
}
