/*
   header - show header parameters in a data file
 */
#include <format>
#include <iostream>
#include <string>
#include <unordered_map>

#include <CLI/CLI.hpp>

#include <sigproc/common/params.hpp>
#include <sigproc/common/types.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>

namespace {

// Print a "<label>: <value>" line with the label left-padded to a fixed width.
template <typename T> void print_kv(std::string_view label, const T& value) {
    std::cout << std::format("{:<33}: {}\n", label, value);
}

void print_header(const sigproc::io::SigprocHeader& hdr) {
    print_kv("Data file", hdr.get<std::string>("rawdatafile"));
    print_kv("Header size (bytes)", hdr.get<int>("header_size"));
    print_kv("Data size (bytes)", hdr.get<int>("data_size"));
    std::cout << std::format("{:<33}: {} ({})\n", "Data type",
                             hdr.get<std::string>("datatype"),
                             hdr.get<std::string>("frame"));
    print_kv("Telescope", hdr.get<std::string>("telescope"));
    print_kv("Datataking Machine", hdr.get<std::string>("backend"));
    print_kv("Source Name", hdr.get<std::string>("source_name"));
    print_kv("Source RA (J2000)", hdr.get<std::string>("ra"));
    print_kv("Source DEC (J2000)", hdr.get<std::string>("dec"));
    print_kv("Start AZ (deg)", hdr.get<double>("az_start"));
    print_kv("Start ZA (deg)", hdr.get<double>("za_start"));

    switch (hdr.get<int>("data_type")) {
    case 0:
    case 1:
        print_kv("Frequency of channel 1 (MHz)", hdr.get<double>("fch1"));
        print_kv("Channel bandwidth      (MHz)", hdr.get<double>("foff"));
        print_kv("Number of channels", hdr.get<int>("nchans"));
        print_kv("Number of beams", hdr.get<int>("nbeams"));
        print_kv("Beam number", hdr.get<int>("ibeam"));
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

    std::string signedness = hdr.get<bool>("signed") ? "SIGNED" : "UNSIGNED";
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

void header_help() {
    for (const auto& [key, info] : sigproc::params::kSigprocKeys) {
        std::cout << std::format("{:<15}- return {}\n", key, info.helpstr);
    }
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"header  - examine header parameters of filterbank data"};

    std::string filename;
    app.add_option("filename", filename, "the filterbank data file")
        ->check(CLI::ExistingFile);

    std::string token;
    app.add_option("-k,--key", token,
                   "A header key to read (e.g. telescope, fch1, nsamples");

    bool keys_help_flag{false};
    app.add_flag("--keys_help", keys_help_flag,
                 "Print header keys help message and exit.");

    CLI11_PARSE(app, argc, argv);

    if (keys_help_flag) {
        header_help();
        return 0;
    }

    sigproc::FilterbankReader filreader(filename);

    if (!token.empty()) {
        std::unordered_map<std::string, sigproc::params::KeyInfo> header_keys =
            sigproc::params::kSigprocKeys;
        header_keys.insert(sigproc::params::kExtraKeys.begin(),
                           sigproc::params::kExtraKeys.end());
        const auto it = header_keys.find(token);
        if (it != header_keys.end()) {
            switch (it->second.type) {
            case sigproc::KeyType::kSInt:
                std::cout << std::format("{}", filreader.hdr.get<int>(token));
                break;
            case sigproc::KeyType::kSDouble:
                std::cout << std::format("{}",
                                         filreader.hdr.get<double>(token));
                break;
            case sigproc::KeyType::kSBool:
                std::cout << std::format("{}", filreader.hdr.get<bool>(token));
                break;
            case sigproc::KeyType::kSString:
                std::cout << std::format("{}",
                                         filreader.hdr.get<std::string>(token));
                break;
            }
        } else {
            header_help();
            std::cerr << std::format("Unknown argument {} passed to header\n",
                                     token);
            return 0;
        }
    } else {
        /* no command-line flags were specified - display full output */
        print_header(filreader.hdr);
    }
    return 0;
}
