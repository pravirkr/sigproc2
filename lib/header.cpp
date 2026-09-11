#include <sigproc/header.hpp>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <sigproc/astro.hpp>
#include <sigproc/common/params.hpp>

#include "sigproc/exceptions.hpp"
#include "sigproc/header_codec.hpp"
#include "sigproc/utils.hpp"

namespace sigproc::io {

namespace {

const std::array<std::string_view, 8> kRequiredCore = {
    "machine_id", "telescope_id", "data_type", "nchans",
    "nbits",      "nifs",         "tstart",    "tsamp"};

[[nodiscard]] bool is_empty_string_value(const HeaderValue& value) {
    const auto* str = std::get_if<std::string>(&value);
    return str != nullptr && str->empty();
}

} // namespace

SigprocHeader::SigprocHeader() {
    std::unordered_map<std::string, params::KeyInfo> header_keys =
        params::kSigprocKeys;
    header_keys.insert(params::kExtraKeys.begin(), params::kExtraKeys.end());
    for (const auto& [key, key_info] : header_keys) {
        assign_data(key, detail::map_utils::get_value(params::kDefaultKeyValues,
                                                      key_info.type));
    }
}

void SigprocHeader::assign_data(std::string_view key,
                                HeaderValue value) noexcept {
    m_data[std::string(key)] = std::move(value);
}

void SigprocHeader::note_present(std::string_view key,
                                 bool empty_string) noexcept {
    std::string k{key};
    if (empty_string || params::kExtraKeys.contains(k) || k == "fchannel") {
        m_present.erase(k);
        return;
    }
    m_present.insert(std::move(k));
}

void SigprocHeader::set(std::string_view key, HeaderValue value) noexcept {
    std::string k{key};
    const bool empty = is_empty_string_value(value);
    m_data[k]        = std::move(value);
    note_present(k, empty);
}

void SigprocHeader::update(const std::map<std::string, HeaderValue>& newmap) {
    for (const auto& [key, value] : newmap) {
        set(key, value);
    }
    update_internal();
}

bool SigprocHeader::has_freq_table() const noexcept {
    return !m_freq_table.empty();
}

std::span<const std::byte> SigprocHeader::raw_header() const noexcept {
    return m_raw_header;
}

bool SigprocHeader::is_present(std::string_view key) const noexcept {
    return m_present.contains(std::string(key));
}

void SigprocHeader::set_freq_table(std::vector<double> freqs) {
    if (freqs.size() > static_cast<std::size_t>(params::kMaxNchans)) {
        throw std::invalid_argument(std::format(
            "nchans {} exceeds cap {}", freqs.size(), params::kMaxNchans));
    }
    m_freq_table = std::move(freqs);
    if (m_freq_table.empty()) {
        return;
    }
    if (m_freq_table.size() > 4096) {
        spdlog::warn("frequency table has {} channels (original cap 4096)",
                     m_freq_table.size());
    }
    assign_data("nchans", static_cast<int>(m_freq_table.size()));
    note_present("nchans", false);
    assign_data("fch1", 0.0);
    assign_data("foff", 0.0);
    m_present.erase("fch1");
    m_present.erase("foff");
}

std::vector<double> SigprocHeader::get_freq_table() const {
    if (!m_freq_table.empty()) {
        return m_freq_table;
    }
    const auto nchans = get<int>("nchans");
    if (nchans < 0) {
        throw std::invalid_argument("nchans is negative");
    }
    if (nchans > params::kMaxNchans) {
        throw std::invalid_argument(std::format("nchans {} exceeds cap {}",
                                                nchans, params::kMaxNchans));
    }
    const auto fch1 = get<double>("fch1");
    const auto foff = get<double>("foff");
    std::vector<double> freqs(static_cast<std::size_t>(nchans));
    for (int i = 0; i < nchans; ++i) {
        freqs[static_cast<std::size_t>(i)] =
            fch1 + (static_cast<double>(i) * foff);
    }
    return freqs;
}

std::vector<float> SigprocHeader::get_freqs() const {
    const auto table = get_freq_table();
    std::vector<float> freqs(table.size());
    for (std::size_t i = 0; i < table.size(); ++i) {
        freqs[i] = static_cast<float>(table[i]);
    }
    return freqs;
}

std::vector<double>
SigprocHeader::get_dm_delays(double dm, std::string_view ref_freq) const {
    auto nchans = get<int>("nchans");
    auto freqs  = get_freqs();
    std::vector<double> delays(static_cast<std::size_t>(nchans));
    double fch_ref = 0.0;

    if (ref_freq == "ch1") {
        fch_ref = get<double>("fch1");
        if (fch_ref == 0.0 && !m_freq_table.empty()) {
            fch_ref = m_freq_table.front();
        }
    } else if (ref_freq == "center") {
        fch_ref = get<double>("fcenter");
    } else {
        throw std::invalid_argument(
            std::format("Unknown reference frequency: {}", ref_freq));
    }
    for (auto i = 0; i < nchans; ++i) {
        delays[static_cast<std::size_t>(i)] =
            kDispConst * dm *
            (1.0 / (static_cast<double>(freqs[static_cast<std::size_t>(i)]) *
                    static_cast<double>(freqs[static_cast<std::size_t>(i)])) -
             1.0 / (fch_ref * fch_ref));
    }
    return delays;
}

bool SigprocHeader::fromfile(std::string_view filename) {
    std::ifstream file_stream(std::string(filename),
                              std::ios::in | std::ios::binary);
    if (!file_stream.is_open()) {
        throw std::runtime_error(std::format("Cannot open file: {}", filename));
    }
    error_check::check_file(file_stream, std::string(filename));
    try {
        return fromstream(file_stream);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::format(
            "Error reading header from file '{}': {}", filename, e.what()));
    }
}

