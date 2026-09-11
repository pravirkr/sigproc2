#include "sigproc/io_hdf5.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <highfive/highfive.hpp>
#include <spdlog/spdlog.h>

#include <sigproc/bits.hpp>
#include <sigproc/common/params.hpp>
#include <sigproc/header.hpp>

#include "sigproc/header_codec.hpp"

namespace sigproc::io {

namespace {

using HighFive::Attribute;
using HighFive::DataSet;
using HighFive::DataSetCreateProps;
using HighFive::DataSpace;
using HighFive::DataTypeClass;
using HighFive::File;

[[nodiscard]] std::string ascii_lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

[[nodiscard]] bool ends_with_ci(std::string_view path, std::string_view suf) {
    if (path.size() < suf.size()) {
        return false;
    }
    const auto tail = path.substr(path.size() - suf.size());
    for (std::size_t i = 0; i < suf.size(); ++i) {
        const auto a = static_cast<char>(
            std::tolower(static_cast<unsigned char>(tail[i])));
        const auto b = suf[i];
        if (a != b) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool magic_matches(const char* buf, std::size_t n) {
    if (n < kHdf5Magic.size()) {
        return false;
    }
    return std::memcmp(buf, kHdf5Magic.data(), kHdf5Magic.size()) == 0;
}

[[nodiscard]] std::string read_string_attr(const Attribute& attr) {
    const auto dtype = attr.getDataType();
    if (dtype.getClass() == DataTypeClass::String || dtype.isVariableStr() ||
        dtype.isFixedLenStr()) {
        std::string value;
        attr.read(value);
        while (!value.empty() && value.back() == '\0') {
            value.pop_back();
        }
        return value;
    }
    throw std::runtime_error(
        std::format("HDF5 attribute '{}' is not a string", attr.getName()));
}

[[nodiscard]] std::string optional_file_class(const File& file) {
    if (!file.hasAttribute("CLASS")) {
        return {};
    }
    return read_string_attr(file.getAttribute("CLASS"));
}

void require_filterbank_class(const File& file) {
    const auto cls = optional_file_class(file);
    if (cls.empty()) {
        throw std::runtime_error("HDF5 file is missing CLASS=FILTERBANK");
    }
    if (ascii_lower(cls) != "filterbank") {
        throw std::runtime_error(
            std::format("HDF5 CLASS='{}' is not FILTERBANK", cls));
    }
}

[[nodiscard]] SizeType packed_axis_len(int nchans, int nbits) {
    if (nchans <= 0) {
        throw std::invalid_argument("nchans must be > 0");
    }
    if (nbits == 1 || nbits == 2 || nbits == 4) {
        const auto bitfact = 8 / nbits;
        if (nchans % bitfact != 0) {
            throw std::invalid_argument(std::format(
                "nchans {} is not divisible by {} for {}-bit packing", nchans,
                bitfact, nbits));
        }
        return static_cast<SizeType>(nchans / bitfact);
    }
    return static_cast<SizeType>(nchans);
}

[[nodiscard]] SizeType item_bytes(int nbits) {
    if (nbits == 16) {
        return 2;
    }
    if (nbits == 32) {
        return 4;
    }
    return 1;
}

void packed_to_float(std::span<const std::byte> packed,
                     std::span<float> out,
                     const bits::BitsInfo& info) {
    const auto nbits = info.get_nbits();
    if (nbits == 1 || nbits == 2 || nbits == 4) {
        bits::unpack_to_float(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(packed.data()),
                packed.size()),
            out, nbits);
        return;
    }
    if (nbits == 8) {
        bits::u8_to_float(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(packed.data()),
                out.size()),
            out);
        return;
    }
    if (nbits == 16) {
        bits::u16_to_float(
            std::span<const std::uint16_t>(
                reinterpret_cast<const std::uint16_t*>(packed.data()),
                out.size()),
            out);
        return;
    }
    if (nbits == 32) {
        bits::f32_copy(
            std::span<const float>(
                reinterpret_cast<const float*>(packed.data()), out.size()),
            out);
        return;
    }
    throw std::invalid_argument(std::format("Unsupported nbits: {}", nbits));
}

struct AxisMap {
    int time = 0;
    int feed = 1;
    int freq = 2;
};

[[nodiscard]] AxisMap map_axes(const DataSet& data) {
    AxisMap map;
    if (!data.hasAttribute("DIMENSION_LABELS")) {
        return map;
    }
    std::vector<std::string> labels;
    try {
        data.getAttribute("DIMENSION_LABELS").read(labels);
    } catch (const std::exception& ex) {
        spdlog::warn("Ignoring unreadable DIMENSION_LABELS: {}", ex.what());
        return map;
    }
    if (labels.size() != 3) {
        spdlog::warn("DIMENSION_LABELS has {} entries; assuming time,feed,freq",
                     labels.size());
        return map;
    }
    auto assign = [&](int axis, std::string_view raw) {
        const auto label = ascii_lower(raw);
        if (label == "time") {
            map.time = axis;
        } else if (label == "feed_id" || label == "feed") {
            map.feed = axis;
        } else if (label == "frequency" || label == "freq") {
            map.freq = axis;
        } else {
            throw std::runtime_error(
                std::format("Unexpected DIMENSION_LABELS[{}]='{}'", axis, raw));
        }
    };
    for (int i = 0; i < 3; ++i) {
        assign(i, labels[static_cast<std::size_t>(i)]);
    }
    return map;
}

[[nodiscard]] bool try_read_i64(const Attribute& attr, std::int64_t& out) {
    const auto dtype = attr.getDataType();
    if (dtype.getClass() != DataTypeClass::Integer) {
        return false;
    }
    const auto n = dtype.getSize();
    try {
        if (n == 1) {
            std::int8_t v{};
            attr.read(v);
            out = v;
            return true;
        }
        if (n == 2) {
            std::int16_t v{};
            attr.read(v);
            out = v;
            return true;
        }
        if (n == 4) {
            std::int32_t v{};
            attr.read(v);
            out = v;
            return true;
        }
        if (n == 8) {
            attr.read(out);
            return true;
        }
    } catch (const std::exception&) {
        try {
            if (n == 1) {
                std::uint8_t v{};
                attr.read(v);
                out = v;
                return true;
            }
            if (n == 2) {
                std::uint16_t v{};
                attr.read(v);
                out = v;
                return true;
            }
            if (n == 4) {
                std::uint32_t v{};
                attr.read(v);
                out = static_cast<std::int64_t>(v);
                return true;
            }
            if (n == 8) {
                std::uint64_t v{};
                attr.read(v);
                out = static_cast<std::int64_t>(v);
                return true;
            }
        } catch (const std::exception&) {
            return false;
        }
    }
    return false;
}

[[nodiscard]] bool try_read_f64(const Attribute& attr, double& out) {
    const auto dtype = attr.getDataType();
    if (dtype.getClass() != DataTypeClass::Float) {
        return false;
    }
    try {
        if (dtype.getSize() == 4) {
            float v{};
            attr.read(v);
            out = static_cast<double>(v);
            return true;
        }
        attr.read(out);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void apply_sigproc_attr(SigprocHeader& hdr,
                        const std::string& key,
                        const Attribute& attr) {
    if (key == "DIMENSION_LABELS" || key == "CLASS" || key == "VERSION" ||
        key == "nfpc") {
        return;
    }
    if (key == "fchannel") {
        std::vector<double> freqs;
        attr.read(freqs);
        if (!freqs.empty()) {
            hdr.set_freq_table(std::move(freqs));
        }
        return;
    }
    const auto it = params::kSigprocKeys.find(key);
    if (it == params::kSigprocKeys.end()) {
        return;
    }
    switch (it->second.type) {
    case KeyType::kSString: {
        hdr.set(key, read_string_attr(attr));
        break;
    }
    case KeyType::kSDouble: {
        double v{};
        if (!try_read_f64(attr, v)) {
            std::int64_t iv{};
            if (!try_read_i64(attr, iv)) {
                spdlog::warn("Skipping HDF5 attr '{}' (not numeric)", key);
                return;
            }
            v = static_cast<double>(iv);
        }
        hdr.set(key, v);
        break;
    }
    case KeyType::kSInt: {
        std::int64_t v{};
        if (!try_read_i64(attr, v)) {
            double dv{};
            if (!try_read_f64(attr, dv)) {
                spdlog::warn("Skipping HDF5 attr '{}' (not integer)", key);
                return;
            }
            v = static_cast<std::int64_t>(dv);
        }
        hdr.set(key, static_cast<int>(v));
        break;
    }
    case KeyType::kSBool: {
        std::int64_t v{};
        if (!try_read_i64(attr, v)) {
            spdlog::warn("Skipping HDF5 attr '{}' (not integer flag)", key);
            return;
        }
        if (key == "signed") {
            hdr.set(key, v < 0);
        } else {
            hdr.set(key, v != 0);
        }
        break;
    }
    }
}

void load_header_from_dataset(SigprocHeader& hdr,
                              const std::string& filename,
                              const DataSet& data,
                              const AxisMap& axes) {
    for (const auto& name : data.listAttributeNames()) {
        apply_sigproc_attr(hdr, name, data.getAttribute(name));
    }
    const auto dims = data.getSpace().getDimensions();
    if (dims.size() != 3) {
        throw std::runtime_error(std::format(
            "FBH5 dataset 'data' must be rank 3, got {}", dims.size()));
    }
    const auto ntime = static_cast<int>(
        std::min(dims[static_cast<std::size_t>(axes.time)],
                 static_cast<std::size_t>(std::numeric_limits<int>::max())));
    const auto nfeed =
        static_cast<int>(dims[static_cast<std::size_t>(axes.feed)]);
    hdr.set("nsamples", ntime);
    if (!hdr.is_present("nifs")) {
        hdr.set("nifs", nfeed);
    }
    if (!hdr.is_present("nbits")) {
        const auto dtype = data.getDataType();
        const auto cls   = dtype.getClass();
        const auto sz    = dtype.getSize();
        if (cls == DataTypeClass::Float && sz == 4) {
            hdr.set("nbits", 32);
        } else if (cls == DataTypeClass::Integer && sz == 2) {
            hdr.set("nbits", 16);
        } else if (cls == DataTypeClass::Integer && sz == 1) {
            hdr.set("nbits", 8);
        } else {
            throw std::runtime_error("HDF5 dataset 'data' is missing nbits and "
                                     "has an unknown dtype");
        }
    }
    if (!hdr.is_present("nchans")) {
        const int nbits   = hdr.get<int>("nbits");
        const auto freq_n = dims[static_cast<std::size_t>(axes.freq)];
        if (nbits == 1 || nbits == 2 || nbits == 4) {
            hdr.set(
                "nchans",
                static_cast<int>(freq_n * (8U / static_cast<unsigned>(nbits))));
        } else {
            hdr.set("nchans", static_cast<int>(freq_n));
        }
    }
    const auto nbytes = std::filesystem::file_size(filename);
    hdr.set("file_size", static_cast<int>(std::min(
                             nbytes, static_cast<std::uintmax_t>(
                                         std::numeric_limits<int>::max()))));
    hdr.set("header_size", 0);
    const auto data_bytes = data.getStorageSize();
    hdr.set("data_size",
            static_cast<int>(std::min(
                data_bytes,
                static_cast<std::uint64_t>(std::numeric_limits<int>::max()))));
    hdr.update({});
}

void write_scalar_attr(DataSet& data,
                       const std::string& key,
                       const SigprocHeader& hdr) {
    if (key == "signed") {
        const std::int8_t v =
            static_cast<std::int8_t>(hdr.get<bool>("signed") ? -1 : 1);
        data.createAttribute(key, v);
        return;
    }
    if (key == "barycentric" || key == "pulsarcentric") {
        const std::int32_t v = hdr.get<bool>(key) ? 1 : 0;
        data.createAttribute(key, v);
        return;
    }
    const auto it = params::kSigprocKeys.find(key);
    if (it == params::kSigprocKeys.end()) {
        return;
    }
    switch (it->second.type) {
    case KeyType::kSInt:
        data.createAttribute(key, static_cast<std::int32_t>(hdr.get<int>(key)));
        break;
    case KeyType::kSDouble:
        data.createAttribute(key, hdr.get<double>(key));
        break;
    case KeyType::kSBool:
        data.createAttribute(
            key, static_cast<std::int32_t>(hdr.get<bool>(key) ? 1 : 0));
        break;
    case KeyType::kSString:
        data.createAttribute(key, hdr.get<std::string>(key));
        break;
    }
}

void write_header_attrs(DataSet& data, const SigprocHeader& hdr) {
    auto keys = hdr.encode_keys();
    if (hdr.has_freq_table()) {
        const bool have_nchans =
            std::find(keys.begin(), keys.end(), "nchans") != keys.end();
        if (!have_nchans) {
            keys.push_back("nchans");
        }
    }
    for (const auto& key : keys) {
        write_scalar_attr(data, key, hdr);
    }
    if (hdr.has_freq_table()) {
        data.createAttribute("fchannel", hdr.get_freq_table());
    }
    const std::vector<std::string> labels{"time", "feed_id", "frequency"};
    try {
        data.createAttribute("DIMENSION_LABELS", labels);
    } catch (const std::exception& ex) {
        spdlog::warn("Could not write DIMENSION_LABELS: {}", ex.what());
    }
}

[[nodiscard]] SizeType chunk_time(SizeType nsamples,
                                  SizeType nifs,
                                  SizeType packed_chans,
                                  SizeType elem_bytes) {
    SizeType chunk_t = kFbh5ChunkGulp;
    if (nsamples > 0) {
        chunk_t = std::min(chunk_t, nsamples);
    }
    const auto row_bytes              = nifs * packed_chans * elem_bytes;
    constexpr SizeType kMaxChunkBytes = 1 << 20;
    if (row_bytes > 0) {
        const auto cap = std::max(SizeType{1}, kMaxChunkBytes / row_bytes);
        chunk_t        = std::min(chunk_t, cap);
    }
    return std::max(chunk_t, SizeType{1});
}

void write_raw_slab(DataSet& data,
                    SizeType t0,
                    SizeType nsamps,
                    SizeType nifs,
                    SizeType packed_chans,
                    int nbits,
                    const std::byte* packed) {
    const std::vector<std::size_t> offset{t0, 0, 0};
    const std::vector<std::size_t> count{nsamps, nifs, packed_chans};
    auto sel = data.select(offset, count);
    if (nbits == 32) {
        sel.write_raw(reinterpret_cast<const float*>(packed));
    } else if (nbits == 16) {
        sel.write_raw(reinterpret_cast<const std::uint16_t*>(packed));
    } else {
        sel.write_raw(reinterpret_cast<const std::uint8_t*>(packed));
    }
}

void read_raw_slab(const DataSet& data,
                   SizeType t0,
                   SizeType nsamps,
                   SizeType nifs,
                   SizeType packed_chans,
                   int nbits,
                   std::byte* packed) {
    const std::vector<std::size_t> offset{t0, 0, 0};
    const std::vector<std::size_t> count{nsamps, nifs, packed_chans};
    auto sel = data.select(offset, count);
    if (nbits == 32) {
        sel.read_raw(reinterpret_cast<float*>(packed));
    } else if (nbits == 16) {
        sel.read_raw(reinterpret_cast<std::uint16_t*>(packed));
    } else {
        sel.read_raw(reinterpret_cast<std::uint8_t*>(packed));
    }
}

[[nodiscard]] DataSet create_fbh5_dataset(File& file,
                                          SizeType nifs,
                                          SizeType packed_chans,
                                          int nbits,
                                          SizeType nsamples) {
    const auto elem_b  = item_bytes(nbits);
    const auto chunk_t = chunk_time(nsamples, nifs, packed_chans, elem_b);
    DataSetCreateProps props;
    props.add(HighFive::Chunking(std::vector<hsize_t>{
        static_cast<hsize_t>(chunk_t), static_cast<hsize_t>(nifs),
        static_cast<hsize_t>(packed_chans)}));
    const std::vector<std::size_t> dims{0, nifs, packed_chans};
    const std::vector<std::size_t> maxdims{DataSpace::UNLIMITED, nifs,
                                           packed_chans};
    DataSpace space(dims, maxdims);
    if (nbits == 32) {
        return file.createDataSet<float>(std::string(kFbh5Dataset), space,
                                         props);
    }
    if (nbits == 16) {
        return file.createDataSet<std::uint16_t>(std::string(kFbh5Dataset),
                                                 space, props);
    }
    return file.createDataSet<std::uint8_t>(std::string(kFbh5Dataset), space,
                                            props);
}

} // namespace

bool is_stdio_name(std::string_view name) noexcept {
    return name.empty() || name == "-";
}

bool is_hdf5_path(std::string_view path) noexcept {
    return ends_with_ci(path, ".h5") || ends_with_ci(path, ".hdf5") ||
           ends_with_ci(path, ".fbh5");
}

bool file_has_hdf5_magic(std::string_view path) {
    std::ifstream in(std::string(path), std::ios::in | std::ios::binary);
    if (!in) {
        return false;
    }
    std::array<char, 8> buf{};
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    return magic_matches(buf.data(), static_cast<std::size_t>(in.gcount()));
}

bool is_hdf5_file(std::string_view path) {
    if (is_stdio_name(path)) {
        return false;
    }
    if (is_hdf5_path(path)) {
        return true;
    }
    return file_has_hdf5_magic(path);
}

bool stream_has_hdf5_magic(std::istream& in) {
    if (!detail::header_codec::is_istream_seekable(in)) {
        return false;
    }
    const auto pos = in.tellg();
    std::array<char, 8> buf{};
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    const auto n = static_cast<std::size_t>(in.gcount());
    in.clear();
    in.seekg(pos);
    if (!in) {
        in.clear();
        in.seekg(pos);
    }
    return magic_matches(buf.data(), n);
}

void require_hdf5_filesystem_path(std::string_view path) {
    if (is_stdio_name(path)) {
        throw std::invalid_argument(std::string(kHdf5NeedsPath));
    }
}

class Hdf5Reader::Impl {
public:
    explicit Impl(const std::string& filename)
        : m_file(filename, File::ReadOnly),
          m_data(open_dataset(m_file)),
          m_axes(map_axes(m_data)) {
        load_header_from_dataset(m_hdr, filename, m_data, m_axes);
        m_nbits    = m_hdr.get<int>("nbits");
        m_nifs     = m_hdr.get<int>("nifs");
        m_nchans   = m_hdr.get<int>("nchans");
        m_bitsinfo = bits::BitsInfo(static_cast<SizeType>(m_nbits));
        m_stride_len =
            static_cast<SizeType>(m_nchans) * static_cast<SizeType>(m_nifs);
        m_packed_chans  = packed_last_axis();
        m_stride_bytes  = static_cast<SizeType>(m_nifs) * m_packed_chans *
                          item_bytes(m_nbits);
        const auto dims = m_data.getSpace().getDimensions();
        m_nsamps        = dims[static_cast<std::size_t>(m_axes.time)];
        if (m_nchans > params::kMaxNchans) {
            throw std::invalid_argument(std::format(
                "nchans {} exceeds cap {}", m_nchans, params::kMaxNchans));
        }
        if (m_axes.time != 0 || m_axes.feed != 1 || m_axes.freq != 2) {
            spdlog::debug("FBH5 axis map time={} feed={} freq={}", m_axes.time,
                          m_axes.feed, m_axes.freq);
        }
        if (m_axes.time != 0 || m_axes.feed != 1 || m_axes.freq != 2) {
            throw std::runtime_error(
                "FBH5 files with non-(time,feed,frequency) axis order are "
                "not supported in this build");
        }
    }

    [[nodiscard]] const SigprocHeader& hdr() const noexcept { return m_hdr; }
    [[nodiscard]] SizeType nsamps() const noexcept { return m_nsamps; }
    [[nodiscard]] SizeType stride_len() const noexcept { return m_stride_len; }
    [[nodiscard]] SizeType stride_bytes() const noexcept {
        return m_stride_bytes;
    }

    SizeType read_values(SizeType start_sample,
                         SizeType nvalues,
                         std::vector<float>& block) {
        if (start_sample >= m_nsamps || nvalues == 0) {
            block.clear();
            return 0;
        }
        const auto max_values = (m_nsamps - start_sample) * m_stride_len;
        const auto want       = std::min(nvalues, max_values);
        const auto nsamps     = (want + m_stride_len - 1) / m_stride_len;
        std::vector<std::byte> packed(nsamps * m_stride_bytes);
        read_raw_slab(m_data, start_sample, nsamps,
                      static_cast<SizeType>(m_nifs), m_packed_chans, m_nbits,
                      packed.data());
        std::vector<float> full(nsamps * m_stride_len);
        packed_to_float(packed, full, m_bitsinfo);
        full.resize(want);
        block = std::move(full);
        return want;
    }

    void
    copy_packed(std::ostream& out, SizeType start_sample, SizeType nsamps) {
        if (nsamps == 0) {
            return;
        }
        if (start_sample + nsamps > m_nsamps) {
            throw std::runtime_error("HDF5 copy_packed: range past EOF");
        }
        std::vector<std::byte> packed(nsamps * m_stride_bytes);
        read_raw_slab(m_data, start_sample, nsamps,
                      static_cast<SizeType>(m_nifs), m_packed_chans, m_nbits,
                      packed.data());
        out.write(reinterpret_cast<const char*>(packed.data()),
                  static_cast<std::streamsize>(packed.size()));
        if (!out.good()) {
            throw std::runtime_error(
                "Failed while copying HDF5 sample payload");
        }
    }

private:
    static DataSet open_dataset(File& file) {
        require_filterbank_class(file);
        if (!file.exist(std::string(kFbh5Dataset))) {
            throw std::runtime_error("HDF5 file has no dataset 'data'");
        }
        return file.getDataSet(std::string(kFbh5Dataset));
    }

    [[nodiscard]] SizeType packed_last_axis() const {
        const auto dims   = m_data.getSpace().getDimensions();
        const auto freq_n = dims[static_cast<std::size_t>(m_axes.freq)];
        if (m_nbits == 1 || m_nbits == 2 || m_nbits == 4) {
            const auto expect = packed_axis_len(m_nchans, m_nbits);
            if (freq_n == expect) {
                return expect;
            }
            if (freq_n == static_cast<std::size_t>(m_nchans)) {
                throw std::runtime_error(
                    "Unpacked 1/2/4-bit FBH5 last axis is not supported");
            }
            throw std::runtime_error(std::format(
                "FBH5 frequency axis {} does not match packed length {}",
                freq_n, expect));
        }
        if (freq_n != static_cast<std::size_t>(m_nchans)) {
            throw std::runtime_error(
                std::format("FBH5 frequency axis {} does not match nchans {}",
                            freq_n, m_nchans));
        }
        return static_cast<SizeType>(m_nchans);
    }

    File m_file;
    DataSet m_data;
    AxisMap m_axes;
    SigprocHeader m_hdr;
    bits::BitsInfo m_bitsinfo{8};
    int m_nbits{};
    int m_nifs{};
    int m_nchans{};
    SizeType m_nsamps{};
    SizeType m_stride_len{};
    SizeType m_packed_chans{};
    SizeType m_stride_bytes{};
};

Hdf5Reader::Hdf5Reader(const std::string& filename) {
    require_hdf5_filesystem_path(filename);
    try {
        m_impl = std::make_unique<Impl>(filename);
    } catch (const std::invalid_argument&) {
        throw;
    } catch (const std::exception& ex) {
        throw std::runtime_error(
            std::format("Failed to open FBH5 '{}': {}", filename, ex.what()));
    }
}

Hdf5Reader::~Hdf5Reader()                                = default;
Hdf5Reader::Hdf5Reader(Hdf5Reader&&) noexcept            = default;
Hdf5Reader& Hdf5Reader::operator=(Hdf5Reader&&) noexcept = default;

const SigprocHeader& Hdf5Reader::hdr() const noexcept { return m_impl->hdr(); }

SizeType Hdf5Reader::nsamps() const noexcept { return m_impl->nsamps(); }

SizeType Hdf5Reader::stride_len() const noexcept {
    return m_impl->stride_len();
}

SizeType Hdf5Reader::stride_bytes() const noexcept {
    return m_impl->stride_bytes();
}

SizeType Hdf5Reader::read_values(SizeType start_sample,
                                 SizeType nvalues,
                                 std::vector<float>& block) {
    return m_impl->read_values(start_sample, nvalues, block);
}

void Hdf5Reader::copy_packed(std::ostream& out,
                             SizeType start_sample,
                             SizeType nsamps) {
    m_impl->copy_packed(out, start_sample, nsamps);
}

class Hdf5Writer::Impl {
public:
    Impl(const std::string& filename, SigprocHeader& hdr)
        : m_nbits(hdr.get<int>("nbits")),
          m_nifs(hdr.get<int>("nifs")),
          m_nchans(hdr.get<int>("nchans")),
          m_bitsinfo(static_cast<SizeType>(m_nbits)),
          m_packed_chans(packed_axis_len(m_nchans, m_nbits)),
          m_stride_len(static_cast<SizeType>(m_nchans) *
                       static_cast<SizeType>(m_nifs)),
          m_file(filename, File::Truncate | File::Create | File::ReadWrite),
          m_data(create_fbh5_dataset(
              m_file,
              static_cast<SizeType>(m_nifs),
              m_packed_chans,
              m_nbits,
              hdr.is_present("nsamples") && hdr.get<int>("nsamples") > 0
                  ? static_cast<SizeType>(hdr.get<int>("nsamples"))
                  : SizeType{0})) {
        m_file.createAttribute("CLASS", std::string(kFbh5Class));
        m_file.createAttribute("VERSION", std::string(kFbh5Version));
        write_header_attrs(m_data, hdr);
        if (!m_data.hasAttribute("nbits")) {
            m_data.createAttribute("nbits", static_cast<std::int32_t>(m_nbits));
        }
        if (!m_data.hasAttribute("nchans")) {
            m_data.createAttribute("nchans",
                                   static_cast<std::int32_t>(m_nchans));
        }
        if (!m_data.hasAttribute("nifs")) {
            m_data.createAttribute("nifs", static_cast<std::int32_t>(m_nifs));
        }
        if (!m_data.hasAttribute("nsamples")) {
            m_data.createAttribute("nsamples", std::int32_t{0});
        }
    }

    ~Impl() {
        try {
            finalize();
        } catch (const std::exception& ex) {
            spdlog::error("HDF5 writer finalize failed: {}", ex.what());
        }
    }

    void write_block(std::span<const float> samples) {
        if (samples.empty()) {
            return;
        }
        if (samples.size() % m_stride_len != 0) {
            throw std::invalid_argument(
                "HDF5 write_block length must be a multiple of nchans*nifs");
        }
        const auto nsamps = samples.size() / m_stride_len;
        std::vector<std::byte> packed(
            bits::packed_nbytes(samples.size(), m_bitsinfo));
        bits::from_float(samples, packed, m_bitsinfo);
        const auto new_total = m_written + nsamps;
        m_data.resize(
            {new_total, static_cast<std::size_t>(m_nifs), m_packed_chans});
        write_raw_slab(m_data, m_written, nsamps, static_cast<SizeType>(m_nifs),
                       m_packed_chans, m_nbits, packed.data());
        m_written = new_total;
    }

    void finalize() {
        if (m_finalized) {
            return;
        }
        m_finalized = true;
        m_data.resize(
            {m_written, static_cast<std::size_t>(m_nifs), m_packed_chans});
        if (m_data.hasAttribute("nsamples")) {
            m_data.getAttribute("nsamples")
                .write(static_cast<std::int32_t>(std::min(
                    m_written,
                    static_cast<SizeType>(std::numeric_limits<int>::max()))));
        }
        m_file.flush();
    }

private:
    int m_nbits{};
    int m_nifs{};
    int m_nchans{};
    bits::BitsInfo m_bitsinfo;
    SizeType m_packed_chans{};
    SizeType m_stride_len{};
    SizeType m_written{};
    bool m_finalized = false;
    File m_file;
    DataSet m_data;
};

Hdf5Writer::Hdf5Writer(const std::string& filename, SigprocHeader& hdr) {
    require_hdf5_filesystem_path(filename);
    try {
        m_impl = std::make_unique<Impl>(filename, hdr);
    } catch (const std::invalid_argument&) {
        throw;
    } catch (const std::exception& ex) {
        throw std::runtime_error(
            std::format("Failed to create FBH5 '{}': {}", filename, ex.what()));
    }
}

Hdf5Writer::~Hdf5Writer()                                = default;
Hdf5Writer::Hdf5Writer(Hdf5Writer&&) noexcept            = default;
Hdf5Writer& Hdf5Writer::operator=(Hdf5Writer&&) noexcept = default;

void Hdf5Writer::write_block(std::span<const float> samples) {
    m_impl->write_block(samples);
}

} // namespace sigproc::io
