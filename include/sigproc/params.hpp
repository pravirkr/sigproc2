#pragma once

#include <climits>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

constexpr double kDMConst = 4.148808e3; // MHz^2 cm^3 pc^-1 s

using SighdrTypes = std::variant<int, double, bool, std::string>;
enum class KeyType { kSInt, kSDouble, kSBool, kSString };

struct KeyInfo {
    KeyType type;
    std::string helpstr;
};

const std::unordered_map<KeyType, SighdrTypes> kDefaultKeyValues = {
    {KeyType::kSInt, 0},
    {KeyType::kSDouble, 0.0},
    {KeyType::kSBool, false},
    {KeyType::kSString, std::string()}};

const std::unordered_map<std::string, KeyInfo> kSigprocKeys = {
    {"rawdatafile", {KeyType::kSString, "original data file name"}},
    {"source_name", {KeyType::kSString, "source name"}},
    {"machine_id", {KeyType::kSInt, "sigproc backend ID"}},
    {"telescope_id", {KeyType::kSInt, "sigproc telescope ID"}},
    {"data_type", {KeyType::kSInt, "sigproc data type ID"}},
    {"nchans", {KeyType::kSInt, "number of frequency channels"}},
    {"nbits", {KeyType::kSInt, "number of bits per time sample"}},
    {"nifs", {KeyType::kSInt, "number of seperate IF channels"}},
    {"nbeams", {KeyType::kSInt, "number of beams"}},
    {"ibeam", {KeyType::kSInt, "beam number"}},
    {"nsamples", {KeyType::kSInt, "number of time samples"}},
    {"npuls", {KeyType::kSInt, " "}},
    {"nbins", {KeyType::kSInt, " "}},
    {"barycentric", {KeyType::kSBool, "if data are barycentric"}},
    {"pulsarcentric", {KeyType::kSBool, "if data are pulsarcentric"}},
    {"signed", {KeyType::kSBool, "if data are signed"}},
    {"tstart", {KeyType::kSDouble, "time stamp of first sample (MJD)"}},
    {"tsamp", {KeyType::kSDouble, "sample time (us)"}},
    {"fch1", {KeyType::kSDouble, "frequency of channel 1 in MHz"}},
    {"foff", {KeyType::kSDouble, "channel bandwidth in MHz"}},
    {"refdm", {KeyType::kSDouble, "reference dispersion measure"}},
    {"az_start", {KeyType::kSDouble, "telescope Azimuth angle (deg)"}},
    {"za_start", {KeyType::kSDouble, "telescope Zenith angle (deg)"}},
    {"src_raj", {KeyType::kSDouble, "right ascension (J2000 hhmmss.ss)"}},
    {"src_dej", {KeyType::kSDouble, "declination (J2000 ddmmss.ss)"}},
    {"period", {KeyType::kSDouble, "folding period (s)"}}};

const std::unordered_map<std::string, KeyInfo> kExtraKeys = {
    {"telescope", {KeyType::kSString, "Telescope name."}},
    {"backend", {KeyType::kSString, "Backend name."}},
    {"datatype", {KeyType::kSString, "Data type."}},
    {"frame", {KeyType::kSString, "pulsar/bary/topocentric."}},
    {"tobs", {KeyType::kSDouble, "Obs. length (s)."}},
    {"tobs_str", {KeyType::kSString, "Obs. length (readable)."}},
    {"bandwidth", {KeyType::kSDouble, "Frequency bandwidth (MHz)."}},
    {"ftop", {KeyType::kSDouble, " "}},
    {"fbottom", {KeyType::kSDouble, " "}},
    {"fcenter", {KeyType::kSDouble, "Centre frequency."}},
    {"ra", {KeyType::kSString, "Right ascension ('hh:mm:ss.sss')."}},
    {"dec", {KeyType::kSString, "Declination ('dd:mm:ss.sss')"}},
    {"ra_rad", {KeyType::kSDouble, "Right ascension (radian)."}},
    {"dec_rad", {KeyType::kSDouble, "Declination (radian))."}},
    {"obs_date", {KeyType::kSString, "Gregorian date (YYYY-MM-DD)."}},
    {"header_size", {KeyType::kSInt, "Header size in bytes."}},
    {"data_size", {KeyType::kSInt, "Data size in bytes."}},
    {"file_size", {KeyType::kSInt, "File size in bytes."}}};

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

class BitsInfo {
public:
    explicit BitsInfo(int nbits) : m_nbits(nbits) {
        if (!m_attributes.contains(nbits)) {
            throw std::invalid_argument(
                std::format("nbits = {} not supported.", nbits));
        }
    }
    size_t itemsize() const { return m_attributes.at(m_nbits).itemsize; }

    bool packunpack() const {
        return m_nbits == 1 || m_nbits == 2 || m_nbits == 4;
    }
    size_t bitfact() const { return packunpack() ? CHAR_BIT / m_nbits : 1; }
    size_t digi_min() const { return m_nbits == 32 ? 0 : 0; }
    size_t digi_max() const { return (1 << m_nbits) - 1; }
    float digi_mean() const {
        return static_cast<float>((1 << (m_nbits - 1)) - 0.5);
    }
    float digi_scale() const {
        return digi_mean() / m_attributes.at(m_nbits).digi_sigma;
    }

private:
    size_t m_nbits;

    struct Attribute {
        size_t itemsize;
        float digi_sigma;
    };

    std::unordered_map<size_t, Attribute> m_attributes = {
        {1, {sizeof(uint8_t), 0.5F}},   {2, {sizeof(uint8_t), 1.5F}},
        {4, {sizeof(uint8_t), 6.0F}},   {8, {sizeof(uint8_t), 6.0F}},
        {16, {sizeof(uint16_t), 6.0F}}, {32, {sizeof(float), 6.0F}}};
};