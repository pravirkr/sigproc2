#pragma once

#include <span>

namespace sigproc::kernels {

void add_channels(std::span<const float> inbuffer, std::span<float> outbuffer,
                  int chan_start, int nchans, int nsamps, int index);

void add_samples(std::span<const float> inbuffer, std::span<double> outbuffer,
                 int nchans, int nsamps, int nifs);

void get_bpass(std::span<const float> inbuffer, std::span<double> outbuffer,
               int nchans, int nsamps);
void downsample(std::span<const float> inbuffer, std::span<float> outbuffer,
                int tfactor, int ffactor, int nchans, int nsamps);

} // namespace sigproc::kernels
