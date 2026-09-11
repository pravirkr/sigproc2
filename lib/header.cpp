#include <sigproc/header.hpp>

#include <cmath>
#include <fstream>
#include <istream>
#include <string>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

#include <sigproc/astro.hpp>
#include <sigproc/common/params.hpp>

#include "sigproc/exceptions.hpp"
#include "sigproc/utils.hpp"

namespace sigproc::io {

// Default constructor, initializes all keys to default values
SigprocHeader::SigprocHeader() {
    std::unordered_map<std::string, params::KeyInfo> header_keys =
        params::kSigprocKeys;
    header_keys.insert(params::kExtraKeys.begin(), params::kExtraKeys.end());
    for (const auto& [key, keyInfo] : header_keys) {
        set(key, detail::map_utils::get_value(params::kDefaultKeyValues,
                                              keyInfo.type));
    }
}

void SigprocHeader::set(std::string_view key, HeaderValue value) noexcept {
    m_data[std::string(key)] = std::move(value);
}

void SigprocHeader::update(const std::map<std::string, HeaderValue>& newmap) {
    for (const auto& [key, value] : newmap) {
        set(key, value);
    }
    update_internal();
}

std::vector<float> SigprocHeader::get_freqs() const noexcept {
    auto nchans = get<int>("nchans");
    auto fch1   = get<double>("fch1");
    auto foff   = get<double>("foff");
    std::vector<float> freqs(nchans);
    for (auto i = 0; i < nchans; ++i) {
        freqs[i] = static_cast<float>(fch1 + (i * foff));
    }
    return freqs;
}

std::vector<double>
SigprocHeader::get_dm_delays(double dm, std::string_view ref_freq) const {
    auto nchans = get<int>("nchans");
    auto freqs  = get_freqs();
    std::vector<double> delays(nchans);
    double fch_ref = 0.0;

    if (ref_freq == "ch1") {
        fch_ref = get<double>("fch1");
    } else if (ref_freq == "center") {
        fch_ref = get<double>("fcenter");
    } else {
        throw std::invalid_argument(
            std::format("Unknown reference frequency: {}", ref_freq));
    }
    for (auto i = 0; i < nchans; ++i) {
        delays[i] = kDispConst * dm *
                    (1.0 / (freqs[i] * freqs[i]) - 1.0 / (fch_ref * fch_ref));
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
    int header_size{}, data_size{}, file_size{};
    std::string token = detail::io_utils::read_string(stream);
    if (token != "HEADER_START") {
        stream.seekg(0, std::ios::beg);
        return false;
    }

    // Read header key-value pairs
    while (true) {
        token = detail::io_utils::read_string(stream);
        if (token == "HEADER_END") {
            header_size = static_cast<int>(stream.tellg());
            break;
        }
        const auto it = params::kSigprocKeys.find(token);
        if (it != params::kSigprocKeys.end()) {
            const KeyType type = it->second.type;
            switch (type) {
            case KeyType::kSInt:
                set(token, detail::io_utils::read_value<int>(stream));
                break;
            case KeyType::kSDouble:
                set(token, detail::io_utils::read_value<double>(stream));
                break;
            case KeyType::kSBool:
                set(token, detail::io_utils::read_value<bool>(stream));
                break;
            case KeyType::kSString:
                set(token, detail::io_utils::read_string(stream));
                break;
            }
        } else {
            spdlog::warn("read_header: unknown parameter {}", token);
        }
    }

    if (!stream.good()) {
        throw std::runtime_error("Stream error while reading header.");
    }

    stream.seekg(0, std::ios::end);
    file_size = static_cast<int>(stream.tellg());
    data_size = file_size - header_size;
    if (data_size < 0) [[unlikely]] {
        throw std::runtime_error(
            std::format("Invalid file structure: data_size={}", data_size));
    }
    auto nsamples = get<int>("nsamples");
    if (nsamples == 0) {
        // Compute the number of samples from the file size
        const auto nchans = get<int>("nchans");
        const auto nifs   = get<int>("nifs");
        const auto nbits  = get<int>("nbits");
        if (nchans <= 0 || nifs <= 0 || nbits <= 0) [[unlikely]] {
            throw std::runtime_error(std::format(
                "Invalid header values: nchans={}, nifs={}, nbits={}", nchans,
                nifs, nbits));
        }
        const auto denominator = static_cast<SizeType>(nchans) *
                                 static_cast<SizeType>(nifs) *
                                 static_cast<SizeType>(nbits);
        if (denominator == 0) [[unlikely]] {
            throw std::runtime_error("Division by zero computing nsamples");
        }
        nsamples = static_cast<int>((static_cast<SizeType>(data_size) * 8UL) /
                                    denominator);
        set("nsamples", nsamples);
    }
    set("header_size", header_size);
    set("data_size", data_size);
    set("file_size", file_size);
    update_internal();

    // Seek back to the end of the header
    stream.seekg(header_size, std::ios::beg);
    return true;
}

void SigprocHeader::update_internal() {
    set("telescope", detail::map_utils::get_value(params::kTelescopeIds,
                                                  get<int>("telescope_id")));
    set("backend", detail::map_utils::get_value(params::kMachineIds,
                                                get<int>("machine_id")));
    set("datatype", detail::map_utils::get_value(params::kDataTypes,
                                                 get<int>("data_type")));
    set("frame",
        get<bool>("barycentric")
            ? "barycentric"
            : (get<bool>("pulsarcentric") ? "pulsarcentric" : "topocentric"));
    set("tobs", get<double>("tsamp") * get<int>("nsamples"));
    set("tobs_str", astro::get_duration_string(get<double>("tobs")));
    set("bandwidth", std::abs(get<double>("foff")) * get<int>("nchans"));
    set("ftop", get<double>("fch1") - (0.5 * get<double>("foff")));
    set("fbottom",
        get<double>("ftop") + (get<double>("foff") * get<int>("nchans")));
    set("fcenter",
        get<double>("ftop") + (0.5 * get<double>("foff") * get<int>("nchans")));
    set("ra", astro::radec_to_str(get<double>("src_raj")));
    set("dec", astro::radec_to_str(get<double>("src_dej")));
    set("ra_rad", astro::ra_to_rad(get<std::string>("ra")));
    set("dec_rad", astro::dec_to_rad(get<std::string>("dec")));
    set("obs_date",
        astro::mjd_to_gregorian(static_cast<int>(get<double>("tstart"))));
}

[[nodiscard]] std::vector<char> SigprocHeader::tobuffer() const {
    std::vector<char> buffer;
    buffer.reserve(4096); // Reasonable initial size for header
    detail::io_utils::write_string(buffer, "HEADER_START");
    for (const auto& [key, keyInfo] : params::kSigprocKeys) {
        switch (keyInfo.type) {
        case KeyType::kSInt:
            detail::io_utils::write_value(buffer, key, get<int>(key));
            break;

        case KeyType::kSDouble:
            detail::io_utils::write_value(buffer, key, get<double>(key));
            break;

        case KeyType::kSBool:
            detail::io_utils::write_value(buffer, key, get<bool>(key));
            break;

        case KeyType::kSString:
            detail::io_utils::write_string(buffer, key);
            detail::io_utils::write_string(buffer, get<std::string>(key));
            break;
        }
    }
    detail::io_utils::write_string(buffer, "HEADER_END");
    return buffer;
}

} // namespace sigproc::io
