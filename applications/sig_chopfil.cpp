/*
 * chop_fil.c chop a fil file up!
 */

#include <vector>
#include <tuple>
#include <cmath>

#include <CLI/CLI.hpp>
#include <sigproc/io.hpp>

int main(int argc, char** argv) {
    CLI::App app{"chop_fil: splits a fil file in time"};

    std::string filename;
    app.add_option("filename", filename, "the filterbank data file")
        ->required()
        ->check(CLI::ExistingFile);

    std::string outfile;
    app.add_option("-o,--outfile", outfile, "output flterbank file name");

    double tstart = 0.0;
    app.add_option("-s,--start", tstart, "Start processing at (def=0)");

    double total_time = 0.0;
    app.add_option("-t,--total", total_time,
                   "Total obs time to be processed (def=all)");

    int gulp = 512;
    app.add_option("-g,--gulp", gulp,
                   "number of time samples to read at a given time(def=512)");
    CLI11_PARSE(app, argc, argv);

    FilterbankReader filreader(filename);

    int nstart = (int)std::rint(tstart / filreader.hdr.get<double>("tsamp"));
    int nsamp = (int)std::rint(total_time / filreader.hdr.get<double>("tsamp"));

    FilterbankWriter filwriter(outfile, filreader.hdr);

    int stride_len
        = filreader.hdr.get<int>("nchans") * filreader.hdr.get<int>("nifs");

    std::vector<float> out_arr(gulp * stride_len, 0);
    std::vector<float> block;

    std::vector<readplan_tuple> plan_blocks
        = filreader.get_readplan(gulp, 0, nstart, nsamp);
    filreader.seek_sample(nstart);  // start sample = nstart

    int block_len, skip, nsamps;
    for (const auto& tup : plan_blocks) {
        block_len = std::get<1>(tup);
        skip      = std::get<2>(tup);
        nsamps    = (int)(block_len / filreader.hdr.get<int>("nchans"));
        filreader.read_plan(block_len, block, skip);
        filwriter.write_block(out_arr, nsamps * stride_len);
    };

    return 0;
}
