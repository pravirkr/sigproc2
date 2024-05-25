#pragma once

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>

#include <sigproc/angles.hpp>
#include <sigproc/exceptions.hpp>
#include <sigproc/header.hpp>
#include <sigproc/utils.hpp>

// Default constructor, initializes all keys to default values
SigprocHeader::SigprocHeader() {
    std::unordered_map<std::string, KeyInfo> header_keys = kSigprocKeys;
    header_keys.insert(kExtraKeys.begin(), kExtraKeys.end());
    for (const auto& [key, keyInfo] : header_keys) {
        set(key, map_utils::get_value(kDefaultKeyValues, keyInfo.type));
    }
}

template <typename T> T SigprocHeader::get(const std::string& key) const {
    return map_utils::get_value_variant<std::string, T>(m_data, key);
}

template <typename T>
void SigprocHeader::set(const std::string& key, const T& value) {
    m_data[key] = value;
}

std::vector<float> SigprocHeader::get_freqs() const {
    auto nchans = get<int>("nchans");
    auto fch1   = get<double>("fch1");
    auto foff   = get<double>("foff");
    std::vector<float> freqs(nchans);
    for (auto i = 0; i < nchans; ++i) {
        freqs[i] = fch1 + i * foff;
    }
    return freqs;
}

std::vector<double> SigprocHeader::get_dm_delays(double dm,
                                                 std::string ref_freq) const {
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
            fmt::format("Unknown reference frequency: {}", ref_freq));
    }
    for (auto i = 0; i < nchans; ++i) {
        delays[i] = kDMConst * dm *
                    (1.0 / (freqs[i] * freqs[i]) - 1.0 / (fch_ref * fch_ref));
    }
    return delays;
}

template <typename T>
SigprocHeader
SigprocHeader::new_header(const std::map<std::string, T>& newmap) {
    SigprocHeader newhdr(*this); // Copy the current header
    for (const auto& param : newmap) {
        newhdr.set(param.first, param.second);
    }
    newhdr.update_internal();
    return newhdr;
}

template <class BinaryStream>
void SigprocHeader::tostream(BinaryStream& stream) {
    auto buffer = tobuffer();
    stream.write(buffer.data(), buffer.size());
}

bool SigprocHeader::fromfile(const std::string& filename) {
    std::fstream file_stream;
    file_stream.open(filename.c_str(),
                     std::ifstream::in | std::ifstream::binary);
    ErrorChecker::check_file(file_stream, filename);
    bool success = fromstream(file_stream);
    file_stream.close();
    return success;
}

