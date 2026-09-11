/*
    DECIMATE  - decimate filterbank data by adding channels and/or time samples
*/

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include <sigproc/bits.hpp>
#include <sigproc/common/types.hpp>
#include <sigproc/filterbank.hpp>
#include <sigproc/kernels.hpp>

#include "cli_utils.hpp"

int main(int argc, char** argv) {
    CLI::App app{"decimate - reduce time and/or frequency resolution of "
                 "filterbank data"};
    sigproc::cli::configure_app(app);

    std::string filename;
    sigproc::cli::add_input_file(app, filename);

    std::string outfile;
    sigproc::cli::add_output_file(app, outfile);

    int naddc = 0;
    app.add_option("-c,--numchans", naddc,
                   "Channels to add (omitted or <1 → all nchans)");

    int naddt = 1;
    app.add_option("-t,--numsamps", naddt,
                   "Time samples to add (def=1; <=1 → 1)");

    int out_nsamp = 0;
    app.add_option("-T,--out-nsamp", out_nsamp,
                   "Set time factor from nsamples/out_nsamp (even)");

    int gulp = sigproc::cli::kDefaultGulp;
    sigproc::cli::add_gulp_flag(app, gulp);

    int out_nbits = 0;
    app.add_option("-n,--nbits", out_nbits, "Output bits (0 → input nbits)");

    bool headerless = false;
    app.add_flag("-headerless,--headerless", headerless,
                 "Skip writing the output header");

    bool verbose = false;
    bool debug   = false;
    sigproc::cli::add_log_flags(app, verbose, debug);

    CLI11_PARSE(app, argc, argv);
    sigproc::cli::init_logging(verbose, debug);

    sigproc::FilterbankReader reader(filename);

    const int in_nchans = reader.hdr.get<int>("nchans");
    const int in_nifs   = reader.hdr.get<int>("nifs");
    const int in_nbits  = reader.hdr.get<int>("nbits");
    const int in_nsamp  = reader.hdr.get<int>("nsamples");

    if (out_nsamp > 0 && in_nsamp > 0) {
        naddt = in_nsamp / out_nsamp;
        if (naddt % 2 != 0) {
            --naddt;
        }
    }
    if (naddt <= 1) {
        naddt = 1;
    }
    if (naddc < 1) {
        naddc = in_nchans;
    }

    const int nc = in_nchans / naddc;
    if ((nc * naddc) != in_nchans) {
        throw std::runtime_error(
            "nchans must be integer multiple of decimation factor");
    }
    if (out_nbits == 0) {
        out_nbits = in_nbits;
    }

    gulp =
        static_cast<int>(std::ceil(static_cast<double>(gulp) / naddt) * naddt);

    std::map<std::string, sigproc::HeaderValue> out_hdr_map = {
        {"tsamp", reader.hdr.get<double>("tsamp") * naddt},
        {"foff", reader.hdr.get<double>("foff") * naddc},
        {"nchans", in_nchans / naddc},
        {"nbits", out_nbits},
        {"nsamples", in_nsamp > 0 ? in_nsamp / naddt : 0}};
    if (in_nsamp <= 0) {
        out_hdr_map.erase("nsamples");
    }
    reader.hdr.update(out_hdr_map);

    const int in_stride  = in_nchans * in_nifs;
    const int out_stride = (in_nchans / naddc) * in_nifs; // nifs unchanged

    auto write_payload = [&](sigproc::FilterbankWriter* writer) {
        std::vector<float> out_arr(
            static_cast<std::size_t>(gulp * out_stride / naddt));
        std::vector<float> block;
        auto plans = reader.get_readplan(gulp);
        reader.seek_sample(0);
        for (const auto& plan : plans) {
            while (true) {
                const auto nread =
                    reader.read_plan(plan.nvalues, block, plan.skip_values);
                if (nread == 0) {
                    break;
                }
                const int nsamps = static_cast<int>(nread / in_stride);
                const int usable = (nsamps / naddt) * naddt;
                if (usable <= 0) {
                    break;
                }
                sigproc::kernels::downsample(block, out_arr, naddt, naddc,
                                             in_nchans, usable);
                writer->write_block(out_arr, (usable / naddt) * out_stride);
                if (!plan.until_eof || nread < plan.nvalues) {
                    break;
                }
            }
        }
    };

    if (headerless) {
        // Still need a writer that skips the header: write to a sink after
        // encoding would require a separate path. Re-open via FilterbankWriter
        // is not headerless. Write samples through a throwaway header then...
        // Use FilterbankWriter but we already wrote the header if we construct
        // it. For headerless, stream samples only via bits::from_float.
        sigproc::io::SigprocHeader& hdr = reader.hdr;
        auto* out                       = &std::cout;
        std::ofstream owned;
        if (!sigproc::cli::is_stdio_path(outfile)) {
            owned.open(outfile, std::ios::binary);
            out = &owned;
        }
        sigproc::bits::BitsInfo info(
            static_cast<sigproc::SizeType>(hdr.get<int>("nbits")));
        std::vector<float> out_arr(
            static_cast<std::size_t>(gulp * out_stride / naddt));
        std::vector<float> block;
        auto plans = reader.get_readplan(gulp);
        reader.seek_sample(0);
        for (const auto& plan : plans) {
            while (true) {
                const auto nread =
                    reader.read_plan(plan.nvalues, block, plan.skip_values);
                if (nread == 0) {
                    break;
                }
                const int nsamps = static_cast<int>(nread / in_stride);
                const int usable = (nsamps / naddt) * naddt;
                if (usable <= 0) {
                    break;
                }
                sigproc::kernels::downsample(block, out_arr, naddt, naddc,
                                             in_nchans, usable);
                const int nwrite = (usable / naddt) * out_stride;
                std::vector<std::byte> packed(sigproc::bits::packed_nbytes(
                    static_cast<sigproc::SizeType>(nwrite), info));
                sigproc::bits::from_float(
                    std::span<const float>(out_arr.data(),
                                           static_cast<std::size_t>(nwrite)),
                    packed, info);
                out->write(reinterpret_cast<const char*>(packed.data()),
                           static_cast<std::streamsize>(packed.size()));
                if (!plan.until_eof || nread < plan.nvalues) {
                    break;
                }
            }
        }
        return 0;
    }

    sigproc::FilterbankWriter writer(outfile, reader.hdr);
    write_payload(&writer);
    return 0;
}
