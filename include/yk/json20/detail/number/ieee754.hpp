#ifndef YK_ZZ_JSON20_DETAIL_NUMBER_IEEE754_HPP
#define YK_ZZ_JSON20_DETAIL_NUMBER_IEEE754_HPP

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace yk::json20::detail::number {

// The IEEE 754 binary interchange formats json20 converts itself.
enum class float_format { binary32, binary64 };

// A binary format is fixed by its width and precision (significand bits,
// the implicit one included); the rest of its layout follows.
template<int Width, int Precision, class Carrier>
  requires (std::numeric_limits<Carrier>::digits == Width)
struct binary_layout {
  using carrier_uint = Carrier;
  static constexpr int significand_bits = Precision - 1;
  static constexpr int exponent_bits = Width - Precision;
  static constexpr int emax = (1 << (exponent_bits - 1)) - 1;
  static constexpr int exponent_bias = -emax;
  static constexpr int min_exponent = 1 - emax;
  static constexpr int max_exponent = emax;
  // the biased exponent of infinity and NaN
  static constexpr int infinite_power = (1 << exponent_bits) - 1;
};

template<float_format Format>
struct format_traits;

template<>
struct format_traits<float_format::binary32> : binary_layout<32, 24, std::uint32_t> {
  // Eisel-Lemire
  static constexpr int smallest_power_of_ten = -64;
  static constexpr int largest_power_of_ten = 38;
  static constexpr int min_exponent_round_to_even = -17;
  static constexpr int max_exponent_round_to_even = 10;
  // digits beyond this many never change the rounding, only whether the
  // value is above the digits kept
  static constexpr std::size_t max_digits = 114;
};

template<>
struct format_traits<float_format::binary64> : binary_layout<64, 53, std::uint64_t> {
  static constexpr int smallest_power_of_ten = -342;
  static constexpr int largest_power_of_ten = 308;
  static constexpr int min_exponent_round_to_even = -4;
  static constexpr int max_exponent_round_to_even = 23;
  static constexpr std::size_t max_digits = 769;
};

// Whether T is stored in the format. C++ fixes only the types, not their
// formats: IBM's hexadecimal floating point keeps a 64-bit double, so the size
// alone does not tell. numeric_limits counts exponents for a significand in
// [0.5, 1), one above IEEE's [1, 2).
template<class T, float_format Format>
concept has_format =
    std::floating_point<T> && std::numeric_limits<T>::is_iec559 && std::numeric_limits<T>::radix == 2
    && std::numeric_limits<T>::digits == format_traits<Format>::significand_bits + 1
    && std::numeric_limits<T>::min_exponent == format_traits<Format>::min_exponent + 1
    && std::numeric_limits<T>::max_exponent == format_traits<Format>::max_exponent + 1 && sizeof(T) == sizeof(typename format_traits<Format>::carrier_uint);

// The format of T, by what T is rather than by its name: float and double,
// and also std::float32_t / std::float64_t, or a long double that is
// binary64. No `value` for a type in no format above.
template<class T>
struct format_of {};

template<class T>
  requires has_format<T, float_format::binary32>
struct format_of<T> {
  static constexpr float_format value = float_format::binary32;
};

template<class T>
  requires has_format<T, float_format::binary64>
struct format_of<T> {
  static constexpr float_format value = float_format::binary64;
};

// A type in one of the formats above, which the conversions are written for.
template<class Float>
concept ieee754_binary = requires { format_of<Float>::value; };

template<ieee754_binary Float>
using traits_of = format_traits<format_of<Float>::value>;

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

}  // namespace yk::json20::detail::number

#endif  // YK_ZZ_JSON20_DETAIL_NUMBER_IEEE754_HPP
