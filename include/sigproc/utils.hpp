#pragma once

#include <map>
#include <string>
#include <boost/type_index.hpp>

namespace utils {

/**
 * @brief Check if the given key exists in the given map
 *
 * @tparam TValue  The data type of the map keys.
 * @tparam Ts      The map parameter types.
 * @param smap     The map to check the key with.
 * @param key      The key to check if it exists.
 * @return true    if the key exists.
 * @return false   if the key is not present in the given map.
 */
template <typename TValue, typename... Ts>
bool exists_key(const std::map<Ts...>& smap, const TValue& key) {
    return smap.find(key) != smap.end();
}

template <typename T>
std::string printNameOfType() {
    return boost::typeindex::type_id<T>().pretty_name();
}


}  // namespace utils
