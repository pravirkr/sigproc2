#include <catch2/catch.hpp>

#include "sigproc/params.hpp"

// WRITE units tests for header.hpp

TEST_CASE("Test header.hpp") {
    // Test the function get_value
    SECTION("Test get_value") {
        // Test the function get_value with a map of int, int
        std::map<int, int> smap1 = {{1, 2}, {3, 4}, {5, 6}};
        REQUIRE(MapUtils::get_value(smap1, 1) == 2);
        REQUIRE(MapUtils::get_value(smap1, 3) == 4);
        REQUIRE(MapUtils::get_value(smap1, 5) == 6);
        // Test the function get_value with a map of int, std::string
        std::map<int, std::string> smap2 = {{1, "2"}, {3, "4"}, {5, "6"}};
        REQUIRE(MapUtils::get_value(smap2, 1) == "2");
        REQUIRE(MapUtils::get_value(smap2, 3) == "4");
        REQUIRE(MapUtils::get_value(smap2, 5) == "6");
    }
    // Test the function get_value_variant
    SECTION("Test get_value_variant") {
        // Test the function get_value_variant with a map of int, sighdr_types
        std::map<int, sighdr_types> smap1 = {{1, 2}, {3, 4}, {5, 6}};
        REQUIRE(MapUtils::get_value_variant<int, int>(smap1, 1) == 2);
        REQUIRE(MapUtils::get_value_variant<int, int>(smap1, 3) == 4);
        REQUIRE(MapUtils::get_value_variant<int, int>(smap1, 5) == 6);
        // Test the function get_value_variant with a map of int, sighdr_types
        std::map<int, sighdr_types> smap2 = {{1, "2"}, {3, "4"}, {5, "6"}};
        REQUIRE(MapUtils::get_value_variant<int, std::string>(smap2, 1) == "2");
        REQUIRE(MapUtils::get_value_variant<int, std::string>(smap2, 3) == "4");
        REQUIRE(MapUtils::get_value_variant<int, std::string>(smap2, 5) == "6");
    }
    // Test the function printNameOfType