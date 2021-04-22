#pragma once

#include <string>
#include <map>
#include <boost/variant.hpp>
#include <fmt/format.h>

#include "utils.hpp"

using sig_hdr_types = boost::variant<int, double, bool, std::string>;
enum class sig_type { sInt, sDouble, sBool, sString };

std::map<std::string, sig_type> sigproc_keys = {
    {"rawdatafile", sig_type::sString}, /**< Name of the original data file. */
    {"source_name", sig_type::sString}, /**< Source name. */
    {"machine_id", sig_type::sInt},     /**< Sigproc backend ID. */
    {"telescope_id", sig_type::sInt},   /**< Sigproc telescope ID. */
    {"data_type", sig_type::sInt},      /**< Sigproc data type ID. */
    {"nchans", sig_type::sInt},         /**< Number of frequency channels. */
    {"nbits", sig_type::sInt},          /**< Number of bits per time sample. */
    {"nifs", sig_type::sInt},           /**< Number of seperate IF channels. */
    {"nbeams", sig_type::sInt},         /**< Number of beams. */
    {"ibeam", sig_type::sInt},          /**< Beam number. */
    {"nsamples", sig_type::sInt},       /**< Number of time samples. */
    {"npuls", sig_type::sInt},          /**<   */
    {"nbins", sig_type::sInt},          /**<   */
    {"barycentric", sig_type::sBool},   /**< If data are barycentric. */
    {"pulsarcentric", sig_type::sBool}, /**< If data are pulsarcentric. */
    {"signed", sig_type::sBool},        /**< If data are signed */
    {"tstart", sig_type::sDouble}, /**< Time stamp (MJD) of first sample  */
    {"tsamp", sig_type::sDouble},  /**< Time interval between samples (s). */
    {"fch1",
     sig_type::sDouble}, /**< Centre frequency of first channel (MHz). */
    {"foff", sig_type::sDouble},     /**< Channel bandwith (MHz). */
    {"refdm", sig_type::sDouble},    /**< Reference dispersion measure. */
    {"az_start", sig_type::sDouble}, /**< Telescope Azimuth angle (deg). */
    {"za_start", sig_type::sDouble}, /**< Telescope Zenith angle (deg). */
    {"src_raj", sig_type::sDouble},  /**< Right ascension (J2000 hhmmss.ss). */
    {"src_dej", sig_type::sDouble},  /**< Declination (J2000 ddmmss.ss). */
    {"period", sig_type::sDouble}};  /**< Folding period (s). */

std::map<std::string, sig_type> extra_keys
    = {{"telescope", sig_type::sString}, /**< Telescope name. */
       {"backend", sig_type::sString},   /**< Backend name. */
       {"datatype", sig_type::sString},  /**< Data type. */
       {"frame", sig_type::sString},     /**< pulsar/bary/topocentric. */
       {"tobs", sig_type::sDouble},      /**< Obs. length (s). */
       {"bandwidth", sig_type::sDouble}, /**< Frequency bandwidth (MHz). */
       {"ftop", sig_type::sDouble},      /**< . */
       {"fbottom", sig_type::sDouble},   /**< . */
       {"fcenter", sig_type::sDouble},   /**< Centre frequency. */
       {"ra", sig_type::sString},      /**< Right ascension ('hh:mm:ss.sss'). */
       {"dec", sig_type::sString},     /**< Declination ('dd:mm:ss.sss') */
       {"ra_rad", sig_type::sDouble},  /**< Right ascension (radian). */
       {"dec_rad", sig_type::sDouble}, /**< Declination (radian)). */
       {"obs_date", sig_type::sString}, /**< Gregorian date (YYYY-MM-DD). */
       {"header_size", sig_type::sInt}, /**< Header size in bytes. */
       {"data_size", sig_type::sInt},   /**< Data size in bytes. */
       {"file_size", sig_type::sInt}};  /**< File size in bytes. */

std::map<int, std::string> telescope_ids
    = {{0, "Fake"},       {1, "Arecibo"}, {2, "Ooty"},     {3, "Nancay"},
       {4, "Parkes"},     {5, "Jodrell"}, {6, "GBT"},      {7, "GMRT"},
       {8, "Effelsberg"}, {9, "140ft"},   {10, "SRT"},     {11, "LOFAR"},
       {64, "MeerKAT"},   {65, "KAT-7"},  {82, "eMerlin"}, {1916, "I-LOFAR"}};

std::map<int, std::string> machine_ids
    = {{0, "FAKE"},       {1, "PSPM"},     {2, "WaPP"},    {3, "AOFTM"},
       {4, "BPP"},        {5, "OOTY"},     {6, "SCAMP"},   {7, "GMRTFB"},
       {8, "PULSAR2000"}, {9, "PARSPEC"},  {10, "BPSR"},   {14, "GMRTNEW"},
       {64, "KAT"},       {65, "KAT-DC2"}, {82, "loft-e"}, {1916, "RÉALTA"}};

std::map<int, std::string> tempo_sites
    = {{0, "3"}, {1, "3"}, {3, "f"},  {4, "7"},  {5, "8"},  {6, "1"},
       {8, "g"}, {9, "a"}, {10, "z"}, {64, "m"}, {65, "k"}, {1916, "ie613"}};

std::map<int, std::string> data_types = {{0, "raw data"},
                                         {1, "filterbank"},
                                         {2, "time series"},
                                         {3, "pulse profiles"},
                                         {4, "amplitude spectrum"},
                                         {5, "complex spectrum"},
                                         {6, "dedispersed subbands"}};

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
