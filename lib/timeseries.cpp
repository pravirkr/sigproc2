#include <cstddef>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <sigproc/timeseries.hpp>

namespace sigproc {

namespace {

[[nodiscard]] bool is_stdio_name(std::string_view name) {
    return name.empty() || name == "-";
}

template <HeaderValueType T>
void copy_present(io::SigprocHeader& out,
                  const io::SigprocHeader& in,
                  std::string_view key) {
    if (!in.is_present(key)) {
        return;
    }
    if (auto value = in.try_get<T>(key)) {
        out.set(key, *value);
    }
}

} // namespace

io::SigprocHeader make_tim_header(const io::SigprocHeader& in, int nsamples) {
    io::SigprocHeader out;
    copy_present<std::string>(out, in, "source_name");
    copy_present<std::string>(out, in, "rawdatafile");
    copy_present<int>(out, in, "machine_id");
    copy_present<int>(out, in, "telescope_id");
    copy_present<bool>(out, in, "barycentric");
    copy_present<bool>(out, in, "pulsarcentric");
    copy_present<double>(out, in, "az_start");
    copy_present<double>(out, in, "za_start");
    copy_present<double>(out, in, "src_raj");
    copy_present<double>(out, in, "src_dej");
    copy_present<double>(out, in, "tstart");
    copy_present<double>(out, in, "tsamp");
    copy_present<double>(out, in, "foff");
    copy_present<int>(out, in, "nifs");
    copy_present<int>(out, in, "nbeams");
    copy_present<int>(out, in, "ibeam");

    out.set("data_type", 2);
    out.set("nchans", 1);
    out.set("nbits", 32);
    if (!out.is_present("nifs")) {
        out.set("nifs", in.try_get<int>("nifs").value_or(1));
    }
    if (!out.is_present("tstart")) {
        out.set("tstart", in.try_get<double>("tstart").value_or(0.0));
    }
    if (!out.is_present("tsamp")) {
        out.set("tsamp", in.try_get<double>("tsamp").value_or(0.0));
    }
    if (!out.is_present("machine_id")) {
        out.set("machine_id", in.try_get<int>("machine_id").value_or(0));
    }
    if (!out.is_present("telescope_id")) {
        out.set("telescope_id", in.try_get<int>("telescope_id").value_or(0));
    }

    double fch1 = 0.0;
    if (in.has_freq_table()) {
        const auto table = in.get_freq_table();
        if (!table.empty()) {
            fch1 = table.front();
        }
    } else if (auto value = in.try_get<double>("fch1")) {
        fch1 = *value;
    }
    out.set("fch1", fch1);
    out.set("refdm", in.try_get<double>("refdm").value_or(0.0));

    if (nsamples > 0) {
        out.set("nsamples", nsamples);
    } else {
        copy_present<int>(out, in, "nsamples");
    }

    out.update({});
    return out;
}

TimeSeries::TimeSeries(io::SigprocHeader hdr, std::vector<float> samples)
    : m_samples(std::move(samples)) {
    const int nsamps =
        m_samples.empty() ? 0 : static_cast<int>(m_samples.size());
    m_hdr = make_tim_header(hdr, nsamps);
}

io::SigprocHeader const& TimeSeries::hdr() const noexcept { return m_hdr; }

std::span<float> TimeSeries::samples() noexcept { return m_samples; }

std::span<const float> TimeSeries::samples() const noexcept {
    return m_samples;
}

void TimeSeries::write_header(std::ostream& out) const {
    auto hdr = m_hdr;
    hdr.tostream(out);
}

void TimeSeries::write_samples(std::ostream& out,
                               std::span<const float> samples) {
    if (samples.empty()) {
        return;
    }
    out.write(reinterpret_cast<const char*>(samples.data()),
              static_cast<std::streamsize>(samples.size() * sizeof(float)));
    if (!out.good()) {
        throw std::runtime_error("Failed to write time-series samples");
    }
}

void TimeSeries::tostream(std::ostream& out) const {
    write_header(out);
    write_samples(out, m_samples);
}

void TimeSeries::tofile(std::string_view path) const {
    if (is_stdio_name(path)) {
        tostream(std::cout);
        return;
    }
    std::ofstream out(std::string(path), std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot open time-series file: " +
                                 std::string(path));
    }
    tostream(out);
}

} // namespace sigproc
