#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/fake.hpp>
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

TEST_CASE("sig_fake --help lists original short flags") {
    if (!std::filesystem::exists(bin("sig_fake"))) {
        SKIP("sig_fake not available");
    }
    const auto help = run_cmd(bin("sig_fake").string() + " --help");
    REQUIRE(help.find("-period") != std::string::npos);
    REQUIRE(help.find("-tsamp") != std::string::npos);
    REQUIRE(help.find("-evenodd") != std::string::npos);
}

TEST_CASE("sig_fake generates a file sig_header can read") {
    if (!std::filesystem::exists(bin("sig_fake")) ||
        !std::filesystem::exists(bin("sig_header"))) {
        SKIP("sig_fake or sig_header not available");
    }
    const auto tmp = std::filesystem::temp_directory_path() / "fake_tiny.fil";
    const int rc   = std::system((bin("sig_fake").string() +
                                  " -nchans 8 -nbits 8 -tsamp 1000 -tobs 0.016 "
                                  "-period 10 -dm 0 -seed 1 -nosmear -o " +
                                  tmp.string() + " >/dev/null 2>/dev/null")
                                     .c_str());
    REQUIRE(rc == 0);
    const auto nchans =
        run_cmd(bin("sig_header").string() + " -nchans " + tmp.string());
    REQUIRE(nchans.find("8") != std::string::npos);
    std::filesystem::remove(tmp);
}

