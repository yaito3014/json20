#include <yk/json20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace number = yk::json20::detail::number;

constexpr std::array<number::u128, number::pow5::q_max - number::pow5::q_min + 1> pow5_reference = {{
#include "pow5_reference.inc"
}};

constexpr bool pow5_table_matches(int first = number::pow5::q_min, int last = number::pow5::q_max)
{
  for (int q = first; q <= last; ++q) {
    auto const expected = pow5_reference[std::size_t(q - number::pow5::q_min)];
    auto const actual = number::pow5::floor(q);
    if (actual.hi != expected.hi || actual.lo != expected.lo) return false;
  }
  return true;
}

// One constant evaluation per block of 32 entries keeps each within MSVC's
// default step limit, which is far below GCC's and clang's.
template<int First>
constexpr bool pow5_block_matches = pow5_table_matches(First, std::min(First + 31, number::pow5::q_max));

template<std::size_t... I>
constexpr bool pow5_blocks_match(std::index_sequence<I...>)
{
  return (pow5_block_matches<number::pow5::q_min + int(I) * 32> && ...);
}

}  // namespace

TEST_CASE("power-of-five table", "[number]")
{
  static_assert(pow5_blocks_match(std::make_index_sequence<(number::pow5::q_max - number::pow5::q_min) / 32 + 1>{}));
  CHECK(pow5_table_matches());
}
