/*
    BANDPASS - outputs the band pass from a filterbank file
*/

#include <cmath>
#include <format>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include <CLI/CLI.hpp>

#include <sigproc/filterbank.hpp>
#include <sigproc/kernels.hpp>

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

    sigproc::FilterbankReader filreader(filename);

    /* set number of dumps to average over if user has supplied seconds */
    int nstart = static_cast<int>(
        std::rint(tstart / filreader.hdr.get<double>("tsamp")));
    int nsamp = static_cast<int>(
        std::rint(total_time / filreader.hdr.get<double>("tsamp")));
    int nchans = filreader.hdr.get<int>("nchans");

    /* initialize buffer for storing bandpass */
    std::vector<double> chan_freqs(static_cast<std::size_t>(nchans), 0);
    std::vector<double> bandpass(static_cast<std::size_t>(nchans), 0);

    for (int ichan = 0; ichan < nchans; ++ichan) {
        chan_freqs[static_cast<std::size_t>(ichan)] =
            ichan * filreader.hdr.get<double>("foff") +
            filreader.hdr.get<double>("fch1");
    }

    std::vector<float> block;
    std::vector<sigproc::ReadPlanTuple> plan_blocks =
        filreader.get_readplan(gulp, 0, nstart, nsamp);
    filreader.seek_sample(nstart); // start sample = nstart

    int num_samples = 0;
    for (const auto& tup : plan_blocks) {
        int block_len = std::get<1>(tup);
        int skip      = std::get<2>(tup);
        int nsamps    = block_len / nchans;
        filreader.read_plan(block_len, block, skip);
        sigproc::kernels::get_bpass(block, bandpass, nchans, nsamps);
        num_samples += nsamps;
    }

    for (auto& elem : bandpass) {
        elem = elem / num_samples;
    }

    std::ofstream outstream(outfile.c_str());
    for (int ichan = 0; ichan < nchans; ++ichan) {
        outstream << std::format("{:.4f}\t{:.4f}\n",
                                 chan_freqs[static_cast<std::size_t>(ichan)],
                                 bandpass[static_cast<std::size_t>(ichan)]);
    }

    return 0;
}
