
#include <sigproc/kernels.hpp>

namespace sigproc {

void add_channels(std::span<const float> inbuffer, std::span<float> outbuffer,
                  int chan_start, int nchans, int nsamps, int index) {
#pragma omp parallel for default(none)                                         \
    shared(inbuffer, outbuffer, chan_start, nchans, nsamps, index)
    for (int ii = 0; ii < nsamps; ii++) {
        for (int jj = chan_start; jj < chan_start + nchans; jj++) {
            outbuffer[index + ii] += inbuffer[(nchans * ii) + jj];
        }
    }
}

template <class T>
void add_samples(std::span<const float> inbuffer, std::span<double> outbuffer,
                 int nchans, int nsamps, int nifs) {
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

void get_bpass(std::span<const float> inbuffer, std::span<double> outbuffer,
               int nchans, int nsamps) {
#pragma omp parallel for default(none)                                         \
    shared(inbuffer, outbuffer, nchans, nsamps)
    for (int jj = 0; jj < nchans; jj++) {
        for (int ii = 0; ii < nsamps; ii++) {
            outbuffer[jj] += inbuffer[(nchans * ii) + jj];
        }
    }
}

void downsample(std::span<const float> inbuffer, std::span<float> outbuffer,
                int tfactor, int ffactor, int nchans, int nsamps) {
    int newnsamps = nsamps / tfactor;
    int newnchans = nchans / ffactor;
    int totfactor = ffactor * tfactor;
#pragma omp parallel for default(none)                                         \
    shared(inbuffer, outbuffer, nchans, nsamps, newnsamps, newnchans, tfactor, \
               ffactor, totfactor)
    for (int ii = 0; ii < newnsamps; ii++) {
        for (int jj = 0; jj < newnchans; jj++) {
            float temp = 0;
            int pos    = nchans * ii * tfactor + jj * ffactor;
            for (int kk = 0; kk < tfactor; kk++) {
                for (int ll = 0; ll < ffactor; ll++) {
                    temp += inbuffer[kk * nchans + ll + pos];
                }
            }
            outbuffer[ii * newnchans + jj] =
                temp / static_cast<float>(totfactor);
        }
    }
}

} // namespace sigproc

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
