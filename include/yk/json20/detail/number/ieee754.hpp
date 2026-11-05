#ifndef YK_ZZ_JSON20_DETAIL_NUMBER_IEEE754_HPP
#define YK_ZZ_JSON20_DETAIL_NUMBER_IEEE754_HPP

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace yk {

namespace json20 {

namespace detail::number {

template<class Float>
struct ieee754;

template<>
struct ieee754<double> {
  using carrier_uint = std::uint64_t;
  static constexpr int significand_bits = 52;
  static constexpr int exponent_bits = 11;
  static constexpr int exponent_bias = -1023;
  static constexpr int min_exponent = -1022;
  static constexpr int max_exponent = 1023;

  // Eisel-Lemire
  static constexpr int infinite_power = 0x7ff;
  static constexpr int smallest_power_of_ten = -342;
  static constexpr int largest_power_of_ten = 308;
  static constexpr int min_exponent_round_to_even = -4;
  static constexpr int max_exponent_round_to_even = 23;
  // digits beyond this many never change the rounding, only whether the
  // value is above the digits kept
  static constexpr std::size_t max_digits = 769;
};

template<>
struct ieee754<float> {
  using carrier_uint = std::uint32_t;
  static constexpr int significand_bits = 23;
  static constexpr int exponent_bits = 8;
  static constexpr int exponent_bias = -127;
  static constexpr int min_exponent = -126;
  static constexpr int max_exponent = 127;

  static constexpr int infinite_power = 0xff;
  static constexpr int smallest_power_of_ten = -64;
  static constexpr int largest_power_of_ten = 38;
  static constexpr int min_exponent_round_to_even = -17;
  static constexpr int max_exponent_round_to_even = 10;
  static constexpr std::size_t max_digits = 114;
};

// float or double in the format the conversions below are written for, which
// they are on practically every platform. Only the type is fixed by C++, not
// its format: IBM's hexadecimal floating point keeps a 64-bit double, so
// the size alone does not tell.
template<class Float>
concept ieee754_binary =
    (std::same_as<Float, float> || std::same_as<Float, double>) && std::numeric_limits<Float>::is_iec559 && std::numeric_limits<Float>::radix == 2
    && std::numeric_limits<Float>::digits == ieee754<Float>::significand_bits + 1
    && std::numeric_limits<Float>::min_exponent == ieee754<Float>::min_exponent + 1
    && std::numeric_limits<Float>::max_exponent == ieee754<Float>::max_exponent + 1 && sizeof(Float) == sizeof(typename ieee754<Float>::carrier_uint);

// The floating-point types json20 converts itself: in every character type
// and in constant expressions. The rest use the platform's std::from_chars /
// std::to_chars, char only.
template<class Float>
concept self_converted = ieee754_binary<Float>;

// A binary floating-point value as the integer bit pattern without the sign:
// biased exponent << significand_bits | stored significand
struct adjusted_mantissa {
  std::uint64_t mantissa;
  int power2;

  friend constexpr bool operator==(adjusted_mantissa, adjusted_mantissa) = default;
};

}  // namespace detail::number

}  // namespace json20

}  // namespace yk

#endif  // YK_ZZ_JSON20_DETAIL_NUMBER_IEEE754_HPP
