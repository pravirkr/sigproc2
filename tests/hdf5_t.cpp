#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <hdf5.h>

#include <sigproc/bits.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>

#include "fil_test_utils.hpp"

#ifndef SIG_TEST_DATA_DIR
#define SIG_TEST_DATA_DIR "."
#endif

#ifndef SIG_BIN_DIR
#define SIG_BIN_DIR "."
#endif

namespace {

std::filesystem::path tmp_path(std::string_view name) {
    return std::filesystem::temp_directory_path() / name;
}

sigproc::io::SigprocHeader
header_with_nbits(int nbits, int nchans, int nsamps) {
    sigproc::io::SigprocHeader hdr;
    hdr.set("source_name", std::string("RAMP"));
    hdr.set("machine_id", 0);
    hdr.set("telescope_id", 0);
    hdr.set("data_type", 1);
    hdr.set("nchans", nchans);
    hdr.set("nbits", nbits);
    hdr.set("nifs", 1);
    hdr.set("nsamples", nsamps);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -1.0);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", 50000.0);
    return hdr;
}

std::vector<float> ramp(int nbits, int nchans, int nsamps) {
    sigproc::bits::BitsInfo info(static_cast<sigproc::SizeType>(nbits));
    const auto maxv = static_cast<int>(info.get_digi_max());
    std::vector<float> samples(static_cast<std::size_t>(nchans * nsamps));
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<float>(static_cast<int>(i) % (maxv + 1));
    }
    return samples;
}

void write_fil(const std::filesystem::path& path,
               sigproc::io::SigprocHeader hdr,
               const std::vector<float>& samples) {
    sigproc::FilterbankWriter writer(path.string(), hdr);
    writer.write_block(samples, static_cast<int>(samples.size()));
}

void copy_fil_to_h5(const std::filesystem::path& fil,
                    const std::filesystem::path& h5) {
    sigproc::FilterbankReader reader(fil.string());
    std::vector<float> block;
    reader.read_block(0, reader.nsamps(), block);
    sigproc::FilterbankWriter writer(h5.string(), reader.hdr);
    writer.write_block(block, static_cast<int>(block.size()));
}

[[nodiscard]] std::optional<std::string>
hdf5_file_string_attr(const std::string& path, const char* name) {
    const hid_t file = H5Fopen(path.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) {
        return std::nullopt;
    }
    if (H5Aexists(file, name) <= 0) {
        H5Fclose(file);
        return std::nullopt;
    }
    const hid_t attr  = H5Aopen(file, name, H5P_DEFAULT);
    const hid_t ftype = H5Aget_type(attr);
    const hid_t mem   = H5Tcopy(ftype);
    std::string out;
    if (H5Tis_variable_str(ftype) > 0) {
        char* buf = nullptr;
        if (H5Aread(attr, mem, &buf) >= 0 && buf != nullptr) {
            out = buf;
            H5free_memory(buf);
        }
    } else {
        const auto sz = H5Tget_size(ftype);
        std::string buf(sz, '\0');
        if (H5Aread(attr, mem, buf.data()) >= 0) {
            if (const auto n = buf.find('\0'); n != std::string::npos) {
                buf.resize(n);
            }
            out = std::move(buf);
        }
    }
    H5Tclose(mem);
    H5Tclose(ftype);
    H5Aclose(attr);
    H5Fclose(file);
    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}

std::string run_cmd(const std::string& cmd) {
    const auto tmp = tmp_path("sigproc2_hdf5_cli.txt");
    const int rc =
        std::system((cmd + " > " + tmp.string() + " 2>/dev/null").c_str());
    std::ifstream in(tmp);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::filesystem::remove(tmp);
    REQUIRE(rc == 0);
    return ss.str();
}

} // namespace

TEST_CASE("is_hdf5_path matches FBH5 suffixes and rejects stdio") {
    REQUIRE(sigproc::is_hdf5_path("foo.h5"));
    REQUIRE(sigproc::is_hdf5_path("foo.H5"));
    REQUIRE(sigproc::is_hdf5_path("foo.hdf5"));
    REQUIRE(sigproc::is_hdf5_path("foo.fbh5"));
    REQUIRE_FALSE(sigproc::is_hdf5_path("foo.fil"));
    REQUIRE_FALSE(sigproc::is_hdf5_path("-"));
    REQUIRE_FALSE(sigproc::is_hdf5_path(""));
}

