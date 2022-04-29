/*
    DECIMATE  - decimate filterbank data by adding channels and/or time samples
*/

#include <vector>
#include <tuple>
#include <cmath>

#include <CLI/CLI.hpp>

#include <sigproc/io.hpp>
#include "kernels.hpp"

int main(int argc, char** argv) {
    CLI::App app{"decimate - reduce time and/or frequency resolution of "
                 "filterbank data"};

    std::string filename;
    app.add_option("filename", filename, "the filterbank data file")
        ->required()
        ->check(CLI::ExistingFile);

    std::string outfile;
    app.add_option("-o,--outfile", outfile, "output flterbank file name");
    int ffactor = 0;
    app.add_option("-c,--numchans", ffactor,
                   "number of channels to add (def=all)");
    int tfactor = 0;
    app.add_option("-t,--numsamps", tfactor,
                   "number of time samples to add (def=none)");
    int gulp = 512;
    app.add_option("-g,--gulp", gulp,
                   "number of time samples to read at a given time(def=512)");
    int out_nbits = 0;
    app.add_option("-n,--nbits", out_nbits,
                   "specify output number of bits (def=input)");

    CLI11_PARSE(app, argc, argv);

    FilterbankReader filreader(filename);

    // gulp must be a multiple of tfactor
    gulp = (int)(std::ceil(gulp / tfactor) * tfactor);

    // Output nbits
    if (out_nbits == 0) {
        out_nbits = filreader.hdr.get<int>("nbits");
    }

    int nc = (int)filreader.hdr.get<int>("nchans") / ffactor;
    if ((nc * ffactor) != filreader.hdr.get<int>("nchans")) {
        std::runtime_error(
            "nchans must be integer multiple of decimation factor");
    }

    std::map<std::string, sig_hdr_types> out_hdr_map
        = {{"tsamp", filreader.hdr.get<double>("tsamp") * tfactor},
           {"foff", filreader.hdr.get<double>("foff") * ffactor},
           {"nchans", filreader.hdr.get<int>("nchans") / ffactor},
           {"nbits", out_nbits}};
    filreader.hdr.update(out_hdr_map);

    FilterbankWriter filwriter(outfile, filreader.hdr);

    int stride_len
        = filreader.hdr.get<int>("nchans") * filreader.hdr.get<int>("nifs");

    std::vector<float> out_arr(gulp * stride_len / ffactor / tfactor, 0);
    std::vector<float> block;

    std::vector<readplan_tuple> plan_blocks = filreader.get_readplan(gulp);
    filreader.seek_sample(0);  // start sample = 0

    int block_len, skip, nsamps;
    for (const auto& tup : plan_blocks) {
        block_len = std::get<1>(tup);
        skip      = std::get<2>(tup);
        nsamps    = (int)(block_len / filreader.hdr.get<int>("nchans"));
        filreader.read_plan(block_len, block, skip);
        sigproc::downsample(block.data(), out_arr.data(), tfactor, ffactor,
                            sigfile.hdr.get<int>("nchans"), nsamps);
        filwriter.write_block(out_arr, nsamps * stride_len / ffactor / tfactor);
    };

    return 0;
}
