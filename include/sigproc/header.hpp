#pragma once

#include <format>
#include <istream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <sigproc/common/params.hpp>
#include <sigproc/common/types.hpp>

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
     * @brief Read header data into this SigprocHeader.
     *
     * Attempts to read all standard SIGPROC header keywords. Unknown keys
     * are logged and skipped. Derived keys are recomputed on success.
     *
     * @param stream A binary input stream to read the header from.
     * @return true  if the reading is successful
     * @return false if the data file is not in standard format
     */
    bool fromstream(std::istream& stream);

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
    const auto it = m_data.find(std::string(key));
    if (it == m_data.end()) {
        throw std::runtime_error(std::format("Key '{}' not found", key));
    }
    const auto* value = std::get_if<T>(&it->second);
    if (value == nullptr) {
        throw std::runtime_error(
            std::format("Key '{}' has the wrong type", key));
    }
    return *value;
}

template <HeaderValueType T>
std::optional<T> SigprocHeader::try_get(std::string_view key) const noexcept {
    const auto it = m_data.find(std::string(key));
    if (it == m_data.end()) {
        return std::nullopt;
    }
    const auto* value = std::get_if<T>(&it->second);
    return value != nullptr ? std::optional<T>(*value) : std::nullopt;
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
