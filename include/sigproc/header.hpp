#pragma once

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <sigproc/params.hpp>

class SigprocHeader {
public:
    SigprocHeader();

    /**
     * @brief Get the sigproc header value for given key.
     *
     * It will throw boost::bad_get exception if you try to get it with
     * the wrong type. Yeah, that's the price we pay for static typing.
     *
     * It will throw Invalid_argument exception if the key cannot be found.
     *
     * @tparam T  The data type of the value stored.
     * @param key The key to read.
     * @return T  The value mapped for given key.
     */
    template <typename T> T get(const std::string& key) const;

    /**
     * @brief Update/write the sigproc header value for given key.
     *
     * TODO: Should we check types before updating?
     *
     * @tparam T    The data type of the value.
     * @param key   The key to write/update the mapped value.
     * @param value The value to write.
     */
    template <typename T> void set(const std::string& key, const T& value);

    [[nodiscard]] std::vector<float> get_freqs() const;
    [[nodiscard]] std::vector<double>
    get_dm_delays(double dm, std::string ref_freq = "top") const;

    template <typename T>
    SigprocHeader new_header(const std::map<std::string, T>& newmap);

    template <class BinaryStream> void tostream(BinaryStream& stream);

    bool fromfile(const std::string& filename);

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
    template <class BinaryStream> bool fromstream(BinaryStream& stream);

private:
    std::map<std::string, SighdrTypes> m_data;

    void update_internal();
    [[nodiscard]] std::vector<char> tobuffer() const;

    /**
     * @brief Read a string from a binary stream.
     *
     * @tparam BinaryStream  A binary stream type.
     * @param stream  A binary stream to read data.
     * @return std::string A string read from the stream.
     */
    template <class BinaryStream> std::string read_string(BinaryStream& stream);

    /**
     * @brief Read a key with value from a binary stream.
     *
     * @tparam DataType   A data type of the value: int, long, float, double,
     * unsigned char.
     * @tparam BinaryStream A binary stream type.
     * @param stream  A binary stream to read data.
     * @return DataType  A value read from the stream.
     */
    template <class DataType, class BinaryStream>
    DataType read_value(BinaryStream& stream);

    /**
     * @brief Write a string to a binary stream.
     *
     * @tparam BinaryStream A binary stream type.
     * @param stream  A binary stream to write data.
     * @param str     A string to be written.
     */
    template <class BinaryStream>
    static void write_string(BinaryStream& stream, const std::string& str);

    /**
     * @brief Write a string to a buffer.
     *
     * @param buffer A buffer to write data.
     * @param str  A string to be written.
     */
    static void write_string(std::vector<char>& buffer, const std::string& str);

    /**
     * @brief Write key with value to a binary stream.
     *
     * @tparam BinaryStream A binary stream type.
     * @tparam DataType    A data type of the value: int, long, float, double,
     * unsigned char.
     * @param stream  A binary stream to write data.
     * @param name    Keyword name.
     * @param val     Keyword value.
     */
    template <class DataType, class BinaryStream>
    static void write_value(BinaryStream& stream, const std::string& name,
                            const DataType& val);

    /**
     * @brief Write key with value to a buffer.
     *
     * @tparam DataType A data type of the value: int, long, float, double,
     * unsigned char.
     * @param buffer  A buffer to write data.
     * @param name Keyword name.
     * @param val Keyword value.
     */
    template <class DataType>
    static void write_value(std::vector<char>& buffer, const std::string& name,
                            const DataType& val);
};
