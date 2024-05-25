#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include <boost/type_index.hpp>
#include <fmt/format.h>

#include <sigproc/params.hpp>

namespace map_utils {

// Get the value of a key in a basic map
template <typename K, typename V>
V get_value(const std::unordered_map<K, V>& smap, const K& key) {
    if (!smap.contains(key)) {
        throw std::runtime_error(fmt::format("Key {} not found in map {}", key,
                                             printNameOfType<K>()));
    }
    return smap.at(key);
}

// Get the value of a key in a map of variants
template <typename K, typename T>
T get_value_variant(const std::unordered_map<K, SighdrTypes>& smap,
                    const K& key) {
    if (!smap.contains(key)) {
        throw std::runtime_error(fmt::format("Key {} not found in map {}", key,
                                             printNameOfType<K>()));
    }
    const auto& variant = smap.at(key);
    if (!std::holds_alternative<T>(variant)) {
        throw std::runtime_error(
            fmt::format("Key {} in map {} is not of expected type {}.", key,
                        printNameOfType<K>(), printNameOfType<T>()));
    }
    return std::get<T>(variant);
}

template <typename T> std::string print_name_of_type() {
    return boost::typeindex::type_id<T>().pretty_name();
}

} // namespace map_utils
