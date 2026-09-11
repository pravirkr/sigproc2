/*
   header - show header parameters in a data file
 */
#include <cmath>
#include <cstdlib>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <CLI/CLI.hpp>

#include <sigproc/common/params.hpp>
#include <sigproc/common/types.hpp>
#include <sigproc/header.hpp>

#include "cli_utils.hpp"

namespace {

template <typename T> void print_kv(std::string_view label, const T& value) {
    std::cout << std::format("{:<33}: {}\n", label, value);
}

void print_header(const sigproc::io::SigprocHeader& hdr,
                  std::string_view filename) {
    print_kv("Data file", filename);
    print_kv("Header size (bytes)", hdr.get<int>("header_size"));
    const auto data_size = hdr.get<int>("data_size");
    if (data_size > 0) {
        print_kv("Data size (bytes)", data_size);
    }
    std::cout << std::format("{:<33}: {} ({})\n", "Data type",
                             hdr.get<std::string>("datatype"),
                             hdr.get<std::string>("frame"));
    print_kv("Telescope", hdr.get<std::string>("telescope"));
    print_kv("Datataking Machine", hdr.get<std::string>("backend"));
    const auto source = hdr.get<std::string>("source_name");
    if (!source.empty()) {
        print_kv("Source Name", source);
    }
    print_kv("Source RA (J2000)", hdr.get<std::string>("ra"));
    print_kv("Source DEC (J2000)", hdr.get<std::string>("dec"));
    print_kv("Start AZ (deg)", hdr.get<double>("az_start"));
    print_kv("Start ZA (deg)", hdr.get<double>("za_start"));

    switch (hdr.get<int>("data_type")) {
    case 0:
    case 1:
        if (hdr.has_freq_table()) {
            const auto table = hdr.get_freq_table();
            print_kv("Highest frequency channel (MHz)", table.front());
            print_kv("Lowest frequency channel (MHz)", table.back());
        } else {
            print_kv("Frequency of channel 1 (MHz)", hdr.get<double>("fch1"));
            print_kv("Channel bandwidth      (MHz)", hdr.get<double>("foff"));
            print_kv("Number of channels", hdr.get<int>("nchans"));
            print_kv("Number of beams", hdr.get<int>("nbeams"));
            print_kv("Beam number", hdr.get<int>("ibeam"));
        }
        break;
    case 2:
        print_kv("Reference DM (pc/cc)", hdr.get<double>("refdm"));
        print_kv("Reference frequency    (MHz)", hdr.get<double>("fch1"));
        print_kv("Number of channels", 1);
        break;
    case 3:
        print_kv("Reference DM (pc/cc)", hdr.get<double>("refdm"));
        print_kv("Frequency of channel 1 (MHz)", hdr.get<double>("fch1"));
        print_kv("Channel bandwidth      (MHz)", hdr.get<double>("foff"));
        print_kv("Number of channels", hdr.get<int>("nchans"));
        print_kv("Number of phase bins", hdr.get<int>("nbins"));
        std::cout << std::format("{:<33}: {:.12f}\n", "Folding period  (s)",
                                 hdr.get<double>("period"));
        break;
    case 6:
        print_kv("Reference DM (pc/cc)", hdr.get<double>("refdm"));
        print_kv("Frequency of channel 1 (MHz)", hdr.get<double>("fch1"));
        print_kv("Channel bandwidth      (MHz)", hdr.get<double>("foff"));
        print_kv("Number of channels", hdr.get<int>("nchans"));
        break;
    default:
        break;
    }

    // K22: true means signed (isign < 0). Original: isign>0 → UNSIGNED.
    const std::string signedness =
        hdr.get<bool>("signed") ? "SIGNED" : "UNSIGNED";
    print_kv("Signedness of 8-bit numbers", signedness);
    std::cout << std::format("{:<33}: {:.12f}\n",
                             "Time stamp of first sample (MJD)",
                             hdr.get<double>("tstart"));
    print_kv("Gregorian date (YYYY-MM-DD)", hdr.get<std::string>("obs_date"));

    if (hdr.get<int>("data_type") != 3) {
        std::cout << std::format("{:<33}: {:.5f}\n", "Sample time (us)",
                                 hdr.get<double>("tsamp") * 1.0e6);
        print_kv("Number of samples", hdr.get<int>("nsamples"));
        print_kv("Observation length", hdr.get<std::string>("tobs_str"));
    }
    print_kv("Number of bits per sample", hdr.get<int>("nbits"));
    print_kv("Number of IFs", hdr.get<int>("nifs"));
}

[[nodiscard]] sigproc::io::SigprocHeader
load_header(const std::string& filename) {
    sigproc::io::SigprocHeader hdr;
    if (sigproc::cli::is_stdio_path(filename)) {
        if (!hdr.fromstream(std::cin)) {
            throw std::runtime_error(
                "could not read header parameters from stdin");
        }
    } else if (!hdr.fromfile(filename)) {
        throw std::runtime_error(
            std::format("could not read header parameters from {}", filename));
    }
    return hdr;
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"header - examine header parameters of filterbank data"};
    app.footer("Original short flags keep original units. -tsamp prints "
               "microseconds; -k tsamp prints seconds.");

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    std::string key;
    app.add_option("-k,--key", key,
                   "Print a header key in native stored units (tsamp in s)");

