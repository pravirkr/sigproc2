#pragma once

#include <string>
#include <map>
#include <boost/variant.hpp>

using sighdr_types = boost::variant<int, double, bool, std::string>;
enum class key_type { sInt, sDouble, sBool, sString };

std::map<std::string, key_type> sigproc_keys = {
    {"rawdatafile", key_type::sString}, /**< Name of the original data file. */
    {"source_name", key_type::sString}, /**< Source name. */
    {"machine_id", key_type::sInt},     /**< Sigproc backend ID. */
    {"telescope_id", key_type::sInt},   /**< Sigproc telescope ID. */
    {"data_type", key_type::sInt},      /**< Sigproc data type ID. */
    {"nchans", key_type::sInt},         /**< Number of frequency channels. */
    {"nbits", key_type::sInt},          /**< Number of bits per time sample. */
    {"nifs", key_type::sInt},           /**< Number of seperate IF channels. */
    {"nbeams", key_type::sInt},         /**< Number of beams. */
    {"ibeam", key_type::sInt},          /**< Beam number. */
    {"nsamples", key_type::sInt},       /**< Number of time samples. */
    {"npuls", key_type::sInt},          /**<   */
    {"nbins", key_type::sInt},          /**<   */
    {"barycentric", key_type::sBool},   /**< If data are barycentric. */
    {"pulsarcentric", key_type::sBool}, /**< If data are pulsarcentric. */
    {"signed", key_type::sBool},        /**< If data are signed */
    {"tstart", key_type::sDouble}, /**< Time stamp (MJD) of first sample  */
    {"tsamp", key_type::sDouble},  /**< Time interval between samples (s). */
    {"fch1",
     key_type::sDouble}, /**< Centre frequency of first channel (MHz). */
    {"foff", key_type::sDouble},     /**< Channel bandwith (MHz). */
    {"refdm", key_type::sDouble},    /**< Reference dispersion measure. */
    {"az_start", key_type::sDouble}, /**< Telescope Azimuth angle (deg). */
    {"za_start", key_type::sDouble}, /**< Telescope Zenith angle (deg). */
    {"src_raj", key_type::sDouble},  /**< Right ascension (J2000 hhmmss.ss). */
    {"src_dej", key_type::sDouble},  /**< Declination (J2000 ddmmss.ss). */
    {"period", key_type::sDouble}};  /**< Folding period (s). */

std::map<std::string, key_type> extra_keys
    = {{"telescope", key_type::sString}, /**< Telescope name. */
       {"backend", key_type::sString},   /**< Backend name. */
       {"datatype", key_type::sString},  /**< Data type. */
       {"frame", key_type::sString},     /**< pulsar/bary/topocentric. */
       {"tobs", key_type::sDouble},      /**< Obs. length (s). */
       {"bandwidth", key_type::sDouble}, /**< Frequency bandwidth (MHz). */
       {"ftop", key_type::sDouble},      /**< . */
       {"fbottom", key_type::sDouble},   /**< . */
       {"fcenter", key_type::sDouble},   /**< Centre frequency. */
       {"ra", key_type::sString},      /**< Right ascension ('hh:mm:ss.sss'). */
       {"dec", key_type::sString},     /**< Declination ('dd:mm:ss.sss') */
       {"ra_rad", key_type::sDouble},  /**< Right ascension (radian). */
       {"dec_rad", key_type::sDouble}, /**< Declination (radian)). */
       {"obs_date", key_type::sString}, /**< Gregorian date (YYYY-MM-DD). */
       {"header_size", key_type::sInt}, /**< Header size in bytes. */
       {"data_size", key_type::sInt},   /**< Data size in bytes. */
       {"file_size", key_type::sInt}};  /**< File size in bytes. */

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

std::map<std::string, std::string> keys_helpstr
    = {{"rawdatafile", "original data file name"},
       {"source_name", "source name"},
       {"machine_id", "sigproc backend ID"},
       {"backend", "datataking machine name"},
       {"telescope_id", "sigproc telescope ID"},
       {"telescope", "telescope name"},
       {"data_type", "sigproc data type ID"},
       {"datatype", "data type"},
       {"nchans", "number of frequency channels"},
       {"nbits", "number of bits per time sample"},
       {"nifs", "number of IF channels"},
       {"nbeams", "numbe rof beams"},
       {"ibeam", "beam number"},
       {"nsamples", "number of time samples"},
       {"npuls", "telescope name"},
       {"nbins", "telescope name"},
       {"barycentric", "if data are barycentric"},
       {"pulsarcentric", "if data are pulsarcentric"},
       {"frame", "pulsar/bary/topocentric"},
       {"signed", "if data are signed"},
       {"tstart", "time stamp of first sample (MJD)"},
       {"tsamp", "sample time (us)"},
       {"tobs", "length of observation (s)"},
       {"fch1", "frequency of channel 1 in MHz"},
       {"foff", "channel bandwidth in MHz"},
       {"ftop", "channel bandwidth in MHz"},
       {"fbottom", "channel bandwidth in MHz"},
       {"fcenter", "channel bandwidth in MHz"},
       {"refdm", "reference dispersion measure"},
       {"az_start", "telescope Azimuth angle (deg)"},
       {"za_start", "telescope Zenith angle (deg)"},
       {"src_raj", "right ascension (J2000 hhmmss.ss)"},
       {"src_dej", "declination (J2000 ddmmss.ss)"},
       {"ra", "right ascension ('hh:mm:ss.sss')"},
       {"dec", "declination ('dd:mm:ss.sss')"},
       {"ra_rad", "right ascension (radian)"},
       {"dec_rad", "declination (radian)"},
       {"period", "folding period (s)"},
       {"obs_date", "gregorian date (YYYY-MM-DD)"},
       {"headersize", "header size in bytes"},
       {"datasize", "data size in bytes if known"}};

std::map<int, float> nbits_to_sigmas
    = {{1, 0.5}, {2, 1.5}, {4, 6}, {8, 6}, {16, 6}, {32, 6}};
