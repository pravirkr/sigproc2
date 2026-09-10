#pragma once

#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "sigproc/common/params.hpp"
#include "sigproc/common/types.hpp"
#include "sigproc/detail/utils.hpp"

namespace sigproc::io {

// ===================== PUBLIC API =====================
class SigprocHeader {
public:
    /**
     * @brief Construct a new SigprocHeader object
     *
     * All keys are initialized to their default values.
     */
    SigprocHeader();

    /**
     * @brief Get the sigproc header value for given key.
     *
     * @tparam T  The data type of the value stored.
     * @param key The key to read.
     * @return T  The value mapped for given key.
     * @throws std::runtime_error if the key is not found
     */
    template <HeaderValueType T>
    [[nodiscard]] T get(std::string_view key) const;

    /**
     * @brief Try to get the sigproc header value for given key (non-throwing).
     *
     * @tparam T  The data type of the value stored.
     * @param key The key to read.
     * @return std::optional<T> The value if found and correct type, nullopt
     * otherwise.
     */
    template <HeaderValueType T>
    [[nodiscard]] std::optional<T> try_get(std::string_view key) const noexcept;

    /**
     * @brief Update/write the sigproc header value for given key.
     *
     * @tparam T    The data type of the value.
     * @param key   The key to write/update the mapped value.
     * @param value The value to write.
     */
    template <HeaderValueType T>
    void set(std::string_view key, T value) noexcept;

    /**
     * @brief Update/write a header value from a variant directly.
     *
     * Overload used when the value is already a HeaderValue (or a string
     * literal), e.g. when copying default values or merging maps.
     *
     * @param key   The key to write/update the mapped value.
     * @param value The variant value to store.
     */
    void set(std::string_view key, HeaderValue value) noexcept;

    /**
     * @brief Merge values from a map into this header (in place).
     *
     * Existing keys are overwritten and derived values are recomputed.
     *
     * @param newmap The map of key/value pairs to merge.
     */
    void update(const std::map<std::string, HeaderValue>& newmap);

    /**
     * @brief Get the frequency array
     *
     * @return std::vector<float> The frequency array
     */
    [[nodiscard]] std::vector<float> get_freqs() const noexcept;

    /**
     * @brief Get the DM delays
     *
     * @param dm The DM value
     * @param ref_freq The reference frequency
     * @return std::vector<double> The DM delays
     */
    [[nodiscard]] std::vector<double>
    get_dm_delays(double dm, std::string_view ref_freq = "top") const;

    /**
     * @brief Create a new SigprocHeader with updated values from the map.
     *
     * Creates a copy of the current header and updates it with values from
     * newmap.
     *
     * @tparam T The data type of the value
     * @param newmap The map to create the new SigprocHeader from
     * @return SigprocHeader The new SigprocHeader
     */
    template <HeaderValueType T>
    [[nodiscard]] SigprocHeader
    new_header(const std::map<std::string, T>& newmap) noexcept;

    /**
     * @brief Write the SigprocHeader to a binary stream
     *
     * @tparam BinaryStream The binary stream type
     * @param stream The binary stream to write to
     */
    template <BinaryWritableType BinaryStream>
    void tostream(BinaryStream& stream);

    /**
     * @brief Read header data into a SigprocHeader (or similar) structure.
     *
     * Function attempts to read all standard sigproc header keywords.
     * Only header attributes with matching keywords are updated in the
     * given Header object.
     *
     * @tparam BinaryStream
     * @param stream A binary stream to read header from.
     * @return true  if the reading is successful
     * @return false if the data file is not in standard format
     */
    template <BinaryReadableType BinaryStream>
    bool fromstream(BinaryStream& stream);

    /**
     * @brief Read the SigprocHeader from a file
     *
     * @param filename The name of the file to read from
     * @return true if the reading is successful
     * @return false if the file is not in standard format
     */
    bool fromfile(std::string_view filename);

    /**
     * @brief Write the SigprocHeader to a file.
     *
     * @param filename The name of the file to write to.
     */
    void tofile(std::string_view filename);

    template <typename T>
    SigprocHeader new_header(const std::map<std::string, T>& newmap);

private:
    std::unordered_map<std::string, HeaderValue> m_data;

    [[nodiscard]] std::vector<char> tobuffer() const;
    void update_internal();
};

// ===================== TEMPLATE IMPLEMENTATIONS =====================
template <HeaderValueType T> T SigprocHeader::get(std::string_view key) const {
    return detail::map_utils::get_value_variant<T, std::string, HeaderValue>(
        m_data, std::string(key));
}

template <HeaderValueType T>
std::optional<T> SigprocHeader::try_get(std::string_view key) const noexcept {
    return detail::map_utils::try_get_value_variant<T>(m_data,
                                                       std::string(key));
}

template <HeaderValueType T>
void SigprocHeader::set(std::string_view key, T value) noexcept {
    m_data[std::string(key)] = std::move(value);
}

template <BinaryWritableType BinaryStream>
void SigprocHeader::tostream(BinaryStream& stream) {
    const auto buffer = tobuffer();
    stream.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!stream.good()) {
        throw std::runtime_error("Failed to write header to stream.");
    }
}

template <BinaryReadableType BinaryStream>
bool SigprocHeader::fromstream(BinaryStream& stream) {
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
            std::cerr << std::format(
                "Warning: read_header: unknown parameter {}\n", token);
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

} // namespace sigproc::io