void SigprocHeader::tofile(std::string_view filename) {
    std::ofstream file_stream(std::string(filename),
                              std::ios::out | std::ios::binary);
    if (!file_stream.is_open()) {
        throw std::runtime_error(std::format("Cannot open file: {}", filename));
    }
    tostream(file_stream);
}

bool SigprocHeader::fromstream(std::istream& stream) {
    *this = SigprocHeader();

    using detail::header_codec::HeaderStream;
    using detail::header_codec::is_istream_seekable;
    using detail::header_codec::probe_unknown_value;

    const bool seekable = is_istream_seekable(stream);
    HeaderStream hs(stream);

    std::string token;
    if (!hs.try_read_token(token) || token != "HEADER_START") {
        if (seekable) {
            stream.clear();
            stream.seekg(0);
            return false;
        }
        throw std::runtime_error(
            "Input is not a SIGPROC header and the stream is not seekable");
    }

    bool saw_nsamples = false;
    std::optional<std::string> pending;
    std::unordered_set<std::string> warned;

    auto next_token = [&]() -> std::string {
        if (pending) {
            auto t = std::move(*pending);
            pending.reset();
            return t;
        }
        return hs.read_token();
    };

    while (true) {
        token = next_token();
        if (token == "HEADER_END") {
            break;
        }
        if (token == "FREQUENCY_START" || token == "FREQUENCY_END") {
            continue;
        }
        if (token == "fchannel") {
            const double freq = hs.read_f64();
            m_freq_table.push_back(freq);
            if (m_freq_table.size() == 4097) {
                spdlog::warn(
                    "frequency table exceeds original 4096-channel cap");
            }
            assign_data("fch1", 0.0);
            assign_data("foff", 0.0);
            continue;
        }

        const auto it = params::kSigprocKeys.find(token);
        if (it == params::kSigprocKeys.end()) {
            if (warned.insert(token).second) {
                spdlog::warn("read_header: unknown parameter {}", token);
            }
            pending = probe_unknown_value(hs, token);
            continue;
        }

        switch (it->second.type) {
        case KeyType::kSInt: {
            const auto value = static_cast<int>(hs.read_i32());
            if (token == "nchans" && value > params::kMaxNchans) {
                throw std::invalid_argument(std::format(
                    "nchans {} exceeds cap {}", value, params::kMaxNchans));
            }
            if (token == "nsamples") {
                saw_nsamples = true;
            }
            set(token, value);
            break;
        }
        case KeyType::kSDouble:
            set(token, hs.read_f64());
            break;
        case KeyType::kSBool:
            if (token == "signed") {
                const auto isign = hs.read_i8();
                set(token, isign < 0);
            } else {
                // barycentric / pulsarcentric: int32 0/1 on disk
                const auto flag = hs.read_i32();
                set(token, flag != 0);
            }
            break;
        case KeyType::kSString:
            set(token, hs.read_token());
            break;
        }
        m_file_order.push_back(token);
    }

    m_raw_header           = hs.take_raw();
    const auto header_size = static_cast<int>(m_raw_header.size());
    assign_data("header_size", header_size);

    if (seekable) {
        stream.clear();
        stream.seekg(0, std::ios::end);
        const auto end_pos = stream.tellg();
        if (!stream || end_pos == std::streampos(-1)) {
            throw std::runtime_error(
                "Failed to measure seekable stream size after header parse");
        }
        const auto file_size = static_cast<std::int64_t>(end_pos);
        const auto data_size =
            file_size - static_cast<std::int64_t>(header_size);
        if (data_size < 0) {
            throw std::runtime_error(
                std::format("Invalid file structure: data_size={}", data_size));
        }
        // Keep extra keys as int (OQ-10); clamp rather than widening
        // HeaderValue.
        assign_data("file_size",
                    static_cast<int>(std::min(
                        file_size, static_cast<std::int64_t>(INT_MAX))));
        assign_data("data_size",
                    static_cast<int>(std::min(
                        data_size, static_cast<std::int64_t>(INT_MAX))));
        if (!saw_nsamples) {
            const auto nchans = get<int>("nchans");
            const auto nifs   = get<int>("nifs");
            const auto nbits  = get<int>("nbits");
            if (nchans <= 0 || nifs <= 0 || nbits <= 0) {
                throw std::runtime_error(std::format(
                    "Invalid header values: nchans={}, nifs={}, nbits={}",
                    nchans, nifs, nbits));
            }
            const auto denom    = static_cast<std::uint64_t>(nchans) *
                                  static_cast<std::uint64_t>(nifs) *
                                  static_cast<std::uint64_t>(nbits);
            const auto nsamples = static_cast<int>(
                (static_cast<std::uint64_t>(data_size) * 8ULL) / denom);
            assign_data("nsamples", nsamples);
        }
        stream.clear();
        stream.seekg(header_size, std::ios::beg);
    } else {
        assign_data("file_size", 0);
        assign_data("data_size", 0);
        if (!saw_nsamples) {
            assign_data("nsamples", 0);
        }
    }

    if (m_freq_table.size() > 4096) {
        spdlog::warn("frequency table has {} channels (original cap 4096)",
                     m_freq_table.size());
    }

    update_internal();
    return true;
}

