#pragma once

#include <complex>
#include <cstddef>
#include <string>
#include <variant>

namespace sigproc {

using SizeType    = std::size_t;
using IndexType   = std::ptrdiff_t;
using ComplexType = std::complex<float>;
enum class KeyType : std::uint8_t { kSInt, kSDouble, kSBool, kSString };

using HeaderValue = std::variant<int, double, bool, std::string>;
template <typename T>
concept HeaderValueType = std::same_as<T, int> || std::same_as<T, double> ||
                          std::same_as<T, bool> || std::same_as<T, std::string>;

template <typename T>
concept BinaryReadableType = requires(T& stream, char* buf, std::streamsize n) {
    { stream.read(buf, n) } -> std::same_as<T&>;
    { stream.gcount() } -> std::convertible_to<std::streamsize>;
};

template <typename T>
concept BinaryWritableType =
    requires(T& stream, const char* buf, std::streamsize n) {
        { stream.write(buf, n) } -> std::same_as<T&>;
    };

template <typename T>
concept BinaryStreamType = BinaryReadableType<T> && BinaryWritableType<T>;

// Constants for dispersion calculations
inline constexpr double kDispConstLK =
    4.1488080e3; // L&K Handbook of Pulsar Astronomy
inline constexpr double kDispConst = kDispConstLK; // MHz^2 cm^3 pc^-1 s

template <typename T>
concept IntegralDataType = std::is_integral_v<T>;

} // namespace sigproc
