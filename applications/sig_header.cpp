/*
   header - show header parameters in a data file
 */
#include <CLI/CLI.hpp>

#include <sigproc/params.hpp>
#include <sigproc/io.hpp>

void print_header(const SigprocHeader& hdr) {
    std::string format = "{:<33}: {}\n";
    fmt::print(format, "Data file", hdr.get<std::string>("rawdatafile"));
    fmt::print(format, "Header size (bytes)", hdr.get<int>("header_size"));
    fmt::print(format, "Data size (bytes)", hdr.get<int>("data_size"));
    fmt::print("{:<33}: {} ({})\n", "Data type",
               hdr.get<std::string>("datatype"), hdr.get<std::string>("frame"));
    fmt::print(format, "Telescope", hdr.get<std::string>("telescope"));
    fmt::print(format, "Datataking Machine", hdr.get<std::string>("backend"));
    fmt::print(format, "Source Name", hdr.get<std::string>("source_name"));
    fmt::print(format, "Source RA (J2000)", hdr.get<std::string>("ra"));
    fmt::print(format, "Source DEC (J2000)", hdr.get<std::string>("dec"));
    fmt::print(format, "Start AZ (deg)", hdr.get<double>("az_start"));
    fmt::print(format, "Start ZA (deg)", hdr.get<double>("za_start"));

    switch (hdr.get<int>("data_type")) {
        case 0:
        case 1:
            fmt::print(format, "Frequency of channel 1 (MHz)",
                       hdr.get<double>("fch1"));
            fmt::print(format, "Channel bandwidth      (MHz)",
                       hdr.get<double>("foff"));
            fmt::print(format, "Number of channels", hdr.get<int>("nchans"));
            fmt::print(format, "Number of beams", hdr.get<int>("nbeams"));
            fmt::print(format, "Beam number", hdr.get<int>("ibeam"));
            break;
        case 2:
            fmt::print(format, "Reference DM (pc/cc)",
                       hdr.get<double>("refdm"));
            fmt::print(format, "Reference frequency    (MHz)",
                       hdr.get<double>("fch1"));
            fmt::print(format, "Number of channels", 1);
            break;
        case 3:
            fmt::print(format, "Reference DM (pc/cc)",
                       hdr.get<double>("refdm"));
            fmt::print(format, "Frequency of channel 1 (MHz)",
                       hdr.get<double>("fch1"));
            fmt::print(format, "Channel bandwidth      (MHz)",
                       hdr.get<double>("foff"));
            fmt::print(format, "Number of channels", hdr.get<int>("nchans"));
            fmt::print(format, "Number of phase bins", hdr.get<int>("nbins"));
            fmt::print("{:<33}: {:.12f}\n", "Folding period  (s)",
                       hdr.get<double>("period"));
            break;
        case 6:
            fmt::print(format, "Reference DM (pc/cc)",
                       hdr.get<double>("refdm"));
            fmt::print(format, "Frequency of channel 1 (MHz)",
                       hdr.get<double>("fch1"));
            fmt::print(format, "Channel bandwidth      (MHz)",
                       hdr.get<double>("foff"));
            fmt::print(format, "Number of channels", hdr.get<int>("nchans"));
            break;
    }

    std::string signedness = hdr.get<bool>("signed") ? "SIGNED" : "UNSIGNED";
    fmt::print(format, "Signedness of 8-bit numbers", signedness);
    fmt::print("{:<33}: {:.12f}\n", "Time stamp of first sample (MJD)",
               hdr.get<double>("tstart"));
    fmt::print(format, "Gregorian date (YYYY-MM-DD)",
               hdr.get<std::string>("obs_date"));

    if (hdr.get<int>("data_type") != 3) {
        fmt::print("{:<33}: {:.5f}\n", "Sample time (us)",
                   hdr.get<double>("tsamp") * 1.0e6);
        fmt::print(format, "Number of samples", hdr.get<int>("nsamples"));
        dates::Duration_print dur
            = dates::get_duration_print(hdr.get<double>("tobs"));
        std::string obs_msg = fmt::format("Observation length {}", dur.unit);
        fmt::print("{:<33}: {:.1f}\n", obs_msg, dur.duration);
    }
    fmt::print(format, "Number of bits per sample", hdr.get<int>("nbits"));
    fmt::print(format, "Number of IFs", hdr.get<int>("nifs"));
}


void header_help() {
    for (const auto& param : header_keys_help) {
        fmt::print("{:<15}- return {}\n", param.first, param.second);
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