void SigprocHeader::update_internal() {
    assign_data("telescope",
                detail::map_utils::get_value(params::kTelescopeIds,
                                             get<int>("telescope_id")));
    assign_data("backend", detail::map_utils::get_value(
                               params::kMachineIds, get<int>("machine_id")));
    assign_data("datatype", detail::map_utils::get_value(
                                params::kDataTypes, get<int>("data_type")));
    assign_data("frame", get<bool>("barycentric")
                             ? std::string("barycentric")
                             : (get<bool>("pulsarcentric")
                                    ? std::string("pulsarcentric")
                                    : std::string("topocentric")));
    assign_data("tobs", get<double>("tsamp") *
                            static_cast<double>(get<int>("nsamples")));
    assign_data("tobs_str", astro::get_duration_string(get<double>("tobs")));
    assign_data("bandwidth", std::abs(get<double>("foff")) *
                                 static_cast<double>(get<int>("nchans")));
    assign_data("ftop", get<double>("fch1") - (0.5 * get<double>("foff")));
    assign_data("fbottom", get<double>("ftop") +
                               (get<double>("foff") *
                                static_cast<double>(get<int>("nchans"))));
    assign_data("fcenter", get<double>("ftop") +
                               (0.5 * get<double>("foff") *
                                static_cast<double>(get<int>("nchans"))));
    if (!m_freq_table.empty()) {
        assign_data("ftop", m_freq_table.front());
        assign_data("fbottom", m_freq_table.back());
        assign_data("fcenter",
                    0.5 * (m_freq_table.front() + m_freq_table.back()));
        if (m_freq_table.size() > 1) {
            assign_data("bandwidth",
                        std::abs(m_freq_table.front() - m_freq_table.back()));
        }
    }
    assign_data("ra", astro::radec_to_str(get<double>("src_raj")));
    assign_data("dec", astro::radec_to_str(get<double>("src_dej")));
    assign_data("ra_rad", astro::ra_to_rad(get<std::string>("ra")));
    assign_data("dec_rad", astro::dec_to_rad(get<std::string>("dec")));
    assign_data("obs_date", astro::mjd_to_gregorian(
                                static_cast<int>(get<double>("tstart"))));
}

