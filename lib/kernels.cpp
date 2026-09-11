#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <sigproc/kernels.hpp>

namespace sigproc::kernels {

void add_channels(std::span<const float> inbuffer,
                  std::span<float> outbuffer,
                  int chan_start,
                  int nchans,
                  int nsamps,
                  int index) {
#pragma omp parallel for default(none)                                         \
    shared(inbuffer, outbuffer, chan_start, nchans, nsamps, index)
    for (int ii = 0; ii < nsamps; ii++) {
        for (int jj = chan_start; jj < chan_start + nchans; jj++) {
            outbuffer[index + ii] += inbuffer[(nchans * ii) + jj];
        }
    }
}

void add_samples(std::span<const float> inbuffer,
                 std::span<double> outbuffer,
                 int nchans,
                 int nsamps,
                 int nifs) {
#pragma omp parallel for default(none)                                         \
    shared(inbuffer, outbuffer, nchans, nsamps, nifs)
    for (int ipol = 0; ipol < nifs; ipol++) {
        for (int jj = 0; jj < nchans; jj++) {
            for (int ii = 0; ii < nsamps; ii++) {
                outbuffer[(nchans * ipol) + jj] +=
                    inbuffer[(nifs * nchans * ii) + (nchans * ipol) + jj];
            }
        }
    }
}

void get_bpass(std::span<const float> inbuffer,
               std::span<double> outbuffer,
               int nchans,
               int nsamps) {
#pragma omp parallel for default(none)                                         \
    shared(inbuffer, outbuffer, nchans, nsamps)
    for (int jj = 0; jj < nchans; jj++) {
        for (int ii = 0; ii < nsamps; ii++) {
            outbuffer[jj] += inbuffer[(nchans * ii) + jj];
        }
    }
}

void downsample(std::span<const float> inbuffer,
                std::span<float> outbuffer,
                int tfactor,
                int ffactor,
                int nchans,
                int nsamps) {
    int newnsamps = nsamps / tfactor;
    int newnchans = nchans / ffactor;
    int totfactor = ffactor * tfactor;
#pragma omp parallel for default(none)                                         \
    shared(inbuffer, outbuffer, nchans, nsamps, newnsamps, newnchans, tfactor, \
               ffactor, totfactor)
    for (int ii = 0; ii < newnsamps; ii++) {
        for (int jj = 0; jj < newnchans; jj++) {
            float temp = 0;
            int pos    = (nchans * ii * tfactor) + (jj * ffactor);
            for (int kk = 0; kk < tfactor; kk++) {
                for (int ll = 0; ll < ffactor; ll++) {
                    temp += inbuffer[(kk * nchans) + ll + pos];
                }
            }
            outbuffer[(ii * newnchans) + jj] =
                temp / static_cast<float>(totfactor);
        }
    }
}

void concat_channels(std::span<const float* const> inputs,
                     std::span<const int> nchans,
                     std::span<float> out,
                     int nifs,
                     int nsamps) {
    if (nifs <= 0) {
        throw std::invalid_argument("concat_channels: nifs must be > 0");
    }
    if (nsamps < 0) {
        throw std::invalid_argument("concat_channels: nsamps must be >= 0");
    }
    if (inputs.size() != nchans.size() || nchans.empty()) {
        throw std::invalid_argument(
            "concat_channels: need a matching nonempty nchans list");
    }
    const int nfiles = static_cast<int>(nchans.size());
    int out_nchans   = 0;
    for (int i = 0; i < nfiles; ++i) {
        if (inputs[static_cast<std::size_t>(i)] == nullptr) {
            throw std::invalid_argument("concat_channels: null input pointer");
        }
        if (nchans[static_cast<std::size_t>(i)] <= 0) {
            throw std::invalid_argument("concat_channels: nchans must be > 0");
        }
        out_nchans += nchans[static_cast<std::size_t>(i)];
    }
    const auto need = static_cast<std::size_t>(nsamps) *
                      static_cast<std::size_t>(nifs) *
                      static_cast<std::size_t>(out_nchans);
    if (out.size() < need) {
        throw std::invalid_argument("concat_channels: output span too small");
    }
    if (nsamps == 0) {
        return;
    }

    const float* const* in_ptr  = inputs.data();
    float* __restrict__ out_ptr = out.data();
    const int* nchan_ptr        = nchans.data();

#pragma omp parallel for default(none)                                         \
    shared(in_ptr, nchan_ptr, out_ptr, nfiles, nifs, nsamps, out_nchans)
    for (int t = 0; t < nsamps; ++t) {
        for (int ipol = 0; ipol < nifs; ++ipol) {
            float* dst = out_ptr + (static_cast<std::size_t>(t) *
                                        static_cast<std::size_t>(nifs) +
                                    static_cast<std::size_t>(ipol)) *
                                       static_cast<std::size_t>(out_nchans);
            for (int f = 0; f < nfiles; ++f) {
                const int nc = nchan_ptr[f];
                const float* __restrict__ src =
                    in_ptr[f] + (static_cast<std::size_t>(t) *
                                     static_cast<std::size_t>(nifs) +
                                 static_cast<std::size_t>(ipol)) *
                                    static_cast<std::size_t>(nc);
                for (int c = 0; c < nc; ++c) {
                    dst[c] = src[c];
                }
                dst += nc;
            }
        }
    }
}

void dice_channels(std::span<const float> in,
                   std::span<float> out,
                   std::span<const int> keep,
                   int nchans,
                   int nifs,
                   int nsamps,
                   bool collapse) {
    if (nchans <= 0 || nifs <= 0) {
        throw std::invalid_argument(
            "dice_channels: nchans and nifs must be > 0");
    }
    if (nsamps < 0) {
        throw std::invalid_argument("dice_channels: nsamps must be >= 0");
    }
    if (static_cast<int>(keep.size()) != nchans) {
        throw std::invalid_argument(
            "dice_channels: keep mask width must equal nchans");
    }
    int nkeep = 0;
    for (int c = 0; c < nchans; ++c) {
        if (keep[static_cast<std::size_t>(c)] != 0) {
            ++nkeep;
        }
    }
    if (collapse && nkeep == 0) {
        throw std::invalid_argument(
            "dice_channels: collapse with no kept channels");
    }
    const int out_nchans = collapse ? nkeep : nchans;
    const auto in_need   = static_cast<std::size_t>(nsamps) *
                         static_cast<std::size_t>(nifs) *
                         static_cast<std::size_t>(nchans);
    const auto out_need = static_cast<std::size_t>(nsamps) *
                          static_cast<std::size_t>(nifs) *
                          static_cast<std::size_t>(out_nchans);
    if (in.size() < in_need || out.size() < out_need) {
        throw std::invalid_argument("dice_channels: span too small");
    }
    if (nsamps == 0) {
        return;
    }

    const float* __restrict__ in_ptr = in.data();
    float* __restrict__ out_ptr      = out.data();
    const int* keep_ptr              = keep.data();
    const bool drop_zapped           = collapse;

#pragma omp parallel for default(none)                                         \
    shared(in_ptr, out_ptr, keep_ptr, nchans, nifs, nsamps, out_nchans,        \
               drop_zapped)
    for (int t = 0; t < nsamps; ++t) {
        for (int ipol = 0; ipol < nifs; ++ipol) {
            const auto base =
                (static_cast<std::size_t>(t) * static_cast<std::size_t>(nifs) +
                 static_cast<std::size_t>(ipol));
            const float* src = in_ptr + base * static_cast<std::size_t>(nchans);
            float* dst = out_ptr + base * static_cast<std::size_t>(out_nchans);
            if (drop_zapped) {
                int k = 0;
                for (int c = 0; c < nchans; ++c) {
                    if (keep_ptr[c] != 0) {
                        dst[k++] = src[c];
                    }
                }
            } else {
                for (int c = 0; c < nchans; ++c) {
                    dst[c] = keep_ptr[c] != 0 ? src[c] : 0.0F;
                }
            }
        }
    }
}

float gulp_median(std::span<const float> in) {
    if (in.empty()) {
        return 0.0F;
    }
    std::vector<float> copy(in.begin(), in.end());
    const auto n   = copy.size();
    const auto mid = n / 2;
    std::nth_element(copy.begin(),
                     copy.begin() + static_cast<std::ptrdiff_t>(mid),
                     copy.end());
    if (n % 2 == 1) {
        return copy[mid];
    }
    const float upper = copy[mid];
    const float lower = *std::max_element(
        copy.begin(), copy.begin() + static_cast<std::ptrdiff_t>(mid));
    return 0.5F * (lower + upper);
}

void flatten_gulp(std::span<const float> in,
                  std::span<float> out,
                  float median,
                  float scale) {
    if (out.size() < in.size()) {
        throw std::invalid_argument("flatten_gulp: output span too small");
    }
    const int n         = static_cast<int>(in.size());
    const float* in_ptr = in.data();
    float* out_ptr      = out.data();
    if (scale == 0.0F) {
#pragma omp parallel for default(none) shared(out_ptr, n)
        for (int i = 0; i < n; ++i) {
            out_ptr[i] = 0.0F;
        }
        return;
    }
#pragma omp parallel for default(none) shared(in_ptr, out_ptr, n, median, scale)
    for (int i = 0; i < n; ++i) {
        out_ptr[i] = (in_ptr[i] - median) / scale;
    }
}

void clip_gulp(std::span<const float> in, std::span<float> out) {
    if (out.size() < in.size()) {
        throw std::invalid_argument("clip_gulp: output span too small");
    }
    if (in.empty()) {
        return;
    }
    const float median  = gulp_median(in);
    const int n         = static_cast<int>(in.size());
    const float* in_ptr = in.data();
    double sum          = 0.0;
    double ssq          = 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(in_ptr[i]);
        sum += x;
        ssq += x * x;
    }
    const double inv  = 1.0 / static_cast<double>(n);
    const double mean = sum * inv;
    const double mnsq = ssq * inv;
    const double var  = mnsq - mean * mean;
    const float sigma = var > 0.0 ? static_cast<float>(std::sqrt(var)) : 0.0F;
    float* out_ptr    = out.data();
#pragma omp parallel for default(none) shared(in_ptr, out_ptr, n, median, sigma)
    for (int i = 0; i < n; ++i) {
        const float x = in_ptr[i];
        out_ptr[i]    = std::fabs(x - median) > sigma ? median : x;
    }
}

