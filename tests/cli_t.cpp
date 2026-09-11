#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
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

TEST_CASE("sig_bandpass -d 1 emits #START/#STOP") {
    const auto fil = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    if (!std::filesystem::exists(bin("sig_bandpass")) ||
        !std::filesystem::exists(fil)) {
        SKIP("sig_bandpass or tiny.fil not available");
    }
    const auto out =
        run_cmd(bin("sig_bandpass").string() + " -d 1 " + fil.string());
    REQUIRE(out.find("#START") != std::string::npos);
    REQUIRE(out.find("#STOP") != std::string::npos);
}

TEST_CASE("sig_chopfil copies header bytes and a packed time slice") {
    const auto fil = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    if (!std::filesystem::exists(bin("sig_chopfil")) ||
        !std::filesystem::exists(fil)) {
        SKIP("sig_chopfil or tiny.fil not available");
    }
    const auto tmp = std::filesystem::temp_directory_path() / "chop_out.fil";
    const int rc   = std::system((bin("sig_chopfil").string() +
                                  " -s 0 -r 0.016 " + fil.string() + " -o " +
                                  tmp.string() + " >/dev/null 2>/dev/null")
                                     .c_str());
    REQUIRE(rc == 0);
    std::ifstream in_orig(fil, std::ios::binary);
    std::ifstream in_out(tmp, std::ios::binary);
    std::string orig((std::istreambuf_iterator<char>(in_orig)), {});
    std::string chopped((std::istreambuf_iterator<char>(in_out)), {});
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromfile(fil.string()));
    const auto hsize = static_cast<std::size_t>(hdr.get<int>("header_size"));
    REQUIRE(chopped.size() >= hsize);
    REQUIRE(chopped.substr(0, hsize) == orig.substr(0, hsize));
    REQUIRE(chopped.size() == orig.size()); // 16 samples at 0.016 s
    std::filesystem::remove(tmp);
}

TEST_CASE("sig_decimate omitted -c adds all channels") {
    const auto fil = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    if (!std::filesystem::exists(bin("sig_decimate")) ||
        !std::filesystem::exists(fil)) {
        SKIP("sig_decimate or tiny.fil not available");
    }
    const auto tmp = std::filesystem::temp_directory_path() / "dec_out.fil";
    const int rc =
        std::system((bin("sig_decimate").string() + " -t 2 " + fil.string() +
                     " -o " + tmp.string() + " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromfile(tmp.string()));
    REQUIRE(hdr.get<int>("nchans") == 1);
    REQUIRE(hdr.get<int>("nsamples") == 8);
    std::filesystem::remove(tmp);
}
