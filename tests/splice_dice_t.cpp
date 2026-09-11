#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <sigproc/kernels.hpp>

TEST_CASE("concat_channels joins spectra in file order") {
    const std::vector<float> a = {1.F, 2.F, 3.F, 4.F, 5.F, 6.F, 7.F, 8.F};
    const std::vector<float> b = {10.F, 11.F, 12.F, 13.F,
                                  14.F, 15.F, 16.F, 17.F};
    const float* ptrs[]        = {a.data(), b.data()};
    const int nchans[]         = {4, 4};
    std::vector<float> out(16, -1.F);
    sigproc::kernels::concat_channels(std::span<const float* const>(ptrs, 2),
                                      nchans, out, 1, 2);
    REQUIRE(out == std::vector<float>{1.F, 2.F, 3.F, 4.F, 10.F, 11.F, 12.F,
                                      13.F, 5.F, 6.F, 7.F, 8.F, 14.F, 15.F,
                                      16.F, 17.F});
}

TEST_CASE("concat_channels concatenates within each IF") {
    // Two files, nifs=2, nchans=2, nsamps=1: [if][chan]
    const std::vector<float> a = {1.F, 2.F, 3.F, 4.F}; // if0: 1,2  if1: 3,4
    const std::vector<float> b = {9.F, 8.F, 7.F, 6.F};
    const float* ptrs[]        = {a.data(), b.data()};
    const int nchans[]         = {2, 2};
    std::vector<float> out(8, 0.F);
    sigproc::kernels::concat_channels(std::span<const float* const>(ptrs, 2),
                                      nchans, out, 2, 1);
    REQUIRE(out == std::vector<float>{1.F, 2.F, 9.F, 8.F, 3.F, 4.F, 7.F, 6.F});
}

TEST_CASE("dice_channels force-zeros dropped channels") {
    const std::vector<float> in = {1.F, 2.F, 3.F, 4.F, 5.F, 6.F, 7.F, 8.F};
    const std::vector<int> keep = {1, 0, 1, 0};
    std::vector<float> out(8, -1.F);
    sigproc::kernels::dice_channels(in, out, keep, 4, 1, 2, false);
    REQUIRE(out == std::vector<float>{1.F, 0.F, 3.F, 0.F, 5.F, 0.F, 7.F, 0.F});
}

TEST_CASE("dice_channels collapse keeps channels in index order") {
    const std::vector<float> in = {1.F, 2.F, 3.F, 4.F};
    const std::vector<int> keep = {1, 0, 1, 0};
    std::vector<float> out(2, -1.F);
    sigproc::kernels::dice_channels(in, out, keep, 4, 1, 1, true);
    REQUIRE(out == std::vector<float>{1.F, 3.F});
}

TEST_CASE("dice_channels collapse with empty keep throws") {
    const std::vector<float> in = {1.F, 2.F};
    const std::vector<int> keep = {0, 0};
    std::vector<float> out(2, 0.F);
    REQUIRE_THROWS_AS(
        sigproc::kernels::dice_channels(in, out, keep, 2, 1, 1, true),
        std::invalid_argument);
}