double pulse_phase(std::int64_t index, double tsamp, double period) {
    if (period <= 0.0) {
        throw std::invalid_argument("pulse_phase: period must be > 0");
    }
    const double turn = static_cast<double>(index + 1) * tsamp / period;
    return turn - std::floor(turn);
}

} // namespace sigproc::kernels

/*
template <typename T>
T* addBoundary(T* inbuffer, int window, int nsamps) {
    // Allocate memory for new extended array (to deal with window edges)
    const int boundarySize = window / 2;  // integer division
    const int outSize      = nsamps + boundarySize * 2;

    T* arrayWithBoundary = new T[outSize];
    std::memcpy(arrayWithBoundary + boundarySize, inbuffer, nsamps * sizeof(T));
    // Extend by reflecting about the edge.
    for (int ii = 0; ii < boundarySize; ++ii) {
        arrayWithBoundary[ii] = inbuffer[boundarySize - 1 - ii];
        arrayWithBoundary[nsamps + boundarySize + ii]
            = inbuffer[nsamps - 1 - ii];
    }
    return arrayWithBoundary;
}

template <class T>
void getmeanrms(T* inbuffer, float* outbuffer, int window, int nsamps) {
    T* arrayWithBoundary = addBoundary<T>(inbuffer, window, nsamps);
    int outSize          = nsamps + (window / 2) * 2;

    // hack for even window size
    outSize = (window % 2) ? outSize : outSize - 1;

    // Move window through all elements of the extended array
    double sum = 0;
    for (int ii = 0; ii < outSize; ++ii) {
        sum += arrayWithBoundary[ii];
        if (ii >= window) {
            sum -= arrayWithBoundary[ii - window];
        }
        if (ii >= (window - 1)) {
            outbuffer[ii - window + 1] = (float)sum / window;
        }
    }
    // Free memory
    delete[] arrayWithBoundary;
}

int* ignored_channels(char* filename, int nchans) {
    int i, idx, *ignore;
    FILE* ignfile;

    // allocate space for ignore array and initialize
    ignore = (int*)malloc(nchans * sizeof(int));
    for (i = 0; i < nchans; i++)
        ignore[i] = 0;

    // read list of ignored channel numbers from file
    ignfile = open_file(filename, "r");
    while (1) {
        fscanf(ignfile, "%d", &idx);
        if (feof(ignfile))
            break;
        idx--;
        if ((idx >= 0) && (idx < nchans))
            ignore[idx] = 1;
    }
    close(ignfile);

    return (ignore);
}
*/

/*
   return a pointer to an array of filterbank channel frequencies given the
   center frequency fmid and the offset between each of the nchan channels
   IHS incorporated additional offset for WAPP data (wapp_off)
*/
/*
double* chan_freqs(double fmid, double foff, int nchans, int wapp_off) {
    int i;
    double* chanfreq;
    chanfreq = (double*)malloc(nchans * sizeof(double));
    for (i = 0; i < nchans; i++) {
        chanfreq[i]
            = fmid + (nchans / 2 - i) * foff - 0.5 * ((double)wapp_off) * foff;
    }
    return (chanfreq);
}
*/
