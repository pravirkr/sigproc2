#pragma once

#include <cstdint>
#include <fstream>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <sigproc/bits.hpp>
#include <sigproc/common/types.hpp>

namespace sigproc::io {

/**
 * @brief Byte-oriented sample I/O for a SIGPROC payload stream.
 *
 * Filename empty or "-" selects stdin (binary). A wrapped `istream` is
 * non-owning; the caller keeps it alive.
 */
class FileIO {
public:
    FileIO(const std::string& filename, int nbits);
    FileIO(std::istream& in, int nbits);
    ~FileIO();

    FileIO(const FileIO&)            = delete;
    FileIO& operator=(const FileIO&) = delete;
    FileIO(FileIO&&)                 = delete;
    FileIO& operator=(FileIO&&)      = delete;

    [[nodiscard]] bool seekable() const noexcept;

    /**
     * @brief Convert up to `nread` values to float.
     *
     * Returns how many floats were stored (short at EOF). Resizes `block` to
     * that count. Never invents zeros past EOF.
     */
    SizeType read_data(std::vector<float>& block, int nread);

    /**
     * @brief Quantize and write `nwrite` floats as packed payload.
     *
     * Requires an output stream (not used by `FilterbankReader`).
     */
    void write_data(const std::vector<float>& block, int nwrite);

    /**
     * @brief Absolute (`offset==false`) or relative seek.
     *
     * Throws if the stream is not seekable unless the destination is the
     * current position.
     */
    void seek_bytes(std::int64_t nbytes, bool offset = false);

    /// Skip forward: seek if seekable, else read-and-discard.
    void skip_bytes(std::int64_t nbytes);

    /// Copy `nbytes` of packed payload to `out` (no unpack).
    void copy_bytes(std::ostream& out, std::int64_t nbytes);

    [[nodiscard]] std::int64_t tell_bytes();

private:
    SizeType m_nbits;
    bits::BitsInfo m_bitsinfo;
    std::istream* m_in  = nullptr;
    std::ostream* m_out = nullptr;
    std::unique_ptr<std::ifstream> m_owned_in;
    bool m_seekable = false;
};

} // namespace sigproc::io
