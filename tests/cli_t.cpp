#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/fake.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>
#include <sigproc/timeseries.hpp>

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

void write_fil(const std::filesystem::path& path,
               int nchans,
               int nsamps,
               double fch1,
               double tstart,
               const std::vector<float>& samples,
               int nbits = 8) {
    sigproc::io::SigprocHeader hdr;
    hdr.set("source_name", std::string("CHAN"));
    hdr.set("data_type", 1);
    hdr.set("nchans", nchans);
    hdr.set("nbits", nbits);
    hdr.set("nifs", 1);
    hdr.set("nsamples", nsamps);
    hdr.set("fch1", fch1);
    hdr.set("foff", -1.0);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", tstart);
    sigproc::FilterbankWriter writer(path.string(), hdr);
    writer.write_block(samples, static_cast<int>(samples.size()));
}

void write_tim(const std::filesystem::path& path,
               int nsamps,
               double tsamp,
               const std::vector<float>& samples) {
    sigproc::io::SigprocHeader hdr;
    hdr.set("source_name", std::string("TIM"));
    hdr.set("data_type", 2);
    hdr.set("nchans", 1);
    hdr.set("nbits", 32);
    hdr.set("nifs", 1);
    hdr.set("nsamples", nsamps);
    hdr.set("fch1", 1400.0);
    hdr.set("tsamp", tsamp);
    hdr.set("tstart", 50000.0);
    hdr.set("refdm", 0.0);
    sigproc::TimeSeries ts(std::move(hdr), samples);
    ts.tofile(path.string());
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

TEST_CASE("sig_splice --help lists -o") {
    if (!std::filesystem::exists(bin("sig_splice"))) {
        SKIP("sig_splice not available");
    }
    const auto help = run_cmd(bin("sig_splice").string() + " --help");
    REQUIRE(help.find("-o") != std::string::npos);
}

TEST_CASE("sig_splice concatenates two 4-chan files and writes a freq table") {
    if (!std::filesystem::exists(bin("sig_splice"))) {
        SKIP("sig_splice not available");
    }
    const auto dir              = std::filesystem::temp_directory_path();
    const auto a                = dir / "splice_a.fil";
    const auto b                = dir / "splice_b.fil";
    const auto out              = dir / "splice_out.fil";
    const std::vector<float> sa = {1.F, 2.F, 3.F, 4.F, 9.F, 10.F, 11.F, 12.F};
    const std::vector<float> sb = {5.F, 6.F, 7.F, 8.F, 13.F, 14.F, 15.F, 16.F};
    write_fil(a, 4, 2, 1400.0, 50000.0, sa);
    write_fil(b, 4, 2, 1396.0, 50000.0, sb);

    const int rc = std::system((bin("sig_splice").string() + " " + a.string() +
                                " " + b.string() + " -o " + out.string() +
                                " >/dev/null 2>/dev/null")
                                   .c_str());
    REQUIRE(rc == 0);

    sigproc::FilterbankReader reader(out.string());
    REQUIRE(reader.hdr.get<int>("nchans") == 8);
    REQUIRE(reader.hdr.has_freq_table());
    REQUIRE_FALSE(reader.hdr.is_present("fch1"));
    REQUIRE_FALSE(reader.hdr.is_present("foff"));
    const auto freqs = reader.hdr.get_freq_table();
    REQUIRE(freqs.size() == 8);
    REQUIRE(freqs.front() == 1400.0);
    REQUIRE(freqs[4] == 1396.0);

    std::ifstream raw(out, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(raw)), {});
    const auto keys = sigproc::test::header_key_order(
        std::span<const char>(bytes.data(), bytes.size()));
    REQUIRE(std::find(keys.begin(), keys.end(), "fch1") == keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "foff") == keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "FREQUENCY_START") !=
            keys.end());

    std::vector<float> block;
    reader.read_block(0, 2, block);
    REQUIRE(block == std::vector<float>{1.F, 2.F, 3.F, 4.F, 5.F, 6.F, 7.F, 8.F,
                                        9.F, 10.F, 11.F, 12.F, 13.F, 14.F, 15.F,
                                        16.F});
    std::filesystem::remove(a);
    std::filesystem::remove(b);
    std::filesystem::remove(out);
}

