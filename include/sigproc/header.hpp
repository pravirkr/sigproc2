#pragma once

#include <boost/variant.hpp>
#include <string>
#include <map>
#include <cmath>
#include <iostream>
#include <fstream>

#include <sigproc/params.hpp>
#include <sigproc/utils.hpp>

#include "angles.hpp"

std::string get_telescope_name(int telescope_id) {
    if (utils::exists_key(telescope_ids, telescope_id)) {
        return telescope_ids.at(telescope_id);
    } else {
        std::string msg = fmt::format("Cannot find key {}.", telescope_id);
        throw std::invalid_argument(msg);
    }
}

std::string get_backend_name(int machine_id) {
    if (utils::exists_key(machine_ids, machine_id)) {
        return machine_ids.at(machine_id);
    } else {
        std::string msg = fmt::format("Cannot find key {}.", machine_id);
        throw std::invalid_argument(msg);
    }
}

std::string get_datatype(int datatype) {
    if (utils::exists_key(data_types, datatype)) {
        return data_types.at(datatype);
    } else {
        std::string msg = fmt::format("Cannot find key {}.", datatype);
        throw std::invalid_argument(msg);
    }
}

/**
 * @brief Class for handling of sigproc format headers.
 *
 */
class SigprocHeader {
public:
    /**
     * @brief Construct a new Sigproc Header object
     *
     * Initializes all values to zero.
     */
    SigprocHeader() {
        std::map<std::string, key_type> header_keys = sigproc_keys;
        header_keys.insert(extra_keys.begin(), extra_keys.end());
        for (const auto& param : header_keys) {
            switch (param.second) {
                case key_type::sInt:
                    set(param.first, 0);
                    break;
                case key_type::sDouble:
                    set(param.first, 0.0);
                    break;
                case key_type::sBool:
                    set(param.first, false);
                    break;
                case key_type::sString:
                    set(param.first, std::string());
                    break;
            }
        }
    }

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
    template <class T>
    T get(const std::string& key) const {
        if (!utils::exists_key(data, key)) {
            std::string msg = fmt::format("Cannot find key {}.", key);
            throw std::invalid_argument(msg);
        }
        try {
            return boost::get<T>(data.find(key)->second);
        } catch (boost::bad_get& e) {
            std::string msg
                = fmt::format("Getting key '{}' with wrong type: {}.\n{}", key,
                              utils::printNameOfType<T>(), e.what());
            throw std::invalid_argument(msg);
        }
    }

    /**
     * @brief Update/write the sigproc header value for given key.
     *
     * TODO: Should we check types before updating?
     *
     * @tparam T    The data type of the value.
     * @param key   The key to write/update the mapped value.
     * @param value The value to write.
     */
    template <typename T>
    void set(const std::string& key, const T& value) {
        data[key] = value;
    }

    /**
     * @brief Update the sigproc header map.
     *
     * @tparam T     The data type of the new map values.
     * @param newmap The new header map to update with.
     */
    template <typename T>
    void update(const std::map<std::string, T>& newmap) {
        for (const auto& param : newmap) {
            set(param.first, param.second);
        }
        update_internal();
    }

    template <class BinaryStream>
    void tofile(BinaryStream& stream) {
        write_string(stream, "HEADER_START");
        for (const auto& param : sigproc_keys) {
            switch (param.second) {
                case key_type::sInt:
                    write_value<int>(stream, param.first,
                                     get<int>(param.first));
                    break;

                case key_type::sDouble:
                    write_value<double>(stream, param.first,
                                        get<double>(param.first));
                    break;

                case key_type::sBool:
                    write_value<bool>(stream, param.first,
                                      get<bool>(param.first));
                    break;

                case key_type::sString:
                    write_string(stream, param.first);
                    write_string(stream, get<std::string>(param.first));
                    break;
            }
        }
        write_string(stream, "HEADER_END");
    }

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
    template <class BinaryStream>
    bool fromfile(BinaryStream& stream) {
        int header_size, data_size, file_size;
        std::string token;
        token = read_string(stream);
        if (token != "HEADER_START") {
            stream.seekg(0, std::ios::beg);
            return false;
        }

        while (true) {
            token = read_string(stream);
            if (token == "HEADER_END") {
                header_size = stream.tellg();
                break;
            }
            if (utils::exists_key(sigproc_keys, token)) {
                key_type type = sigproc_keys.at(token);
                switch (type) {
                    case key_type::sInt:
                        set(token, read_value<int>(stream));
                        break;

                    case key_type::sDouble:
                        set(token, read_value<double>(stream));
                        break;

                    case key_type::sBool:
                        set(token, read_value<bool>(stream));
                        break;

                    case key_type::sString:
                        set(token, read_string(stream));
                        break;
                }
            } else {
                fmt::print(stderr,
                           "Warning: read_header: unknown parameter {}\n",
                           token);
            }
        }

        stream.seekg(0, std::ios::end);
        file_size = stream.tellg();
        data_size = file_size - header_size;
        if (0 == get<int>("nsamples")) {
            // Compute the number of samples from the file size
            int nsamples = data_size / get<int>("nchans") / get<int>("nifs") * 8
                           / get<int>("nbits");
            set("nsamples", nsamples);
            // Seek back to the end of the header
            stream.seekg(header_size, std::ios::beg);
        }
        set("header_size", header_size);
        set("data_size", data_size);
        set("file_size", file_size);
        update_internal();
        return true;
    }

private:
    std::map<std::string, sighdr_types> data;

