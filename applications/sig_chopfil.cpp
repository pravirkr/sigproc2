/*
 * chop_fil - chop a filterbank file up in time
 */

#include <cmath>
#include <string>
#include <tuple>
#include <vector>

#include <CLI/CLI.hpp>

#include <sigproc/filterbank.hpp>

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

    sigproc::FilterbankReader filreader(filename);

    int nstart = static_cast<int>(
        std::rint(tstart / filreader.hdr.get<double>("tsamp")));
    int nsamp = static_cast<int>(
        std::rint(total_time / filreader.hdr.get<double>("tsamp")));

    sigproc::FilterbankWriter filwriter(outfile, filreader.hdr);

    int stride_len =
        filreader.hdr.get<int>("nchans") * filreader.hdr.get<int>("nifs");

    std::vector<float> out_arr(static_cast<std::size_t>(gulp * stride_len), 0);
    std::vector<float> block;

    std::vector<sigproc::ReadPlanTuple> plan_blocks =
        filreader.get_readplan(gulp, 0, nstart, nsamp);
    filreader.seek_sample(nstart); // start sample = nstart

    for (const auto& tup : plan_blocks) {
        int block_len = std::get<1>(tup);
        int skip      = std::get<2>(tup);
        int nsamps    = block_len / filreader.hdr.get<int>("nchans");
        filreader.read_plan(block_len, block, skip);
        filwriter.write_block(out_arr, nsamps * stride_len);
    }

    return 0;
}