    bool telescope     = false;
    bool machine       = false;
    bool source_name   = false;
    bool scan_number   = false;
    bool datatype      = false;
    bool frame         = false;
    bool data_type     = false;
    bool headersize    = false;
    bool datasize      = false;
    bool nsamples      = false;
    bool tobs          = false;
    bool az_start      = false;
    bool za_start      = false;
    bool fch1          = false;
    bool bandwidth     = false;
    bool fmid          = false;
    bool foff          = false;
    bool refdm         = false;
    bool nchans        = false;
    bool tstart        = false;
    bool frequencies   = false;
    bool mjd           = false;
    bool date          = false;
    bool utstart       = false;
    bool tsamp         = false;
    bool nbits         = false;
    bool ibeam         = false;
    bool nbeam         = false;
    bool nifs          = false;
    bool src_raj       = false;
    bool src_dej       = false;
    bool ra_deg        = false;
    bool dec_deg       = false;
    bool barycentric   = false;
    bool pulsarcentric = false;

    app.add_flag("-telescope", telescope, "Print telescope name");
    app.add_flag("-machine", machine, "Print backend / machine name");
    app.add_flag("-source_name", source_name, "Print source name");
    app.add_flag("-scan_number", scan_number,
                 "Print scan number (0 if absent)");
    app.add_flag("-datatype", datatype, "Print data category string");
    app.add_flag("-frame", frame, "Print bary/pulsar/topocentric");
    app.add_flag("-data_type", data_type, "Print data_type id");
    app.add_flag("-headersize", headersize, "Print header size in bytes");
    app.add_flag("-datasize", datasize, "Print data size in bytes");
    app.add_flag("-nsamples", nsamples, "Print number of time samples");
    app.add_flag("-tobs", tobs, "Print observation length in seconds");
    app.add_flag("-az_start", az_start, "Print start azimuth (deg)");
    app.add_flag("-za_start", za_start, "Print start zenith angle (deg)");
    app.add_flag("-fch1", fch1, "Print frequency of channel 1 (MHz)");
    app.add_flag("-bandwidth", bandwidth, "Print |foff|*nchans (MHz)");
    app.add_flag("-fmid", fmid, "Print band centre (MHz)");
    app.add_flag("-foff", foff, "Print channel bandwidth (MHz)");
    app.add_flag("-refdm,--dm", refdm, "Print reference DM");
    app.add_flag("-nchans", nchans, "Print number of channels");
    app.add_flag("-tstart", tstart, "Print MJD of first sample");
    app.add_flag("-frequencies", frequencies, "Print channel frequencies");
    app.add_flag("-mjd", mjd, "Print integer MJD");
    app.add_flag("-date", date, "Print Gregorian date YYYY/MM/DD");
    app.add_flag("-utstart", utstart, "Print UT start HH:MM:SS");
    app.add_flag("-tsamp", tsamp,
                 "Print sample time in microseconds (tsamp*1e6)");
    app.add_flag("-nbits", nbits, "Print bits per sample");
    app.add_flag("-ibeam", ibeam, "Print beam number");
    app.add_flag("-nbeam", nbeam, "Print number of beams");
    app.add_flag("-nifs", nifs, "Print number of IFs");
    app.add_flag("-src_raj", src_raj, "Print RA as hh:mm:ss");
    app.add_flag("-src_dej", src_dej, "Print Dec as dd:mm:ss");
    app.add_flag("-ra_deg", ra_deg, "Print RA in degrees");
    app.add_flag("-dec_deg", dec_deg, "Print Dec in degrees");
    app.add_flag("-barycentric", barycentric, "Print barycentric flag");
    app.add_flag("-pulsarcentric", pulsarcentric, "Print pulsarcentric flag");

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    auto hdr = load_header(filename);

    const bool any_flag =
        telescope || machine || source_name || scan_number || datatype ||
        frame || data_type || headersize || datasize || nsamples || tobs ||
        az_start || za_start || fch1 || bandwidth || fmid || foff || refdm ||
        nchans || tstart || frequencies || mjd || date || utstart || tsamp ||
        nbits || ibeam || nbeam || nifs || src_raj || src_dej || ra_deg ||
        dec_deg || barycentric || pulsarcentric || !key.empty();

    if (!any_flag) {
        print_header(hdr, sigproc::cli::is_stdio_path(filename) ? "stdin"
                                                                : filename);
        return 0;
    }