TEST_CASE("HDF5 stdio and stream constructors throw") {
    const std::string hdf5_magic{"\x89HDF\r\n\x1a\nXXXX"};
    std::istringstream in(hdf5_magic, std::ios::binary);
    REQUIRE_THROWS_AS(sigproc::FilterbankReader(in), std::invalid_argument);
    try {
        std::istringstream in2(hdf5_magic, std::ios::binary);
        sigproc::FilterbankReader reader(in2);
        FAIL("expected throw");
    } catch (const std::invalid_argument& ex) {
        REQUIRE(std::string(ex.what()).find(
                    "HDF5 requires a filesystem path") != std::string::npos);
    }
}

TEST_CASE("tiny.fil copies to .h5 and round-trips header plus gulp") {
    const auto fil = std::filesystem::path(SIG_TEST_DATA_DIR) / "tiny.fil";
    REQUIRE(std::filesystem::exists(fil));
    const auto h5 = tmp_path("tiny_roundtrip.h5");
    copy_fil_to_h5(fil, h5);

    sigproc::FilterbankReader fil_r(fil.string());
    sigproc::FilterbankReader h5_r(h5.string());
    REQUIRE(h5_r.hdr.get<int>("nchans") == fil_r.hdr.get<int>("nchans"));
    REQUIRE(h5_r.hdr.get<int>("nifs") == fil_r.hdr.get<int>("nifs"));
    REQUIRE(h5_r.hdr.get<double>("fch1") == fil_r.hdr.get<double>("fch1"));
    REQUIRE(h5_r.hdr.get<double>("tsamp") == fil_r.hdr.get<double>("tsamp"));
    REQUIRE(h5_r.nsamps() == fil_r.nsamps());

    std::vector<float> fil_block;
    std::vector<float> h5_block;
    fil_r.read_block(0, fil_r.nsamps(), fil_block);
    h5_r.read_block(0, h5_r.nsamps(), h5_block);
    REQUIRE(h5_block == fil_block);

    const auto cls = hdf5_file_string_attr(h5.string(), "CLASS");
    REQUIRE(cls.has_value());
    REQUIRE(*cls == "FILTERBANK");
    const auto ver = hdf5_file_string_attr(h5.string(), "VERSION");
    REQUIRE(ver.has_value());
    REQUIRE(*ver == "2.0");

    sigproc::io::SigprocHeader fromfile_hdr;
    REQUIRE(fromfile_hdr.fromfile(h5.string()));
    REQUIRE(fromfile_hdr.get<int>("nchans") == 8);
    REQUIRE(fromfile_hdr.get<int>("nsamples") == 16);

    std::filesystem::remove(h5);
}

TEST_CASE("32-bit filterbank survives fil to h5 to fil") {
    const int nchans   = 8;
    const int nsamps   = 4;
    auto hdr           = header_with_nbits(32, nchans, nsamps);
    const auto samples = ramp(32, nchans, nsamps);
    const auto fil1    = tmp_path("fbh5_32_a.fil");
    const auto h5      = tmp_path("fbh5_32.h5");
    const auto fil2    = tmp_path("fbh5_32_b.fil");

    write_fil(fil1, hdr, samples);
    copy_fil_to_h5(fil1, h5);
    copy_fil_to_h5(h5, fil2);

    sigproc::FilterbankReader a(fil1.string());
    sigproc::FilterbankReader b(fil2.string());
    REQUIRE(b.hdr.get<int>("nchans") == a.hdr.get<int>("nchans"));
    REQUIRE(b.hdr.get<int>("nbits") == 32);
    REQUIRE(b.hdr.get<double>("fch1") == a.hdr.get<double>("fch1"));
    REQUIRE(b.hdr.get<double>("tsamp") == a.hdr.get<double>("tsamp"));
    REQUIRE(b.hdr.get<double>("tstart") == a.hdr.get<double>("tstart"));
    std::vector<float> aa;
    std::vector<float> bb;
    a.read_block(0, nsamps, aa);
    b.read_block(0, nsamps, bb);
    REQUIRE(aa == samples);
    REQUIRE(bb == samples);

    std::filesystem::remove(fil1);
    std::filesystem::remove(h5);
    std::filesystem::remove(fil2);
}