TEST_CASE("sig_fast_fake default MJD is 56000 and 2-bit payload is in range") {
    if (!std::filesystem::exists(bin("sig_fast_fake"))) {
        SKIP("sig_fast_fake not available");
    }
    const auto hdr =
        std::filesystem::temp_directory_path() / "fast_fake_hdr.fil";
    const int rc_hdr =
        std::system((bin("sig_fast_fake").string() + " --test -o " +
                     hdr.string() + " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc_hdr == 0);
    sigproc::io::SigprocHeader h;
    REQUIRE(h.fromfile(hdr.string()));
    REQUIRE(h.get<double>("tstart") == 56000.0);
    REQUIRE(h.get<int>("nbits") == 2);
    REQUIRE(h.get<int>("nchans") == 1024);
    std::filesystem::remove(hdr);

    const auto pay = std::filesystem::temp_directory_path() / "fast_fake.fil";
    const int rc   = std::system((bin("sig_fast_fake").string() +
                                  " -T 0.000512 -t 64 -c 8 -b 2 -S 3 -o " +
                                  pay.string() + " >/dev/null 2>/dev/null")
                                     .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader reader(pay.string());
    std::vector<float> block;
    reader.read_block(0, reader.nsamps() > 0 ? reader.nsamps() : 8, block);
    for (float v : block) {
        REQUIRE(v >= 0.0F);
        REQUIRE(v <= 3.0F);
    }
    std::filesystem::remove(pay);
}

TEST_CASE("sig_extract is 1-based, updates tstart, copies packed payload") {
    const auto fil = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    if (!std::filesystem::exists(bin("sig_extract")) ||
        !std::filesystem::exists(fil)) {
        SKIP("sig_extract or tiny.fil not available");
    }
    const auto tmp = std::filesystem::temp_directory_path() / "extract_out.fil";
    const int rc =
        std::system((bin("sig_extract").string() + " " + fil.string() +
                     " 3 2 -o " + tmp.string() + " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);

    sigproc::io::SigprocHeader in_hdr;
    REQUIRE(in_hdr.fromfile(fil.string()));
    sigproc::io::SigprocHeader out_hdr;
    REQUIRE(out_hdr.fromfile(tmp.string()));
    const double expect_tstart = in_hdr.get<double>("tstart") +
                                 2.0 * in_hdr.get<double>("tsamp") / 86400.0;
    REQUIRE(std::abs(out_hdr.get<double>("tstart") - expect_tstart) < 1e-15);
    REQUIRE(out_hdr.get<int>("nsamples") == 2);

    std::ifstream in_orig(fil, std::ios::binary);
    std::ifstream in_out(tmp, std::ios::binary);
    std::string orig((std::istreambuf_iterator<char>(in_orig)), {});
    std::string extracted((std::istreambuf_iterator<char>(in_out)), {});
    const auto in_hsize =
        static_cast<std::size_t>(in_hdr.get<int>("header_size"));
    const auto out_hsize =
        static_cast<std::size_t>(out_hdr.get<int>("header_size"));
    REQUIRE(extracted.substr(0, out_hsize) != orig.substr(0, in_hsize));
    const std::size_t spec = 8; // 8-bit × 8 chans
    REQUIRE(extracted.substr(out_hsize) ==
            orig.substr(in_hsize + 2 * spec, 2 * spec));
    std::filesystem::remove(tmp);
}

TEST_CASE("sig_extract tstart example: start_sample 1001, tsamp 0.001 s") {
    if (!std::filesystem::exists(bin("sig_extract"))) {
        SKIP("sig_extract not available");
    }
    const auto src = std::filesystem::temp_directory_path() / "extract_src.fil";
    const auto dst = std::filesystem::temp_directory_path() / "extract_mjd.fil";
    {
        sigproc::fake::FakeConfig cfg;
        cfg.nchans   = 2;
        cfg.nbits    = 8;
        cfg.tsamp    = 0.001;
        cfg.tobs     = 1.002;
        cfg.tstart   = 50000.0;
        cfg.fch1     = 1400.0;
        cfg.foff     = -1.0;
        cfg.period_s = 0.0;
        cfg.dm       = 0.0;
        cfg.seed     = 1;
        std::ofstream out(src, std::ios::binary);
        sigproc::fake::generate(out, cfg);
    }
    const int rc =
        std::system((bin("sig_extract").string() + " " + src.string() +
                     " 1001 1 -o " + dst.string() + " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);
    sigproc::io::SigprocHeader hdr;
    REQUIRE(hdr.fromfile(dst.string()));
    const double delta = 1.0 / 86400.0;
    REQUIRE(std::abs(hdr.get<double>("tstart") - (50000.0 + delta)) < 1e-15);
    std::filesystem::remove(src);
    std::filesystem::remove(dst);
}

TEST_CASE("sig_downsample nadd=2 averages pairs and doubles tsamp") {
    const auto fil = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    if (!std::filesystem::exists(bin("sig_downsample")) ||
        !std::filesystem::exists(fil)) {
        SKIP("sig_downsample or tiny.fil not available");
    }
    const auto tmp =
        std::filesystem::temp_directory_path() / "downsample_out.fil";
    const int rc =
        std::system((bin("sig_downsample").string() + " " + fil.string() + " " +
                     tmp.string() + " 2 >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader in_r(fil.string());
    sigproc::FilterbankReader out_r(tmp.string());
    REQUIRE(out_r.hdr.get<int>("nbits") == 32);
    REQUIRE(out_r.hdr.get<int>("nchans") == in_r.hdr.get<int>("nchans"));
    REQUIRE(out_r.hdr.get<double>("tsamp") ==
            in_r.hdr.get<double>("tsamp") * 2.0);
    REQUIRE(out_r.hdr.get<int>("nsamples") == 8);

    std::vector<float> in_block;
    std::vector<float> out_block;
    in_r.read_block(0, 16, in_block);
    out_r.read_block(0, 8, out_block);
    const int nchans = 8;
    for (int t = 0; t < 8; ++t) {
        for (int c = 0; c < nchans; ++c) {
            const float mean =
                0.5F *
                (in_block[static_cast<std::size_t>(t * 2 * nchans + c)] +
                 in_block[static_cast<std::size_t>((t * 2 + 1) * nchans + c)]);
            REQUIRE(out_block[static_cast<std::size_t>(t * nchans + c)] ==
                    mean);
        }
    }
    std::filesystem::remove(tmp);
}