    if (!key.empty()) {
        std::unordered_map<std::string, sigproc::params::KeyInfo> header_keys =
            sigproc::params::kSigprocKeys;
        header_keys.insert(sigproc::params::kExtraKeys.begin(),
                           sigproc::params::kExtraKeys.end());
        const auto it = header_keys.find(key);
        if (it == header_keys.end()) {
            std::cerr << std::format("Unknown argument {} passed to header\n",
                                     key);
            return 1;
        }
        switch (it->second.type) {
        case sigproc::KeyType::kSInt:
            std::cout << hdr.get<int>(key) << '\n';
            break;
        case sigproc::KeyType::kSDouble:
            std::cout << hdr.get<double>(key) << '\n';
            break;
        case sigproc::KeyType::kSBool:
            std::cout << hdr.get<bool>(key) << '\n';
            break;
        case sigproc::KeyType::kSString:
            std::cout << hdr.get<std::string>(key) << '\n';
            break;
        }
        return 0;
    }

    if (telescope) {
        std::cout << hdr.get<std::string>("telescope") << '\n';
    }
    if (machine) {
        std::cout << hdr.get<std::string>("backend") << '\n';
    }
    if (source_name) {
        std::cout << hdr.get<std::string>("source_name") << '\n';
    }
    if (scan_number) {
        std::cout << 0 << '\n';
    }
    if (datatype) {
        std::cout << hdr.get<std::string>("datatype") << '\n';
    }
    if (frame) {
        std::cout << hdr.get<std::string>("frame") << '\n';
    }
    if (data_type) {
        std::cout << hdr.get<int>("data_type") << '\n';
    }
    if (headersize) {
        std::cout << hdr.get<int>("header_size") << '\n';
    }
    if (datasize) {
        std::cout << hdr.get<int>("data_size") << '\n';
    }
    if (nsamples) {
        std::cout << hdr.get<int>("nsamples") << '\n';
    }
    if (tobs) {
        std::cout << hdr.get<double>("tobs") << '\n';
    }
    if (az_start) {
        std::cout << hdr.get<double>("az_start") << '\n';
    }
    if (za_start) {
        std::cout << hdr.get<double>("za_start") << '\n';
    }
    if (fch1) {
        std::cout << std::format("{:.3f}\n", hdr.get<double>("fch1"));
    }
    if (bandwidth) {
        std::cout << std::format("{:.3f}\n", hdr.get<double>("bandwidth"));
    }
    if (fmid) {
        std::cout << std::format("{:.3f}\n", hdr.get<double>("fcenter"));
    }
    if (foff) {
        std::cout << hdr.get<double>("foff") << '\n';
    }
    if (refdm) {
        std::cout << hdr.get<double>("refdm") << '\n';
    }
    if (nchans) {
        std::cout << hdr.get<int>("nchans") << '\n';
    }
    if (tstart) {
        std::cout << std::format("{:.12f}\n", hdr.get<double>("tstart"));
    }
    if (frequencies) {
        for (double freq : hdr.get_freq_table()) {
            std::cout << freq << '\n';
        }
    }
    if (mjd) {
        std::cout << static_cast<int>(std::floor(hdr.get<double>("tstart")))
                  << '\n';
    }
    if (date) {
        const auto greg = hdr.get<std::string>("obs_date"); // YYYY-MM-DD
        if (greg.size() == 10) {
            std::cout << std::format("{}/{}/{}\n", greg.substr(0, 4),
                                     greg.substr(5, 2), greg.substr(8, 2));
        } else {
            std::cout << greg << '\n';
        }
    }
    if (utstart) {
        double frac = hdr.get<double>("tstart");
        frac -= std::floor(frac);
        const int uth = static_cast<int>(std::floor(24.0 * frac));
        frac -= static_cast<double>(uth) / 24.0;
        const int utm = static_cast<int>(std::floor(1440.0 * frac));
        frac -= static_cast<double>(utm) / 1440.0;
        const int uts = static_cast<int>(std::floor(86400.0 * frac));
        std::cout << std::format("{:02d}:{:02d}:{:02d}\n", uth, utm, uts);
    }
    if (tsamp) {
        std::cout << std::format("{:.5f}\n", hdr.get<double>("tsamp") * 1.0e6);
    }
    if (nbits) {
        std::cout << hdr.get<int>("nbits") << '\n';
    }
    if (ibeam) {
        std::cout << hdr.get<int>("ibeam") << '\n';
    }
    if (nbeam) {
        std::cout << hdr.get<int>("nbeams") << '\n';
    }
    if (nifs) {
        std::cout << hdr.get<int>("nifs") << '\n';
    }
    if (src_raj) {
        std::cout << hdr.get<std::string>("ra") << '\n';
    }
    if (src_dej) {
        std::cout << hdr.get<std::string>("dec") << '\n';
    }
    if (ra_deg) {
        std::cout << hdr.get<double>("ra_rad") *
                         (180.0 / 3.14159265358979323846)
                  << '\n';
    }
    if (dec_deg) {
        std::cout << hdr.get<double>("dec_rad") *
                         (180.0 / 3.14159265358979323846)
                  << '\n';
    }
    if (barycentric) {
        std::cout << (hdr.get<bool>("barycentric") ? 1 : 0) << '\n';
    }
    if (pulsarcentric) {
        std::cout << (hdr.get<bool>("pulsarcentric") ? 1 : 0) << '\n';
    }
    return 0;
}
