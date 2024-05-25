/*
   header - show header parameters in a data file
 */
#include <CLI/CLI.hpp>
#include <fmt/core.h>

#include <sigproc/io.hpp>
#include <sigproc/params.hpp>

void print_header(const SigprocHeader& hdr) {
    constexpr auto kFormat = "{:<33}: {}\n";
    fmt::print(kFormat, "Data file", hdr.get<std::string>("rawdatafile"));
    fmt::print(kFormat, "Header size (bytes)", hdr.get<int>("header_size"));
    fmt::print(kFormat, "Data size (bytes)", hdr.get<int>("data_size"));
    fmt::print("{:<33}: {} ({})\n", "Data type",
               hdr.get<std::string>("datatype"), hdr.get<std::string>("frame"));
    fmt::print(kFormat, "Telescope", hdr.get<std::string>("telescope"));
    fmt::print(kFormat, "Datataking Machine", hdr.get<std::string>("backend"));
    fmt::print(kFormat, "Source Name", hdr.get<std::string>("source_name"));
    fmt::print(kFormat, "Source RA (J2000)", hdr.get<std::string>("ra"));
    fmt::print(kFormat, "Source DEC (J2000)", hdr.get<std::string>("dec"));
    fmt::print(kFormat, "Start AZ (deg)", hdr.get<double>("az_start"));
    fmt::print(kFormat, "Start ZA (deg)", hdr.get<double>("za_start"));

    switch (hdr.get<int>("data_type")) {
    case 0:
    case 1:
        fmt::print(kFormat, "Frequency of channel 1 (MHz)",
                   hdr.get<double>("fch1"));
        fmt::print(kFormat, "Channel bandwidth      (MHz)",
                   hdr.get<double>("foff"));
        fmt::print(kFormat, "Number of channels", hdr.get<int>("nchans"));
        fmt::print(kFormat, "Number of beams", hdr.get<int>("nbeams"));
        fmt::print(kFormat, "Beam number", hdr.get<int>("ibeam"));
        break;
    case 2:
        fmt::print(kFormat, "Reference DM (pc/cc)", hdr.get<double>("refdm"));
        fmt::print(kFormat, "Reference frequency    (MHz)",
                   hdr.get<double>("fch1"));
        fmt::print(kFormat, "Number of channels", 1);
        break;
    case 3:
        fmt::print(kFormat, "Reference DM (pc/cc)", hdr.get<double>("refdm"));
        fmt::print(kFormat, "Frequency of channel 1 (MHz)",
                   hdr.get<double>("fch1"));
        fmt::print(kFormat, "Channel bandwidth      (MHz)",
                   hdr.get<double>("foff"));
        fmt::print(kFormat, "Number of channels", hdr.get<int>("nchans"));
        fmt::print(kFormat, "Number of phase bins", hdr.get<int>("nbins"));
        fmt::print("{:<33}: {:.12f}\n", "Folding period  (s)",
                   hdr.get<double>("period"));
        break;
    case 6:
        fmt::print(kFormat, "Reference DM (pc/cc)", hdr.get<double>("refdm"));
        fmt::print(kFormat, "Frequency of channel 1 (MHz)",
                   hdr.get<double>("fch1"));
        fmt::print(kFormat, "Channel bandwidth      (MHz)",
                   hdr.get<double>("foff"));
        fmt::print(kFormat, "Number of channels", hdr.get<int>("nchans"));
        break;
    }

    std::string signedness = hdr.get<bool>("signed") ? "SIGNED" : "UNSIGNED";
    fmt::print(kFormat, "Signedness of 8-bit numbers", signedness);
    fmt::print("{:<33}: {:.12f}\n", "Time stamp of first sample (MJD)",
               hdr.get<double>("tstart"));
    fmt::print(kFormat, "Gregorian date (YYYY-MM-DD)",
               hdr.get<std::string>("obs_date"));

    if (hdr.get<int>("data_type") != 3) {
        fmt::print("{:<33}: {:.5f}\n", "Sample time (us)",
                   hdr.get<double>("tsamp") * 1.0e6);
        fmt::print(kFormat, "Number of samples", hdr.get<int>("nsamples"));
        fmt::print(kFormat, "Observation length",
                   hdr.get<std::string>("tobs_str"));
    }
    fmt::print(kFormat, "Number of bits per sample", hdr.get<int>("nbits"));
    fmt::print(kFormat, "Number of IFs", hdr.get<int>("nifs"));
}

void header_help() {
    for (const auto& param : kSigprocKeys) {
        fmt::print("{:<15}- return {}\n", param.first, param.second.helpstr);
    }
}

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

    FilterbankReader filreader(filename);

    if (!token.empty()) {
        std::map<std::string, sig_type> header_keys = sigproc_keys;
        header_keys.insert(extra_keys.begin(), extra_keys.end());
        if (utils::exists_key(header_keys, token)) {
            sig_type type = header_keys.at(token);
            switch (type) {
            case sig_type::sInt: {
                fmt::print("{}", filreader.hdr.get<int>(token));
                break;
            }
            case sig_type::sDouble: {
                fmt::print("{}", filreader.hdr.get<double>(token));
                break;
            }
            case sig_type::sBool: {
                fmt::print("{}", filreader.hdr.get<bool>(token));
                break;
            }
            case sig_type::sString: {
                fmt::print("{}", filreader.hdr.get<std::string>(token));
                break;
            }
            }
        } else {
            header_help();
            fmt::print(stderr, "Unknown argument {} passed to header\n", token);
            return 0;
        }
    } else {
        /* no command-line flags were specified - display full output */
        print_header(filreader.hdr);
    }
    return 0;
}
