#include "sigproc/header.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "sigproc/astro.hpp"
#include "sigproc/common/params.hpp"
#include "sigproc/detail/exceptions.hpp"
#include "sigproc/detail/utils.hpp"

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
