#pragma once

#include <string>
#include <unordered_map>

#include "sigproc/common/types.hpp"

namespace sigproc::params {

struct KeyInfo {
    KeyType type;
    std::string helpstr;
};

const std::unordered_map<KeyType, HeaderValue> kDefaultKeyValues = {
    {KeyType::kSInt, 0},
    {KeyType::kSDouble, 0.0},
    {KeyType::kSBool, false},
    {KeyType::kSString, std::string()}};

const std::unordered_map<std::string, KeyInfo> kSigprocKeys = {
    {"rawdatafile",
     {.type = KeyType::kSString, .helpstr = "original data file name"}},
    {"source_name", {.type = KeyType::kSString, .helpstr = "source name"}},
    {"machine_id", {.type = KeyType::kSInt, .helpstr = "sigproc backend ID"}},
    {"telescope_id",
     {.type = KeyType::kSInt, .helpstr = "sigproc telescope ID"}},
    {"data_type", {.type = KeyType::kSInt, .helpstr = "sigproc data type ID"}},
    {"nchans",
     {.type = KeyType::kSInt, .helpstr = "number of frequency channels"}},
    {"nbits",
     {.type = KeyType::kSInt, .helpstr = "number of bits per time sample"}},
    {"nifs",
     {.type = KeyType::kSInt, .helpstr = "number of seperate IF channels"}},
    {"nbeams", {.type = KeyType::kSInt, .helpstr = "number of beams"}},
    {"ibeam", {.type = KeyType::kSInt, .helpstr = "beam number"}},
    {"nsamples", {.type = KeyType::kSInt, .helpstr = "number of time samples"}},
    {"npuls", {.type = KeyType::kSInt, .helpstr = " "}},
    {"nbins", {.type = KeyType::kSInt, .helpstr = " "}},
    {"barycentric",
     {.type = KeyType::kSBool, .helpstr = "if data are barycentric"}},
    {"pulsarcentric",
     {.type = KeyType::kSBool, .helpstr = "if data are pulsarcentric"}},
    {"signed", {.type = KeyType::kSBool, .helpstr = "if data are signed"}},
    {"tstart",
     {.type    = KeyType::kSDouble,
      .helpstr = "time stamp of first sample (MJD)"}},
    {"tsamp", {.type = KeyType::kSDouble, .helpstr = "sample time (us)"}},
    {"fch1",
     {.type = KeyType::kSDouble, .helpstr = "frequency of channel 1 in MHz"}},
    {"foff",
     {.type = KeyType::kSDouble, .helpstr = "channel bandwidth in MHz"}},
    {"refdm",
     {.type = KeyType::kSDouble, .helpstr = "reference dispersion measure"}},
    {"az_start",
     {.type = KeyType::kSDouble, .helpstr = "telescope Azimuth angle (deg)"}},
    {"za_start",
     {.type = KeyType::kSDouble, .helpstr = "telescope Zenith angle (deg)"}},
    {"src_raj",
     {.type    = KeyType::kSDouble,
      .helpstr = "right ascension (J2000 hhmmss.ss)"}},
    {"src_dej",
     {.type = KeyType::kSDouble, .helpstr = "declination (J2000 ddmmss.ss)"}},
    {"period", {.type = KeyType::kSDouble, .helpstr = "folding period (s)"}}};

const std::unordered_map<std::string, KeyInfo> kExtraKeys = {
    {"telescope", {.type = KeyType::kSString, .helpstr = "Telescope name."}},
    {"backend", {.type = KeyType::kSString, .helpstr = "Backend name."}},
    {"datatype", {.type = KeyType::kSString, .helpstr = "Data type."}},
    {"frame",
     {.type = KeyType::kSString, .helpstr = "pulsar/bary/topocentric."}},
    {"tobs", {.type = KeyType::kSDouble, .helpstr = "Obs. length (s)."}},
    {"tobs_str",
     {.type = KeyType::kSString, .helpstr = "Obs. length (readable)."}},
    {"bandwidth",
     {.type = KeyType::kSDouble, .helpstr = "Frequency bandwidth (MHz)."}},
    {"ftop", {.type = KeyType::kSDouble, .helpstr = " "}},
    {"fbottom", {.type = KeyType::kSDouble, .helpstr = " "}},
    {"fcenter", {.type = KeyType::kSDouble, .helpstr = "Centre frequency."}},
    {"ra",
     {.type    = KeyType::kSString,
      .helpstr = "Right ascension ('hh:mm:ss.sss')."}},
    {"dec",
     {.type = KeyType::kSString, .helpstr = "Declination ('dd:mm:ss.sss')"}},
    {"ra_rad",
     {.type = KeyType::kSDouble, .helpstr = "Right ascension (radian)."}},
    {"dec_rad",
     {.type = KeyType::kSDouble, .helpstr = "Declination (radian))."}},
    {"obs_date",
     {.type = KeyType::kSString, .helpstr = "Gregorian date (YYYY-MM-DD)."}},
    {"header_size",
     {.type = KeyType::kSInt, .helpstr = "Header size in bytes."}},
    {"data_size", {.type = KeyType::kSInt, .helpstr = "Data size in bytes."}},
    {"file_size", {.type = KeyType::kSInt, .helpstr = "File size in bytes."}}};

const std::unordered_map<int, std::string> kTelescopeIds = {
    {0, "Fake"},       {1, "Arecibo"}, {2, "Ooty"},     {3, "Nancay"},
    {4, "Parkes"},     {5, "Jodrell"}, {6, "GBT"},      {7, "GMRT"},
    {8, "Effelsberg"}, {9, "140ft"},   {10, "SRT"},     {11, "LOFAR"},
    {64, "MeerKAT"},   {65, "KAT-7"},  {82, "eMerlin"}, {1916, "I-LOFAR"}};

const std::unordered_map<int, std::string> kMachineIds = {
    {0, "FAKE"},       {1, "PSPM"},     {2, "WaPP"},    {3, "AOFTM"},
    {4, "BPP"},        {5, "OOTY"},     {6, "SCAMP"},   {7, "GMRTFB"},
    {8, "PULSAR2000"}, {9, "PARSPEC"},  {10, "BPSR"},   {14, "GMRTNEW"},
    {64, "KAT"},       {65, "KAT-DC2"}, {82, "loft-e"}, {1916, "RÉALTA"}};

const std::unordered_map<int, std::string> kTempoSites = {
    {0, "3"}, {1, "3"}, {3, "f"},  {4, "7"},  {5, "8"},  {6, "1"},
    {8, "g"}, {9, "a"}, {10, "z"}, {64, "m"}, {65, "k"}, {1916, "ie613"}};

const std::unordered_map<int, std::string> kDataTypes = {
    {0, "raw data"},
    {1, "filterbank"},
    {2, "time series"},
    {3, "pulse profiles"},
    {4, "amplitude spectrum"},
    {5, "complex spectrum"},
    {6, "dedispersed subbands"}};

} // namespace sigproc::params