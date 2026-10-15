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

template<class Float>
std::string std_to_chars(Float x)
{
  char buf[64];
  auto const r = std::to_chars(buf, buf + sizeof(buf), x);
  return std::string(buf, r.ptr);
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

TEST_CASE("infinity and NaN are rejected", "[number]")
{
  // JSON has no representation for them (RFC 8259)
  using yk::json20::json;
  CHECK_THROWS_AS(json(std::numeric_limits<double>::infinity()), std::invalid_argument);
  CHECK_THROWS_AS(json(-std::numeric_limits<float>::infinity()), std::invalid_argument);
  CHECK_THROWS_AS(json(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
  CHECK_THROWS_AS(json(-std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
  CHECK(json(std::numeric_limits<double>::max()).as_floating_point<double>() == std::numeric_limits<double>::max());
  CHECK(json(std::numeric_limits<double>::denorm_min()).as_floating_point<double>() == std::numeric_limits<double>::denorm_min());

  // a failed assignment leaves the target unchanged
  json a(1.5);
  CHECK_THROWS_AS(a = std::numeric_limits<double>::infinity(), std::invalid_argument);
  CHECK(a.as_floating_point<double>() == 1.5);

  // long double the same (checked as text: libc++ has no std::from_chars for it)
  CHECK(json(2.5L).as_string_unchecked() == "2.5");
  CHECK(json(std::numeric_limits<long double>::max()).as_string_unchecked() == std_to_chars(std::numeric_limits<long double>::max()));
  CHECK(json(-std::numeric_limits<long double>::max()).as_string_unchecked() == std_to_chars(-std::numeric_limits<long double>::max()));
  CHECK_THROWS_AS(json(std::numeric_limits<long double>::infinity()), std::invalid_argument);
  CHECK_THROWS_AS(json(-std::numeric_limits<long double>::infinity()), std::invalid_argument);
  CHECK_THROWS_AS(json(std::numeric_limits<long double>::quiet_NaN()), std::invalid_argument);
}
