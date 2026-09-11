#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <istream>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

#include <sigproc/filterbank.hpp>
#include <sigproc/header.hpp>

namespace sigproc::test {

inline void append_i32(std::vector<char>& buf, std::int32_t value) {
    const auto* p = reinterpret_cast<const char*>(&value);
    buf.insert(buf.end(), p, p + sizeof(value));
}

inline void append_token(std::vector<char>& buf, std::string_view str) {
    append_i32(buf, static_cast<std::int32_t>(str.size()));
    buf.insert(buf.end(), str.begin(), str.end());
}

inline void append_i8(std::vector<char>& buf, std::int8_t value) {
    buf.push_back(static_cast<char>(value));
}

inline void append_f64(std::vector<char>& buf, double value) {
    const auto* p = reinterpret_cast<const char*>(&value);
    buf.insert(buf.end(), p, p + sizeof(value));
}

[[nodiscard]] inline std::vector<std::string>
header_key_order(std::span<const char> bytes) {
    std::vector<std::string> keys;
    std::size_t i = 0;
    auto read_i32 = [&]() {
        std::int32_t len{};
        std::memcpy(&len, bytes.data() + i, 4);
        i += 4;
        return len;
    };
    auto read_tok = [&]() {
        const auto len = read_i32();
        std::string s(bytes.data() + i, static_cast<std::size_t>(len));
        i += static_cast<std::size_t>(len);
        return s;
    };
    const auto start = read_tok();
    if (start != "HEADER_START") {
        return keys;
    }
    while (i < bytes.size()) {
        const auto tok = read_tok();
        if (tok == "HEADER_END") {
            break;
        }
        if (tok == "FREQUENCY_START" || tok == "FREQUENCY_END") {
            keys.push_back(tok);
            continue;
        }
        keys.push_back(tok);
        if (tok == "signed") {
            i += 1;
        } else if (tok == "fchannel") {
            i += 8;
        } else if (tok == "rawdatafile" || tok == "source_name") {
            (void)read_tok();
        } else if (tok == "barycentric" || tok == "pulsarcentric") {
            i += 4;
        } else {
            // int32 or float64
            i += (tok == "tstart" || tok == "tsamp" || tok == "fch1" ||
                  tok == "foff" || tok == "refdm" || tok == "period" ||
                  tok == "az_start" || tok == "za_start" || tok == "src_raj" ||
                  tok == "src_dej")
                     ? 8
                     : 4;
        }
    }
    return keys;
}

[[nodiscard]] inline sigproc::io::SigprocHeader make_tiny_header() {
    sigproc::io::SigprocHeader hdr;
    hdr.set("source_name", std::string("TINY"));
    hdr.set("data_type", 1);
    hdr.set("nchans", 8);
    hdr.set("nbits", 8);
    hdr.set("nifs", 1);
    hdr.set("nsamples", 16);
    hdr.set("fch1", 1400.0);
    hdr.set("foff", -1.0);
    hdr.set("tsamp", 0.001);
    hdr.set("tstart", 50000.0);
    return hdr;
}

[[nodiscard]] inline std::vector<float> ramp_samples(int nchans, int nsamps) {
    std::vector<float> samples(static_cast<std::size_t>(nchans * nsamps));
    for (int t = 0; t < nsamps; ++t) {
        for (int c = 0; c < nchans; ++c) {
            samples[static_cast<std::size_t>(t * nchans + c)] =
                static_cast<float>((t * nchans + c) % 256);
        }
    }
    return samples;
}

/// Non-seekable `streambuf`: `tellg() == -1`, `seekoff` fails (PR-01 tests
/// a–c).
class NonSeekableBuf : public std::streambuf {
public:
    explicit NonSeekableBuf(std::string data) : m_data(std::move(data)) {
        auto* p = m_data.data();
        setg(p, p, p + m_data.size());
    }

protected:
    pos_type seekoff(off_type,
                     std::ios_base::seekdir,
                     std::ios_base::openmode) override {
        return pos_type(off_type(-1));
    }
    pos_type seekpos(pos_type, std::ios_base::openmode) override {
        return pos_type(off_type(-1));
    }

private:
    std::string m_data;
};

class NonSeekableIStream : private NonSeekableBuf, public std::istream {
public:
    explicit NonSeekableIStream(std::string data)
        : NonSeekableBuf(std::move(data)),
          std::istream(static_cast<NonSeekableBuf*>(this)) {}
};

} // namespace sigproc::test
