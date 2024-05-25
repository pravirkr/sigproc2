#pragma once

#include <fstream>
#include <vector>

#include <sigproc/params.hpp>

class FileBase {
public:
    FileBase(const std::vector<std::string>& filenames, std::string mode);
    ~FileBase();
    bool eos() const;

    // Disable copy and move constructors
    FileBase(const FileBase&)            = delete;
    FileBase& operator=(const FileBase&) = delete;
    FileBase(FileBase&&)                 = delete;
    FileBase& operator=(FileBase&&)      = delete;

private:
    std::vector<std::string> m_filenames;
    std::string m_mode;
    std::fstream m_file_stream;
    size_t m_ifileCur = SIZE_MAX;

    std::unordered_map<std::string, std::ios_base::openmode> m_modeMap = {
        {"r", std::ios::in | std::ios::binary},
        {"w", std::ios::out | std::ios::binary}};

    void open_file(size_t ifile);
    void close_current();
};

class FileReader : public FileBase {
public:
    FileReader(const StreamInfo& stream_info, const std::string& mode = "r",
               int nbits = 8);
    int cur_data_pos_file() const;
    int cur_data_pos_stream() const;
    std::vector<uint8_t> cread(int nunits) const;
    int creadinto(std::vector<uint8_t>& read_buffer,
                  std::vector<uint8_t>& unpack_buffer);
    void seek(int offset, int whence = 0) const;

private:
    StreamInfo sinfo;
    int nbits;
    BitsInfo bitsinfo;

    void _seek2hdr(int fileid) const;
    void _seek_set(int offset) const;
};

class FileIO {
public:
    /**
     * @brief Construct a new File IO object
     *
     * @param filename The name of filename to read/write
     * @param nbits number of bits in the data
     */
    FileIO(const std::string& filename, int nbits);

    /**
     * @brief Destroy the File IO object
     *
     */
    ~FileIO();

    /* read nread units of data from stream */
    void read_data(std::vector<float>& block, int nread);

    /* write block of data to stream */
    void write_data(const std::vector<float>& block, int nwrite);

    /* get to the right place in the file stream. */
    void seek_bytes(int nbytes, bool offset = false);

private:
    int nbits;
    BitsInfo bitsinfo;
    std::fstream file_stream;
};