void SigprocHeader::append_encoded_key(std::vector<char>& buffer,
                                       const std::string& key) const {
    using detail::header_codec::append_f64;
    using detail::header_codec::append_i32;
    using detail::header_codec::append_i8;
    using detail::header_codec::append_token;

    append_token(buffer, key);
    if (key == "signed") {
        const auto signed_flag = get<bool>("signed");
        append_i8(buffer, static_cast<std::int8_t>(signed_flag ? -1 : 1));
        return;
    }
    if (key == "barycentric" || key == "pulsarcentric") {
        append_i32(buffer, get<bool>(key) ? 1 : 0);
        return;
    }
    const auto it = params::kSigprocKeys.find(key);
    if (it == params::kSigprocKeys.end()) {
        throw std::runtime_error(
            std::format("Cannot encode unknown header key '{}'", key));
    }
    switch (it->second.type) {
    case KeyType::kSInt:
        append_i32(buffer, static_cast<std::int32_t>(get<int>(key)));
        break;
    case KeyType::kSDouble:
        append_f64(buffer, get<double>(key));
        break;
    case KeyType::kSBool:
        append_i32(buffer, get<bool>(key) ? 1 : 0);
        break;
    case KeyType::kSString:
        append_token(buffer, get<std::string>(key));
        break;
    }
}

void SigprocHeader::append_freq_table(std::vector<char>& buffer) const {
    using detail::header_codec::append_f64;
    using detail::header_codec::append_i32;
    using detail::header_codec::append_token;

    append_token(buffer, "FREQUENCY_START");
    append_token(buffer, "nchans");
    append_i32(buffer, static_cast<std::int32_t>(m_freq_table.size()));
    for (double freq : m_freq_table) {
        append_token(buffer, "fchannel");
        append_f64(buffer, freq);
    }
    append_token(buffer, "FREQUENCY_END");
}

std::vector<std::string> SigprocHeader::encode_keys() const {
    auto skip_ordinary = [this](const std::string& key) {
        if (params::kExtraKeys.contains(key) || key == "fchannel") {
            return true;
        }
        if (!m_freq_table.empty() &&
            (key == "fch1" || key == "foff" || key == "nchans")) {
            return true;
        }
        const auto it = m_data.find(key);
        if (it != m_data.end() && is_empty_string_value(it->second)) {
            return true;
        }
        return false;
    };

    std::vector<std::string> keys;
    std::set<std::string> used;

    auto maybe_add = [&](const std::string& key) {
        if (used.contains(key) || skip_ordinary(key)) {
            return;
        }
        keys.push_back(key);
        used.insert(key);
    };

    if (!m_file_order.empty()) {
        for (const auto& key : m_file_order) {
            if (m_present.contains(key)) {
                maybe_add(key);
            }
        }
        for (const auto view : params::kEncodeOrder) {
            const std::string key{view};
            if (m_present.contains(key)) {
                maybe_add(key);
            }
        }
        for (const auto& key : m_present) {
            maybe_add(key);
        }
        return keys;
    }

    std::set<std::string> emit = m_present;
    for (const auto view : kRequiredCore) {
        emit.emplace(view);
    }
    if (m_freq_table.empty()) {
        emit.emplace("fch1");
        emit.emplace("foff");
    }
    for (const auto view : params::kEncodeOrder) {
        const std::string key{view};
        if (emit.contains(key)) {
            maybe_add(key);
        }
    }
    for (const auto& key : emit) {
        maybe_add(key);
    }
    return keys;
}

std::vector<char> SigprocHeader::tobuffer() const {
    using detail::header_codec::append_token;

    std::vector<char> buffer;
    buffer.reserve(4096);
    append_token(buffer, "HEADER_START");

    const auto keys    = encode_keys();
    bool table_emitted = m_freq_table.empty();
    for (const auto& key : keys) {
        append_encoded_key(buffer, key);
        if (!table_emitted && key == "data_type") {
            append_freq_table(buffer);
            table_emitted = true;
        }
    }
    if (!table_emitted) {
        append_freq_table(buffer);
    }
    append_token(buffer, "HEADER_END");
    return buffer;
}

} // namespace sigproc::io