TEST_CASE("4-bit packed FBH5 round-trip") {
    const int nchans   = 8;
    const int nsamps   = 4;
    auto hdr           = header_with_nbits(4, nchans, nsamps);
    const auto samples = ramp(4, nchans, nsamps);
    const auto h5      = tmp_path("fbh5_4bit.h5");
    const auto hdf5    = tmp_path("fbh5_4bit.hdf5");
    const auto fbh5    = tmp_path("fbh5_4bit.fbh5");

    {
        sigproc::FilterbankWriter writer(h5.string(), hdr);
        writer.write_block(samples, static_cast<int>(samples.size()));
    }
    sigproc::FilterbankReader reader(h5.string());
    REQUIRE(reader.hdr.get<int>("nbits") == 4);
    std::vector<float> block;
    reader.read_block(0, nsamps, block);
    REQUIRE(block == samples);

    std::filesystem::copy_file(
        h5, hdf5, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        h5, fbh5, std::filesystem::copy_options::overwrite_existing);
    sigproc::FilterbankReader r_hdf5(hdf5.string());
    sigproc::FilterbankReader r_fbh5(fbh5.string());
    std::vector<float> b2;
    std::vector<float> b3;
    r_hdf5.read_block(0, nsamps, b2);
    r_fbh5.read_block(0, nsamps, b3);
    REQUIRE(b2 == samples);
    REQUIRE(b3 == samples);

    std::filesystem::remove(h5);
    std::filesystem::remove(hdf5);
    std::filesystem::remove(fbh5);
}

TEST_CASE("HDF5 magic dispatches even without an h5 suffix") {
    auto hdr           = header_with_nbits(8, 8, 2);
    const auto samples = ramp(8, 8, 2);
    const auto h5      = tmp_path("fbh5_magic.h5");
    const auto dat     = tmp_path("fbh5_magic.dat");
    {
        sigproc::FilterbankWriter writer(h5.string(), hdr);
        writer.write_block(samples, static_cast<int>(samples.size()));
    }
    std::filesystem::copy_file(
        h5, dat, std::filesystem::copy_options::overwrite_existing);
    sigproc::FilterbankReader reader(dat.string());
    REQUIRE(reader.hdr.get<int>("nchans") == 8);
    std::vector<float> block;
    reader.read_block(0, 2, block);
    REQUIRE(block == samples);
    std::filesystem::remove(h5);
    std::filesystem::remove(dat);
}

TEST_CASE("sig_header and sig_extract work on FBH5") {
    const auto h5bin = std::filesystem::path(SIG_BIN_DIR) / "sig_header";
    const auto xbin  = std::filesystem::path(SIG_BIN_DIR) / "sig_extract";
    if (!std::filesystem::exists(h5bin) || !std::filesystem::exists(xbin)) {
        SKIP("sig_header / sig_extract not available");
    }
    auto hdr           = header_with_nbits(8, 8, 8);
    const auto samples = ramp(8, 8, 8);
    const auto h5      = tmp_path("fbh5_cli.h5");
    const auto out     = tmp_path("fbh5_extract.fil");
    {
        sigproc::FilterbankWriter writer(h5.string(), hdr);
        writer.write_block(samples, static_cast<int>(samples.size()));
    }
    const auto printed = run_cmd(h5bin.string() + " -nchans " + h5.string());
    REQUIRE(printed.find("8") != std::string::npos);

    const int rc = std::system((xbin.string() + " " + h5.string() + " 1 4 -o " +
                                out.string() + " >/dev/null 2>/dev/null")
                                   .c_str());
    REQUIRE(rc == 0);
    sigproc::FilterbankReader extracted(out.string());
    REQUIRE(extracted.nsamps() == 4);
    std::vector<float> got;
    extracted.read_block(0, 4, got);
    REQUIRE(got == std::vector<float>(samples.begin(), samples.begin() + 32));

    std::filesystem::remove(h5);
    std::filesystem::remove(out);
}

TEST_CASE("public headers do not include HighFive") {
    const auto root = std::filesystem::path(SIG_TEST_DATA_DIR).parent_path() /
                      ".." / "include" / "sigproc";
    if (!std::filesystem::exists(root)) {
        SKIP("public include tree not found from test data dir");
    }
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::ifstream in(entry.path());
        std::string line;
        while (std::getline(in, line)) {
            REQUIRE_FALSE(line.find("highfive") != std::string::npos);
            REQUIRE_FALSE(line.find("HighFive") != std::string::npos);
            REQUIRE_FALSE(line.find("hdf5.h") != std::string::npos);
            REQUIRE_FALSE(line.find("H5public") != std::string::npos);
        }
    }
}
