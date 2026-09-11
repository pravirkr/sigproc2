#pragma once

#include <format>
#include <functional>
#include <ios>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <sigproc/common/types.hpp>

namespace sigproc::detail::map_utils {

// C++20 replacement for boost printNameOfType
template <typename T> consteval std::string_view type_name() {
    std::string_view name = std::source_location::current().function_name();
#if defined(__clang__) || defined(__GNUC__)
    // For GCC/Clang: extract from "... [T = TypeName]"
    constexpr std::string_view kPrefix = "T = ";
    SizeType start                     = name.find(kPrefix);
    if (start == std::string_view::npos) {
        return "unknown";
    }
    start += kPrefix.size();
    SizeType end = name.find_first_of("];", start);
    if (end == std::string_view::npos) {
        end = name.size();
    }
    return name.substr(start, end - start);
#elif defined(_MSC_VER)
    // For MSVC: extract from "... type_name<TypeName>(void)"
    SizeType start = name.find('<');
    if (start == std::string_view::npos) {
        return "unknown";
    }
    ++start;
    SizeType end = name.rfind('>');
    if (end == std::string_view::npos || end <= start) {
        return "unknown";
    }
    return name.substr(start, end - start);

#else
    return "unknown";
#endif
}

// Get value from basic map
template <typename K, typename V>
const V& get_value(const std::unordered_map<K, V>& smap, const K& key) {
    auto it = smap.find(key);
    if (it == smap.end()) {
        throw std::runtime_error(
            std::format("Key '{}' not found in map<{}, {}>", key,
                        type_name<K>(), type_name<V>()));
    }
    return it->second;
}

template <typename K, typename V>
V get_value_copy(const std::unordered_map<K, V>& smap, const K& key) {
    return get_value(smap, key);
}

// Get value from variant map
template <typename T, typename K, typename VariantType>
T get_value_variant(const std::unordered_map<K, VariantType>& smap,
                    const K& key) {
    auto it = smap.find(key);
    if (it == smap.end()) {
        throw std::runtime_error(std::format(
            "Key '{}' not found in map<{}, variant>", key, type_name<K>()));
    }
    const auto* value = std::get_if<T>(&it->second);
    if (!value) {
        throw std::runtime_error(
            std::format("Key '{}' in map<{}, variant> does not hold type {}",
                        key, type_name<K>(), type_name<T>()));
    }
    return *value;
}

// Safe version that returns std::optional instead of throwing
template <typename K, typename V>
std::optional<std::reference_wrapper<const V>>
try_get_value(const std::unordered_map<K, V>& smap, const K& key) {
    auto it = smap.find(key);
    if (it == smap.end()) {
        return std::nullopt;
    }
    return std::cref(it->second);
}

template <typename T, typename K, typename VariantType>
std::optional<T>
try_get_value_variant(const std::unordered_map<K, VariantType>& smap,
                      const K& key) {
    auto it = smap.find(key);
    if (it == smap.end()) {
        return std::nullopt;
    }

    const auto* value = std::get_if<T>(&it->second);
    return value ? std::optional<T>(*value) : std::nullopt;
}

} // namespace sigproc::detail::map_utils

// --- Binary I/O Helpers ---
namespace sigproc::detail::io_utils {
template <class DataType, class BinaryStream>
DataType read_value(BinaryStream& stream) {
    DataType value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(DataType));
    if (!stream) [[unlikely]] {
        throw std::runtime_error(
            std::format("Failed to read {} bytes for type {}", sizeof(DataType),
                        map_utils::type_name<DataType>()));
    }
    return value;
}

template <typename BinaryStream>
inline void write_string(BinaryStream& stream, std::string_view str) {
    const auto len = str.size();
    stream.write(reinterpret_cast<const char*>(&len), sizeof(SizeType));
    if (len > 0) {
        stream.write(str.data(), static_cast<std::streamsize>(len));
    }
    if (!stream.good()) [[unlikely]] {
        throw std::runtime_error("Failed to write string to stream");
    }
}

inline void write_string(std::vector<char>& buffer, std::string_view str) {
    const auto len      = str.size();
    const auto* len_ptr = reinterpret_cast<const char*>(&len);
    buffer.insert(buffer.end(), len_ptr, len_ptr + sizeof(len));
    buffer.insert(buffer.end(), str.begin(), str.end());
}

template <class DataType, class BinaryStream>
static void write_value(BinaryStream& stream,
                        const std::string& name,
                        const DataType& val) {
    write_string(stream, name);
    stream.write(reinterpret_cast<const char*>(&val), sizeof(DataType));
    if (!stream.good()) [[unlikely]] {
        throw std::runtime_error(
            std::format("Failed to write value for key '{}'", name));
    }
}

template <class DataType>
static void write_value(std::vector<char>& buffer,
                        const std::string& name,
                        const DataType& val) {
    write_string(buffer, name);
    const auto* val_bytes = reinterpret_cast<const char*>(&val);
    buffer.insert(buffer.end(), val_bytes, val_bytes + sizeof(DataType));
}

template <typename BinaryStream>
[[nodiscard]] inline std::string read_string(BinaryStream& stream) {
    std::streamsize len{};
    stream.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!stream) [[unlikely]] {
        throw std::runtime_error("Failed to read string length from stream");
    }

    // Sanity check: reasonable maximum string length (1 MB)
    constexpr SizeType kMaxStringLen = 1024UL * 1024UL;
    if (static_cast<SizeType>(len) > kMaxStringLen) [[unlikely]] {
        throw std::runtime_error(std::format(
            "Invalid string length: {} (max: {})", len, kMaxStringLen));
    }

    if (len == 0) {
        return std::string{};
    }
    std::string result(len, '\0');
    if (len > 0) {
        stream.read(result.data(), len);
        if (!stream) {
            throw std::runtime_error("Failed to read string from stream.");
        }
    }
    return result;
}

} // namespace sigproc::detail::io_utils
