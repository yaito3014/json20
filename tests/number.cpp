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

template<class Float>
std::string std_to_chars(Float x, std::chars_format fmt, int precision)
{
  char buf[1200];
  auto const r = std::to_chars(buf, buf + sizeof(buf), x, fmt, precision);
  return std::string(buf, r.ptr);
}

// the outcome of parsing: the bit pattern, or out of range
template<class Float>
struct parsed {
  bool ok;
  typename number::ieee754<Float>::carrier_uint bits;

  friend bool operator==(parsed, parsed) = default;
};

template<class Float>
parsed<Float> std_parse(std::string_view s)
{
  Float value{};
  auto const r = std::from_chars(s.data(), s.data() + s.size(), value);
  if (r.ec != std::errc{}) return {false, 0};
  REQUIRE(r.ptr == s.data() + s.size());
  return {true, std::bit_cast<typename number::ieee754<Float>::carrier_uint>(value)};
}

template<class Float>
parsed<Float> our_parse(std::string_view s)
{
  try {
    auto const [value, consumed] = number::parse_float<Float>(s);
    REQUIRE(consumed == s.size());
    return {true, std::bit_cast<typename number::ieee754<Float>::carrier_uint>(value)};
  } catch (std::invalid_argument const&) {
    return {false, 0};
  }
}

template<class Float>
Float random_finite(std::mt19937_64& rng)
{
  using carrier_uint = typename number::ieee754<Float>::carrier_uint;
  while (true) {
    auto const x = std::bit_cast<Float>(carrier_uint(rng()));
    if (std::isfinite(x)) return x;
  }
}

// exact decimal digits of an arbitrary-precision integer kept in base 10^9
struct decimal_bigint {
  std::vector<std::uint32_t> limbs;  // little-endian, base 10^9

  explicit decimal_bigint(std::uint64_t v)
  {
    do {
      limbs.push_back(std::uint32_t(v % 1000000000));
      v /= 1000000000;
    } while (v != 0);
  }

  void mul(std::uint32_t m)
  {
    std::uint64_t carry = 0;
    for (auto& l : limbs) {
      std::uint64_t const v = std::uint64_t(l) * m + carry;
      l = std::uint32_t(v % 1000000000);
      carry = v / 1000000000;
    }
    while (carry != 0) {
      limbs.push_back(std::uint32_t(carry % 1000000000));
      carry /= 1000000000;
    }
  }

  std::string digits() const
  {
    std::string s = std::to_string(limbs.back());
    for (std::size_t i = limbs.size() - 1; i-- > 0;) {
      std::string part = std::to_string(limbs[i]);
      s += std::string(9 - part.size(), '0') + part;
    }
    return s;
  }
};

