
#include <vector>
#include <tuple>
#include <cmath>

#include <fmt/ostream.h>
#include <CLI/CLI.hpp>

#include <sigproc/io.hpp>
#include "kernels.hpp"

int main(int argc, char** argv) {
    CLI::App app{"bandpass - outputs the band pass from a filterbank file"};

    std::string filename;
    app.add_option("filename", filename, "the filterbank data file")
        ->required()
        ->check(CLI::ExistingFile);

    std::string outfile;
    app.add_option("-o,--outfile", outfile, "output txt file");

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

    /* set number of dumps to average over if user has supplied seconds */
    int nstart = (int)std::rint(tstart / filreader.hdr.get<double>("tsamp"));
    int nsamp  = (int)std::rint(total_time / filreader.hdr.get<double>("tsamp"));
    int nchans = filreader.hdr.get<int>("nchans");

    /* initialize buffer for storing bandpass */
    std::vector<double> chanFreqs(nchans, 0);
    std::vector<double> bandpass(nchans, 0);

    for (int ichan = 0; ichan < nchans; ++ichan) {
        chanFreqs[ichan] = ichan * filreader.hdr.get<double>("foff")
                           + filreader.hdr.get<double>("fch1");
    }

    std::vector<float> block;
    std::vector<readplan_tuple> plan_blocks
        = filreader.get_readplan(gulp, 0, nstart, nsamp);
    filreader.seek_sample(nstart);  // start sample = nstart

    int block_len, skip, nsamps;
    int num_samples = 0;
    for (const auto& tup : plan_blocks) {
        block_len = std::get<1>(tup);
        skip      = std::get<2>(tup);
        nsamps    = (int)(block_len / nchans);
        filreader.read_plan(block_len, block, skip);
        sigproc::getBpass(block.data(), bandpass.data(), nchans, nsamps);
        num_samples += nsamps;
    };

    for (auto& elem : bandpass) {
        elem = elem / num_samples;
    }

    std::ofstream outstream(outfile.c_str());
    for (int ichan = 0; ichan < nchans; ++ichan) {
        fmt::print(outstream, "{:.4f}\t{:.4f}\n", chanFreqs[ichan],
                   bandpass[ichan]);
    }

    return 0;
}