TEST_CASE("sig_splice throws on tstart mismatch") {
    if (!std::filesystem::exists(bin("sig_splice"))) {
        SKIP("sig_splice not available");
    }
    const auto dir             = std::filesystem::temp_directory_path();
    const auto a               = dir / "splice_t0.fil";
    const auto b               = dir / "splice_t1.fil";
    const std::vector<float> s = {1.F, 2.F, 3.F, 4.F};
    write_fil(a, 4, 1, 1400.0, 50000.0, s);
    write_fil(b, 4, 1, 1396.0, 50001.0, s);
    const int rc = std::system((bin("sig_splice").string() + " " + a.string() +
                                " " + b.string() + " >/dev/null 2>/dev/null")
                                   .c_str());
    REQUIRE(rc != 0);
    std::filesystem::remove(a);
    std::filesystem::remove(b);
}

TEST_CASE("sig_dice --help mentions keep file") {
    if (!std::filesystem::exists(bin("sig_dice"))) {
        SKIP("sig_dice not available");
    }
    const auto help = run_cmd(bin("sig_dice").string() + " --help");
    REQUIRE(help.find("keep") != std::string::npos);
    REQUIRE(help.find("--collapse") != std::string::npos);
}

TEST_CASE("sig_dice keep 1 and 3 zeros channels 2 and 4") {
    if (!std::filesystem::exists(bin("sig_dice"))) {
        SKIP("sig_dice not available");
    }
    const auto dir  = std::filesystem::temp_directory_path();
    const auto fil  = dir / "dice_in.fil";
    const auto keep = dir / "dice_keep.txt";
    const auto out  = dir / "dice_out.fil";
    write_fil(fil, 4, 2, 1400.0, 50000.0,
              {1.F, 2.F, 3.F, 4.F, 5.F, 6.F, 7.F, 8.F});
    {
        std::ofstream kf(keep);
        kf << "1\n3\n";
    }
    const int rc = std::system((bin("sig_dice").string() + " " + fil.string() +
                                " " + keep.string() + " -o " + out.string() +
                                " >/dev/null 2>/dev/null")
                                   .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader reader(out.string());
    REQUIRE(reader.hdr.get<int>("nchans") == 4);
    REQUIRE(reader.hdr.get<double>("fch1") == 1400.0);
    std::vector<float> block;
    reader.read_block(0, 2, block);
    REQUIRE(block ==
            std::vector<float>{1.F, 0.F, 3.F, 0.F, 5.F, 0.F, 7.F, 0.F});
    std::filesystem::remove(fil);
    std::filesystem::remove(keep);
    std::filesystem::remove(out);
}

TEST_CASE("sig_dice 32-bit force-zeros is a superset of original nbits") {
    if (!std::filesystem::exists(bin("sig_dice"))) {
        SKIP("sig_dice not available");
    }
    const auto dir  = std::filesystem::temp_directory_path();
    const auto fil  = dir / "dice32_in.fil";
    const auto keep = dir / "dice32_keep.txt";
    const auto out  = dir / "dice32_out.fil";
    write_fil(fil, 4, 1, 1400.0, 50000.0, {10.F, 20.F, 30.F, 40.F}, 32);
    {
        std::ofstream kf(keep);
        kf << "1\n3\n";
    }
    const int rc = std::system((bin("sig_dice").string() + " " + fil.string() +
                                " " + keep.string() + " -o " + out.string() +
                                " >/dev/null 2>/dev/null")
                                   .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader reader(out.string());
    REQUIRE(reader.hdr.get<int>("nbits") == 32);
    std::vector<float> block;
    reader.read_block(0, 1, block);
    REQUIRE(block == std::vector<float>{10.F, 0.F, 30.F, 0.F});
    std::filesystem::remove(fil);
    std::filesystem::remove(keep);
    std::filesystem::remove(out);
}

TEST_CASE("sig_dice --collapse drops zapped channels") {
    if (!std::filesystem::exists(bin("sig_dice"))) {
        SKIP("sig_dice not available");
    }
    const auto dir  = std::filesystem::temp_directory_path();
    const auto fil  = dir / "dice_col_in.fil";
    const auto keep = dir / "dice_col_keep.txt";
    const auto out  = dir / "dice_col_out.fil";
    write_fil(fil, 4, 1, 1400.0, 50000.0, {1.F, 2.F, 3.F, 4.F});
    {
        std::ofstream kf(keep);
        kf << "1\n2\n";
    }
    const int rc = std::system((bin("sig_dice").string() + " --collapse " +
                                fil.string() + " " + keep.string() + " -o " +
                                out.string() + " >/dev/null 2>/dev/null")
                                   .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader reader(out.string());
    REQUIRE(reader.hdr.get<int>("nchans") == 2);
    REQUIRE_FALSE(reader.hdr.has_freq_table());
    REQUIRE(reader.hdr.get<double>("fch1") == 1400.0);
    std::vector<float> block;
    reader.read_block(0, 1, block);
    REQUIRE(block == std::vector<float>{1.F, 2.F});
    std::filesystem::remove(fil);
    std::filesystem::remove(keep);
    std::filesystem::remove(out);
}

TEST_CASE("sig_flatten --help documents 1-D gulp semantics") {
    if (!std::filesystem::exists(bin("sig_flatten"))) {
        SKIP("sig_flatten not available");
    }
    const auto help = run_cmd(bin("sig_flatten").string() + " --help");
    REQUIRE(help.find("-o") != std::string::npos);
    REQUIRE(help.find("gulp") != std::string::npos);
}

TEST_CASE("sig_flatten constant nonzero gulp writes zeros as data_type=2") {
    if (!std::filesystem::exists(bin("sig_flatten")) ||
        !std::filesystem::exists(bin("sig_header"))) {
        SKIP("sig_flatten or sig_header not available");
    }
    const auto dir = std::filesystem::temp_directory_path();
    const auto fil = dir / "flatten_const.fil";
    const auto tim = dir / "flatten_const.tim";
    write_fil(fil, 4, 4, 1400.0, 50000.0, std::vector<float>(16, 100.F));
    const int rc =
        std::system((bin("sig_flatten").string() + " " + fil.string() + " -o " +
                     tim.string() + " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);

    const auto datatype =
        run_cmd(bin("sig_header").string() + " -datatype " + tim.string());
    REQUIRE(datatype.find("time series") != std::string::npos);

    sigproc::FilterbankReader reader(tim.string());
    REQUIRE(reader.hdr.get<int>("data_type") == 2);
    REQUIRE(reader.hdr.get<int>("nchans") == 1);
    REQUIRE(reader.hdr.get<int>("nbits") == 32);
    REQUIRE(reader.hdr.get<int>("nsamples") == 16);
    std::vector<float> block;
    reader.read_block(0, 16, block);
    REQUIRE(block == std::vector<float>(16, 0.F));
    std::filesystem::remove(fil);
    std::filesystem::remove(tim);
}

TEST_CASE("sig_flatten all-zero file writes zeros") {
    if (!std::filesystem::exists(bin("sig_flatten"))) {
        SKIP("sig_flatten not available");
    }
    const auto dir = std::filesystem::temp_directory_path();
    const auto fil = dir / "flatten_zero.fil";
    const auto tim = dir / "flatten_zero.tim";
    write_fil(fil, 2, 4, 1400.0, 50000.0, std::vector<float>(8, 0.F), 32);
    const int rc =
        std::system((bin("sig_flatten").string() + " " + fil.string() + " -o " +
                     tim.string() + " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader reader(tim.string());
    std::vector<float> block;
    reader.read_block(0, 8, block);
    REQUIRE(block == std::vector<float>(8, 0.F));
    std::filesystem::remove(fil);
    std::filesystem::remove(tim);
}

TEST_CASE("sig_clip replaces one spike and omitted -o writes stdout") {
    if (!std::filesystem::exists(bin("sig_clip"))) {
        SKIP("sig_clip not available");
    }
    const auto dir = std::filesystem::temp_directory_path();
    const auto fil = dir / "clip_spike.fil";
    const auto tim = dir / "clip_spike.tim";
    const auto std = dir / "clip_stdout.tim";
    std::vector<float> samples(8, 1.F);
    samples[7] = 100.F;
    write_fil(fil, 1, 8, 1400.0, 50000.0, samples, 32);

    const int rc =
        std::system((bin("sig_clip").string() + " " + fil.string() + " -o " +
                     tim.string() + " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader reader(tim.string());
    REQUIRE(reader.hdr.get<int>("data_type") == 2);
    std::vector<float> block;
    reader.read_block(0, 8, block);
    REQUIRE(block == std::vector<float>(8, 1.F));

    const int rc_std =
        std::system((bin("sig_clip").string() + " " + fil.string() + " > " +
                     std.string() + " 2>/dev/null")
                        .c_str());
    REQUIRE(rc_std == 0);
    sigproc::FilterbankReader r2(std.string());
    REQUIRE(r2.hdr.get<int>("data_type") == 2);
    std::filesystem::remove(fil);
    std::filesystem::remove(tim);
    std::filesystem::remove(std);
}

TEST_CASE("sig_blanker --help lists original -s -f -P") {
    if (!std::filesystem::exists(bin("sig_blanker"))) {
        SKIP("sig_blanker not available");
    }
    const auto help = run_cmd(bin("sig_blanker").string() + " --help");
    REQUIRE(help.find("-s") != std::string::npos);
    REQUIRE(help.find("-f") != std::string::npos);
    REQUIRE(help.find("-P") != std::string::npos);
    REQUIRE(help.find("phase") != std::string::npos);
}

TEST_CASE("sig_blanker blanks phase 0.25-0.5 of a 1 s period") {
    if (!std::filesystem::exists(bin("sig_blanker"))) {
        SKIP("sig_blanker not available");
    }
    const auto dir = std::filesystem::temp_directory_path();
    const auto tim = dir / "blank_in.tim";
    const auto out = dir / "blank_out.tim";
    std::vector<float> samples(100);
    for (int i = 0; i < 100; ++i) {
        samples[static_cast<std::size_t>(i)] = 1000.F + static_cast<float>(i);
    }
    write_tim(tim, 100, 0.01, samples);

    const int rc =
        std::system((bin("sig_blanker").string() + " " + tim.string() +
                     " -s 0.25 -f 0.5 -P 1.0 --seed 1 -o " + out.string() +
                     " >/dev/null 2>/dev/null")
                        .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader reader(out.string());
    REQUIRE(reader.hdr.get<int>("nbits") == 32);
    REQUIRE(reader.hdr.get<int>("data_type") == 2);
    std::vector<float> got;
    reader.read_block(0, 100, got);
    REQUIRE(got.size() == 100);
    for (int i = 0; i < 100; ++i) {
        const double phase = static_cast<double>(i + 1) * 0.01; // period = 1 s
        const bool blank   = phase >= 0.25 && phase <= 0.5;
        if (blank) {
            REQUIRE(got[static_cast<std::size_t>(i)] !=
                    samples[static_cast<std::size_t>(i)]);
        } else {
            REQUIRE(got[static_cast<std::size_t>(i)] ==
                    samples[static_cast<std::size_t>(i)]);
        }
    }
    std::filesystem::remove(tim);
    std::filesystem::remove(out);
}