    void update_internal() {
        set("telescope", get_telescope_name(get<int>("telescope_id")));
        set("backend", get_backend_name(get<int>("machine_id")));
        set("datatype", get_datatype(get<int>("data_type")));

        std::string frame;
        frame = get<bool>("pulsarcentric") ? "pulsarcentric" : "topocentric";
        frame = get<bool>("barycentric") ? "barycentric" : "topocentric";
        set("frame", frame);

        double tobs      = get<double>("tsamp") * get<int>("nsamples");
        double bandwidth = std::abs(get<double>("foff")) * get<int>("nchans");
        double ftop      = get<double>("fch1") - 0.5 * get<double>("foff");
        double fbottom   = ftop + get<double>("foff") * get<int>("nchans");
        double fcenter = ftop + 0.5 * get<double>("foff") * get<int>("nchans");
        set("tobs", tobs);
        set("bandwidth", bandwidth);
        set("ftop", ftop);
        set("fbottom", fbottom);
        set("fcenter", fcenter);

        set("ra", angles::radec_to_str(get<double>("src_raj")));
        set("dec", angles::radec_to_str(get<double>("src_dej")));
        set("ra_rad", angles::ra_to_rad(get<std::string>("ra")));
        set("dec_rad", angles::dec_to_rad(get<std::string>("dec")));
        set("obs_date", dates::MJD_to_Gregorian(get<double>("tstart")));
    }

    // Read int/long/float/double/unsigned char value
    template <class DataType, class BinaryStream>
    DataType read_value(BinaryStream& stream) {
        DataType value;
        stream.read((char*)&value, sizeof(DataType));
        return value;
    }

    // Read string value
    template <class BinaryStream>
    std::string read_string(BinaryStream& stream) {
        int len;
        char c_str[80];
        std::string str;
        stream.read((char*)&len, sizeof(int));
        stream.read(c_str, len * sizeof(char));
        c_str[len] = '\0';
        str        = c_str;
        return str;
    }

    /**
     * @brief Write a string in sigproc format.
     *
     * Write a string to a binary stream in sigrpoc header format.
     *
     * @tparam String
     * @tparam BinaryStream
     * @param stream  A binary stream to write data.
     * @param str     The string to be written.
     * @param swapout perform byte swapping on output data.
     */
    template <class String, class BinaryStream>
    void write_string(BinaryStream& stream, const String& str) {
        std::string s = str;
        int len       = s.size();
        stream.write((char*)&len, sizeof(int));
        stream.write(s.c_str(), len * sizeof(char));
    }

    // Write int/long/float/double/unsigned char value
    /**
     * @brief Write key value in sigproc format.
     *
     * Write a header key with value to a binary stream in sigproc header
     * format.
     *
     * @tparam String
     * @tparam BinaryStream
     * @tparam DataType
     * @param stream  A binary stream to write data.
     * @param name    Keyword name for value being written.
     * @param val     The value to be written.
     */
    template <class DataType, class String, class BinaryStream>
    void write_value(BinaryStream& stream, String name, DataType val) {
        write_string(stream, name);
        stream.write((char*)&val, sizeof(DataType));
    }
};