// The exact midpoint between `x` and the next double up, as digits and a
// decimal exponent: value = digits * 10^exponent.
std::pair<std::string, int> midpoint_above(double x)
{
  auto const bits = std::bit_cast<std::uint64_t>(x);
  std::uint64_t m = bits & ((std::uint64_t(1) << 52) - 1);
  int e = -1074;
  if (int const biased = int(bits >> 52); biased != 0) {
    m |= std::uint64_t(1) << 52;
    e += biased - 1;
  }
  // (2m + 1) * 2^(e - 1)
  decimal_bigint v(2 * m + 1);
  int twos = e - 1;
  int exponent = 0;
  for (; twos > 0; --twos) v.mul(2);
  for (; twos < 0; ++twos) {
    v.mul(5);  // 2^-1 = 5 * 10^-1
    --exponent;
  }
  return {v.digits(), exponent};
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

TEST_CASE("float parsing: constant evaluation", "[number]")
{
  static_assert(number::parse_float<double, char>("1.5").first == 1.5);
  static_assert(number::parse_float<double, char>("-0.1").first == -0.1);
  static_assert(number::parse_float<double, char>("1e308").first == 1e308);
  static_assert(number::parse_float<double, char>("4.9406564584124654e-324").first == 4.9406564584124654e-324);
  static_assert(number::parse_float<float, char>("3.4028235e38").first == 3.4028235e38f);
  static_assert(number::parse_float<double, char16_t>(u"2.5e-3").first == 2.5e-3);
  // more than 19 significant digits, decided by exact comparison
  static_assert(number::parse_float<double, char>("9007199254740993.00000000000000000001").first == 9007199254740994.0);
  // stops at the first character that cannot continue the number
  static_assert(number::parse_float<double, char>("12.5,").second == 4);
  static_assert(number::parse_float<double, char>("1.e5").second == 1);
}

TEST_CASE("float parsing: edge cases", "[number]")
{
  char const* const cases[] = {
      "0", "-0", "0.0", "0e999", "1", "-1", "0.1", "0.3", "1e23", "8.98846567431158e307",
      "1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308", "1e309", "-1e400",
      "2.2250738585072011e-308", "2.2250738585072014e-308", "4.9406564584124654e-324", "5e-324", "3e-324",
      "2.4703282292062328e-324", "2.4703282292062327e-324", "1e-400", "9007199254740992", "9007199254740993",
      "9007199254740995", "9007199254740993.0000000000000000000000000000000001",
      "123456789012345678901234567890", "0.000000000000000000000000000000000000000000001",
      "1.00000000000000011102230246251565404236316680908203125",
      "1.00000000000000011102230246251565404236316680908203124",
      "1.00000000000000011102230246251565404236316680908203126",
      "7.2057594037927933e16", "9223372036854775807", "18446744073709551615", "18446744073709551616"};
  for (std::string_view s : cases) {
    CAPTURE(s);
    CHECK(our_parse<double>(s) == std_parse<double>(s));
    CHECK(our_parse<float>(s) == std_parse<float>(s));
  }
  CHECK_THROWS_AS(number::parse_float<double>(std::string_view("-")), std::invalid_argument);
  CHECK_THROWS_AS(number::parse_float<double>(std::string_view(".5")), std::invalid_argument);
}

TEST_CASE("float parsing: random values round-trip", "[number]")
{
  std::mt19937_64 rng(20261015);
  for (int i = 0; i < 200000; ++i) {
    double const x = random_finite<double>(rng);
    std::string const shortest = std_to_chars(x);
    std::string const precise = std_to_chars(x, std::chars_format::scientific, 17);
    CAPTURE(shortest, precise);
    REQUIRE(our_parse<double>(shortest) == std_parse<double>(shortest));
    REQUIRE(our_parse<double>(precise) == std_parse<double>(precise));
  }
  for (int i = 0; i < 200000; ++i) {
    float const x = random_finite<float>(rng);
    std::string const shortest = std_to_chars(x);
    std::string const precise = std_to_chars(x, std::chars_format::scientific, 9);
    CAPTURE(shortest, precise);
    REQUIRE(our_parse<float>(shortest) == std_parse<float>(shortest));
    REQUIRE(our_parse<float>(precise) == std_parse<float>(precise));
  }
}

TEST_CASE("float parsing: random decimal strings", "[number]")
{
  std::mt19937_64 rng(42);
  auto const pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
  for (int i = 0; i < 200000; ++i) {
    std::string s;
    if (pick(0, 1)) s += '-';
    int const integer_digits = pick(1, 30);
    s += char('1' + pick(0, 8));
    for (int j = 1; j < integer_digits; ++j) s += char('0' + pick(0, 9));
    if (pick(0, 1)) {
      s += '.';
      int const fraction_digits = pick(1, 30);
      for (int j = 0; j < fraction_digits; ++j) s += char('0' + pick(0, 9));
    }
    if (pick(0, 3) != 0) s += 'e' + std::to_string(pick(-360, 330));
    CAPTURE(s);
    REQUIRE(our_parse<double>(s) == std_parse<double>(s));
    REQUIRE(our_parse<float>(s) == std_parse<float>(s));
  }
}

TEST_CASE("float parsing: exact midpoints between doubles", "[number]")
{
  // These need every digit: the leading 19 digits w and w + 1 round to
  // different doubles, so the exact comparison decides.
  std::mt19937_64 rng(7);
  std::vector<double> xs = {0.0, 4.9406564584124654e-324, 2.2250738585072009e-308, 2.2250738585072014e-308, 1.0, 9007199254740992.0, 1.7976931348623155e308};
  for (int i = 0; i < 2000; ++i) xs.push_back(std::abs(random_finite<double>(rng)));
  for (double const x : xs) {
    if (x == std::numeric_limits<double>::max()) continue;
    auto const [digits, exponent] = midpoint_above(x);
    std::string const exact = digits + "e" + std::to_string(exponent);
    // the midpoint always ends in 5 (or is an even integer for large x);
    // nudge it just above and just below
    std::string const above = digits + "000000000000000000001e" + std::to_string(exponent - 21);
    std::string below = digits;
    std::size_t k = below.size();
    while (k > 0 && below[k - 1] == '0') --k;
    below[k - 1] = char(below[k - 1] - 1);
    below += "99999999999999999999";
    below += "e" + std::to_string(exponent - 20 - int(digits.size() - k));
    for (std::string const& s : {exact, above, below}) {
      CAPTURE(x, s.size(), s.substr(0, 40));
      REQUIRE(our_parse<double>(s) == std_parse<double>(s));
    }
  }
}

TEST_CASE("floating-point values in the DOM", "[number]")
{
  using yk::json20::json;
  CHECK(json(1e300).as_floating_point<double>() == 1e300);
  CHECK(json(0.1f).as_floating_point<float>() == 0.1f);
  CHECK(json(-0.0).as_string_unchecked() == "-0");
  CHECK(std::signbit(json(-0.0).as_floating_point<double>()));
  CHECK(yk::json20::u16json_parser::parse(u"[2.5e-3]").at(0).as_floating_point<double>() == 2.5e-3);

  // out of range values cannot be read
  CHECK_THROWS_AS(yk::json20::json_parser::parse("1e400").as_floating_point<double>(), std::invalid_argument);
  CHECK_FALSE(yk::json20::json_parser::parse("1e-400").try_as_floating_point<double>().has_value());
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