template <class BinaryStream>
bool SigprocHeader::fromstream(BinaryStream& stream) {
    int header_size{}, data_size{}, file_size{};
    std::string token;
    token = read_string(stream);
    if (token != "HEADER_START") {
        stream.seekg(0, std::ios::beg);
        return false;
    }

    while (true) {
        token = read_string(stream);
        if (token == "HEADER_END") {
            header_size = static_cast<int>(stream.tellg());
            break;
        }
        auto it = kSigprocKeys.find(token);
        if (it != kSigprocKeys.end()) {
            KeyType type = it->second.type;
            switch (type) {
            case KeyType::kSInt:
                set(token, read_value<int>(stream));
                break;

            case KeyType::kSDouble:
                set(token, read_value<double>(stream));
                break;

            case KeyType::kSBool:
                set(token, read_value<bool>(stream));
                break;

            case KeyType::kSString:
                set(token, read_string(stream));
                break;
            }
        } else {
            fmt::print(stderr, "Warning: read_header: unknown parameter {}\n",
                       token);
        }
    }

    if (!stream.good()) {
        throw std::runtime_error("Stream error while reading header.");
    }

    stream.seekg(0, std::ios::end);
    file_size     = static_cast<int>(stream.tellg());
    data_size     = file_size - header_size;
    auto nsamples = get<int>("nsamples");
    if (nsamples == 0) {
        // Compute the number of samples from the file size
        nsamples = data_size / get<int>("nchans") / get<int>("nifs") * 8 /
                   get<int>("nbits");
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
    set("telescope",
        map_utils::get_value(kTelescopeIds, get<int>("telescope_id")));
    set("backend", map_utils::get_value(kMachineIds, get<int>("machine_id")));
    set("datatype", map_utils::get_value(kDataTypes, get<int>("data_type")));
    set("frame",
        get<bool>("barycentric")
            ? "barycentric"
            : (get<bool>("pulsarcentric") ? "pulsarcentric" : "topocentric"));
    set("tobs", get<double>("tsamp") * get<int>("nsamples"));
    set("tobs_str", dates::get_duration_string(get<double>("tobs")));
    set("bandwidth", std::abs(get<double>("foff")) * get<int>("nchans"));
    set("ftop", get<double>("fch1") - 0.5 * get<double>("foff"));
    set("fbottom",
        get<double>("ftop") + get<double>("foff") * get<int>("nchans"));
    set("fcenter",
        get<double>("ftop") + 0.5 * get<double>("foff") * get<int>("nchans"));
    set("ra", angles::radec_to_str(get<double>("src_raj")));
    set("dec", angles::radec_to_str(get<double>("src_dej")));
    set("ra_rad", angles::ra_to_rad(get<std::string>("ra")));
    set("dec_rad", angles::dec_to_rad(get<std::string>("dec")));
    set("obs_date", dates::mjd_to_gregorian(get<double>("tstart")));
}

std::vector<char> SigprocHeader::tobuffer() const {
    std::vector<char> buffer;
    write_string(buffer, "HEADER_START");
    for (const auto& [key, keyInfo] : kSigprocKeys) {
        switch (keyInfo.type) {
        case KeyType::kSInt:
            write_value(buffer, key, get<int>(key));
            break;

        case KeyType::kSDouble:
            write_value(buffer, key, get<double>(key));
            break;

        case KeyType::kSBool:
            write_value(buffer, key, get<bool>(key));
            break;

        case KeyType::kSString:
            write_string(buffer, key);
            write_string(buffer, get<std::string>(key));
            break;
        }
    }
    write_string(buffer, "HEADER_END");
    return buffer;
}

template <class BinaryStream>
std::string SigprocHeader::read_string(BinaryStream& stream) {
    size_t len = 0;
    stream.read(static_cast<char*>(static_cast<void*>(&len)), sizeof(size_t));
    if (!stream || len < 0) {
        throw std::runtime_error("Invalid string length in stream.");
    }
    std::string str(len, '\0');
    if (len > 0) {
        stream.read(str.c_str(), len);
        if (!stream) {
            throw std::runtime_error("Failed to read string from stream.");
        }
    }
    return str;
}

template <class DataType, class BinaryStream>
DataType SigprocHeader::read_value(BinaryStream& stream) {
    DataType value;
    stream.read(static_cast<char*>(static_cast<void*>(&value)),
                sizeof(DataType));
    if (!stream) {
        throw std::runtime_error("Failed to read value from stream.");
    }
    return value;
}

template <class BinaryStream>
void SigprocHeader::write_string(BinaryStream& stream, const std::string& str) {
    size_t len = str.size();
    stream.write(static_cast<const char*>(static_cast<const void*>(&len)),
                 sizeof(size_t));
    stream.write(str.c_str(), len * sizeof(char));
}

void SigprocHeader::write_string(std::vector<char>& buffer,
                                 const std::string& str) {
    size_t len = str.size();
    const char* len_bytes =
        static_cast<const char*>(static_cast<const void*>(&len));
    buffer.insert(buffer.end(), len_bytes, len_bytes + sizeof(size_t));
    buffer.insert(buffer.end(), str.begin(), str.end());
}

template <class DataType, class BinaryStream>
void SigprocHeader::write_value(BinaryStream& stream, const std::string& name,
                                const DataType& val) {
    write_string(stream, name);
    stream.write(static_cast<const char*>(static_cast<const void*>(&val)),
                 sizeof(DataType));
}

template <class DataType>
void SigprocHeader::write_value(std::vector<char>& buffer,
                                const std::string& name, const DataType& val) {
    write_string(buffer, name);
    const char* len_bytes =
        static_cast<const char*>(static_cast<const void*>(&val));
    buffer.insert(buffer.end(), len_bytes, len_bytes + sizeof(DataType));
}