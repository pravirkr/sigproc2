#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <istream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sigproc/common/params.hpp>
#include <sigproc/common/types.hpp>

namespace sigproc::detail::header_codec {

inline constexpr std::int32_t kMinStringLen = 1;
inline constexpr std::int32_t kMaxStringLen = 80;

[[nodiscard]] inline bool is_istream_seekable(std::istream& stream) {
    if (!stream) {
        return false;
    }
    const auto pos = stream.tellg();
    if (!stream || pos == std::streampos(-1)) {
        stream.clear();
        return false;
    }
    return true;
}

[[nodiscard]] inline bool is_header_token(std::string_view token) {
    if (token == "HEADER_START" || token == "HEADER_END" ||
        token == "FREQUENCY_START" || token == "FREQUENCY_END") {
        return true;
    }
    return params::kSigprocKeys.contains(std::string(token));
}

inline void
append_bytes(std::vector<char>& buf, const void* ptr, std::size_t nbytes) {
    const auto* bytes = static_cast<const char*>(ptr);
    buf.insert(buf.end(), bytes, bytes + nbytes);
}

inline void append_i32(std::vector<char>& buf, std::int32_t value) {
    append_bytes(buf, &value, sizeof(value));
}

inline void append_i8(std::vector<char>& buf, std::int8_t value) {
    buf.push_back(static_cast<char>(value));
}

inline void append_f64(std::vector<char>& buf, double value) {
    append_bytes(buf, &value, sizeof(value));
}

inline void append_token(std::vector<char>& buf, std::string_view str) {
    if (str.size() < static_cast<std::size_t>(kMinStringLen) ||
        str.size() > static_cast<std::size_t>(kMaxStringLen)) {
        throw std::invalid_argument(
            "SIGPROC header string length must be 1..80");
    }
    append_i32(buf, static_cast<std::int32_t>(str.size()));
    buf.insert(buf.end(), str.begin(), str.end());
}

/// Teeing reader: every byte taken from the underlying stream is appended to
/// `raw()`. Lookahead (unread) bytes stay in `raw()` and are not teed twice.
class HeaderStream {
public:
    explicit HeaderStream(std::istream& in) : m_in(in) {}

    [[nodiscard]] bool seekable() { return is_istream_seekable(m_in); }

    [[nodiscard]] std::span<const std::byte> raw() const noexcept {
        return m_raw;
    }

    [[nodiscard]] std::vector<std::byte> take_raw() { return std::move(m_raw); }

    void read_exact(void* dst, std::size_t nbytes) {
        auto* out       = static_cast<std::byte*>(dst);
        std::size_t got = 0;
        while (got < nbytes && m_look_pos < m_look.size()) {
            out[got++] = m_look[m_look_pos++];
        }
        if (m_look_pos >= m_look.size()) {
            m_look.clear();
            m_look_pos = 0;
        }
        if (got < nbytes) {
            const auto need = nbytes - got;
            m_in.read(reinterpret_cast<char*>(out + got),
                      static_cast<std::streamsize>(need));
            const auto nread = static_cast<std::size_t>(m_in.gcount());
            m_raw.insert(m_raw.end(), out + got, out + got + nread);
            if (nread != need) {
                throw std::runtime_error(
                    "Unexpected EOF while reading SIGPROC header");
            }
        }
    }

    [[nodiscard]] bool try_read_exact(void* dst, std::size_t nbytes) {
        try {
            read_exact(dst, nbytes);
            return true;
        } catch (const std::runtime_error&) {
            return false;
        }
    }

    void unread(std::span<const std::byte> bytes) {
        std::vector<std::byte> next(bytes.begin(), bytes.end());
        if (m_look_pos < m_look.size()) {
            next.insert(next.end(),
                        m_look.begin() +
                            static_cast<std::ptrdiff_t>(m_look_pos),
                        m_look.end());
        }
        m_look     = std::move(next);
        m_look_pos = 0;
    }

    [[nodiscard]] std::int32_t read_i32() {
        std::int32_t value{};
        read_exact(&value, sizeof(value));
        return value;
    }

    [[nodiscard]] std::int8_t read_i8() {
        std::int8_t value{};
        read_exact(&value, sizeof(value));
        return value;
    }

    [[nodiscard]] double read_f64() {
        double value{};
        read_exact(&value, sizeof(value));
        return value;
    }

    /// Length-prefixed string; throws if the length is not in 1..80.
    [[nodiscard]] std::string read_token() {
        const auto len = read_i32();
        if (len < kMinStringLen || len > kMaxStringLen) {
            throw std::runtime_error(
                std::format("Invalid SIGPROC header string length: {}", len));
        }
        std::string token(static_cast<std::size_t>(len), '\0');
        read_exact(token.data(), static_cast<std::size_t>(len));
        return token;
    }

    /// First-token helper: invalid length is a failed magic, not a hard error.
    [[nodiscard]] bool try_read_token(std::string& token) {
        std::int32_t len{};
        if (!try_read_exact(&len, sizeof(len))) {
            return false;
        }
        if (len < kMinStringLen || len > kMaxStringLen) {
            return false;
        }
        token.assign(static_cast<std::size_t>(len), '\0');
        return try_read_exact(token.data(), static_cast<std::size_t>(len));
    }

private:
    std::istream& m_in;
    std::vector<std::byte> m_raw;
    std::vector<std::byte> m_look;
    std::size_t m_look_pos = 0;
};

/// Probe an unknown key's payload (K unknown-key buffer). Returns the next
/// header token on success. Throws if no candidate size {4,8,1} works.
[[nodiscard]] inline std::string
probe_unknown_value(HeaderStream& hs, std::string_view unknown_key) {
    std::array<std::byte, 8> probe{};
    hs.read_exact(probe.data(), probe.size());

    std::vector<std::byte> data(probe.begin(), probe.end());

    auto fetch = [&](std::size_t pos) -> std::byte {
        if (pos >= data.size()) {
            std::byte extra{};
            hs.read_exact(&extra, 1);
            data.push_back(extra);
        }
        return data[pos];
    };

    auto try_size = [&](std::size_t value_size) -> std::optional<std::string> {
        std::size_t pos = value_size;
        std::int32_t len{};
        auto* lenp = reinterpret_cast<std::byte*>(&len);
        for (std::size_t i = 0; i < sizeof(len); ++i) {
            lenp[i] = fetch(pos++);
        }
        if (len < kMinStringLen || len > kMaxStringLen) {
            return std::nullopt;
        }
        std::string next(static_cast<std::size_t>(len), '\0');
        for (std::int32_t i = 0; i < len; ++i) {
            next[static_cast<std::size_t>(i)] = static_cast<char>(fetch(pos++));
        }
        if (!is_header_token(next)) {
            return std::nullopt;
        }
        if (pos < data.size()) {
            hs.unread(std::span<const std::byte>(data.data() + pos,
                                                 data.size() - pos));
        }
        return next;
    };

    for (const std::size_t sz :
         {std::size_t{4}, std::size_t{8}, std::size_t{1}}) {
        if (auto next = try_size(sz)) {
            return *next;
        }
    }
    throw std::runtime_error(std::format(
        "Failed to skip unknown SIGPROC header key '{}'", unknown_key));
}

} // namespace sigproc::detail::header_codec
