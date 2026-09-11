#pragma once

#include <cstddef>
#include <format>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <span>
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
     * All keys are initialized to their default values. Defaults are not
     * written on encode until `set`/`update` marks them present.
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
     * Marks the key as present for sparse encode. Empty strings are never
     * written (they are erased from the write-set).
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
     * @brief Channel centres: the FREQUENCY_START table if present, else
     *        `fch1 + i * foff` as double.
     */
    [[nodiscard]] std::vector<double> get_freq_table() const;

    /// @brief True when the header carries an explicit `fchannel` table.
    [[nodiscard]] bool has_freq_table() const noexcept;

    /**
     * @brief Replace the explicit channel-frequency table.
     *
     * Nonempty tables are exclusive with `fch1`/`foff` on encode (rule 8).
     * Pass an empty vector to clear the table and restore the `fch1`/`foff`
     * path.
     */
    void set_freq_table(std::vector<double> freqs);

    /**
     * @brief Get the frequency array (float copy of `get_freq_table()`).
     *
     * @return std::vector<float> The frequency array
     */
    [[nodiscard]] std::vector<float> get_freqs() const;

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
    template <typename T>
    [[nodiscard]] SigprocHeader
    new_header(const std::map<std::string, T>& newmap);

    /**
     * @brief Write the SigprocHeader to a binary stream
     *
     * Sparse encode: write-set plus required core, in `kEncodeOrder` (or
     * original file order if this header was read from a stream).
     *
     * @tparam BinaryStream The binary stream type
     * @param stream The binary stream to write to
     */
    template <BinaryWritableType BinaryStream>
    void tostream(BinaryStream& stream);

    /**
     * @brief Read header data into this SigprocHeader.
     *
     * Seekable failed magic: rewind and return false. Non-seekable failed
     * magic: throw. Unknown keys are probed (sizes 4, 8, 1) without seekg
     * restore.
     *
     * @param stream A binary input stream to read the header from.
     * @return true  if the reading is successful
     * @return false if the data file is not in standard format (seekable)
     */
    bool fromstream(std::istream& stream);

    /**
     * @brief Read the SigprocHeader from a file
     *
     * @param filename The name of the file to read from.
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

    /**
     * @brief Bytes teed by the last successful `fromstream`.
     *
     * Empty if the header was built in memory. Used by
     * `FilterbankReader::write_raw_header`.
     */
    [[nodiscard]] std::span<const std::byte> raw_header() const noexcept;

    /// @brief True if `key` is in the encode write-set.
    [[nodiscard]] bool is_present(std::string_view key) const noexcept;

    /// Sparse-encode key list (file order / `kEncodeOrder`). FBH5 writes these
    /// as dataset attributes.
    [[nodiscard]] std::vector<std::string> encode_keys() const;

    /**
     * @brief Patch present keys in `raw_header()` without changing length.
     *
     * Strings are space-padded or truncated to the existing on-disk length
     * (original `filedit`). Throws if `raw_header()` is empty, a key is
     * absent from the on-disk header, or a value would need a different
     * encoded width.
     */
    [[nodiscard]] std::vector<std::byte>
    patched_raw_header(const std::map<std::string, HeaderValue>& updates) const;

private:
    std::unordered_map<std::string, HeaderValue> m_data;
    std::vector<std::string> m_file_order;
    std::set<std::string> m_present;
    std::vector<double> m_freq_table;
    std::vector<std::byte> m_raw_header;

    [[nodiscard]] std::vector<char> tobuffer() const;
    void update_internal();
    void note_present(std::string_view key, bool empty_string) noexcept;
    void assign_data(std::string_view key, HeaderValue value) noexcept;
    void append_encoded_key(std::vector<char>& buffer,
                            const std::string& key) const;
    void append_freq_table(std::vector<char>& buffer) const;
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
    std::string k{key};
    bool empty = false;
    if constexpr (std::same_as<T, std::string>) {
        empty = value.empty();
    }
    m_data[k] = std::move(value);
    note_present(k, empty);
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
    SigprocHeader newhdr(*this);
    for (const auto& param : newmap) {
        newhdr.set(param.first, param.second);
    }
    newhdr.update_internal();
    return newhdr;
}

} // namespace sigproc::io
