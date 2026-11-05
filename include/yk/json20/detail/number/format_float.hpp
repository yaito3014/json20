#ifndef YK_ZZ_JSON20_DETAIL_NUMBER_FORMAT_FLOAT_HPP
#define YK_ZZ_JSON20_DETAIL_NUMBER_FORMAT_FLOAT_HPP

#include <yk/json20/detail/number/dragonbox.hpp>
#include <yk/json20/detail/number/ieee754.hpp>
#include <yk/json20/detail/select_str.hpp>

#include <bit>
#include <charconv>
#include <type_traits>

#include <cstddef>
#include <cstdint>

namespace yk::json20::detail::number {

// Binary to decimal: formatting
//
// Same output as std::to_chars(first, last, value): the shortest round-trip
// digits, in fixed or scientific notation whichever is shorter (fixed on a
// tie), with an exponent of at least two digits.

// Writes the decimal digits of `value`, two at a time, so that the last one
// lands just before `last`; returns where the first one went. The digits come
// from a literal table, so they are spelled in charT and in the execution
// character set the compiler translated the literal to.
template<class charT, class UInt>
constexpr charT* write_digits(charT* last, UInt value) noexcept
{
  constexpr auto digit_pairs = YK_JSON20_WIDEN_STRING(
      charT,
      "00010203040506070809"
      "10111213141516171819"
      "20212223242526272829"
      "30313233343536373839"
      "40414243444546474849"
      "50515253545556575859"
      "60616263646566676869"
      "70717273747576777879"
      "80818283848586878889"
      "90919293949596979899"
  );
  while (value >= 100) {
    auto const pair = static_cast<std::size_t>(value % 100) * 2;
    value /= 100;
    *--last = digit_pairs.data[pair + 1];
    *--last = digit_pairs.data[pair];
  }
  if (value >= 10) {
    auto const pair = static_cast<std::size_t>(value) * 2;
    *--last = digit_pairs.data[pair + 1];
    *--last = digit_pairs.data[pair];
  } else {
    *--last = digit_pairs.data[static_cast<std::size_t>(value) * 2 + 1];
  }
  return last;
}

// Constant evaluation goes through append: clang (18, 21) cannot always
// constant-evaluate libstdc++'s (pointer, length) constructor, reporting the
// member template it delegates to as undefined. At run time the constructor
// is faster.
template<class charT>
constexpr std::basic_string<charT> make_string(charT const* first, std::size_t length)
{
  if (std::is_constant_evaluated()) {
    std::basic_string<charT> out;
    out.append(first, length);
    return out;
  }
  return std::basic_string<charT>(first, length);
}

template<class charT, class Float>
  requires ieee754_binary<Float>
constexpr std::basic_string<charT> format_float(Float x)
{
  constexpr auto digit_chars = YK_JSON20_WIDEN_STRING(charT, "0123456789");
  constexpr auto minus = YK_JSON20_WIDEN_STRING(charT, "-");
  constexpr auto plus = YK_JSON20_WIDEN_STRING(charT, "+");
  constexpr auto dot = YK_JSON20_WIDEN_STRING(charT, ".");
  constexpr auto e = YK_JSON20_WIDEN_STRING(charT, "e");

  using carrier_uint = typename ieee754<Float>::carrier_uint;
  constexpr int significand_bits = ieee754<Float>::significand_bits;
  constexpr int sign_index = significand_bits + ieee754<Float>::exponent_bits;
  auto const bits = std::bit_cast<carrier_uint>(x);

  // built in place and copied out once: appending piecewise to the string
  // costs several times the conversion itself.
  // longest output: "-d.ddddddddddddddddde-ddd" (25), and fixed notation is
  // chosen only when it is no longer than scientific
  charT buf[32]{};
  int len = 0;
  if ((bits >> sign_index) != 0) buf[len++] = minus.data[0];
  if ((bits & ~(carrier_uint(1) << sign_index)) == 0) {
    buf[len++] = digit_chars.data[0];
    return make_string(buf, len);
  }

  // no trailing zeros: Dragonbox's to_decimal removes them
  auto const [significand, exponent] = dragonbox::to_decimal(x);
  charT digit_buf[20]{};
  charT const* const digits = write_digits(std::end(digit_buf), significand);
  int const n = int(std::end(digit_buf) - digits);

  int const scientific_exponent = exponent + n - 1;
  int const abs_exponent = scientific_exponent < 0 ? -scientific_exponent : scientific_exponent;
  int const exponent_digits = abs_exponent >= 100 ? 3 : 2;
  int const scientific_length = n + (n > 1 ? 1 : 0) + 2 + exponent_digits;
  int const fixed_length = exponent >= 0 ? n + exponent : (-exponent < n ? n + 1 : 2 - exponent);

  // the significand's digits, with a dot after the first `point` of them
  // when 0 < point < n
  auto const put_significand = [&](int point) {
    for (int i = 0; i < n; ++i) {
      if (i != 0 && i == point) buf[len++] = dot.data[0];
      buf[len++] = digits[i];
    }
  };

  if (fixed_length <= scientific_length) {
    if (exponent > 0) {
      // Every candidate of this length is equally short, and std::to_chars
      // then takes the one closest to the value: the value itself, which is
      // an integer here (below 10^22 for double, 10^14 for float).
      constexpr auto significand_mask = (carrier_uint(1) << significand_bits) - 1;
      int const biased = int((bits >> significand_bits) & ((carrier_uint(1) << ieee754<Float>::exponent_bits) - 1));
      std::uint64_t m = std::uint64_t(bits & significand_mask);
      int e2 = ieee754<Float>::min_exponent - significand_bits;
      if (biased != 0) {
        m |= std::uint64_t(1) << significand_bits;
        e2 = biased + ieee754<Float>::exponent_bias - significand_bits;
      }
      if (e2 < 0) {
        m >>= -e2;
        e2 = 0;
      }
      // base 10^9, least significant limb first
      std::uint32_t limbs[3]{std::uint32_t(m % 1000000000), std::uint32_t(m / 1000000000 % 1000000000), std::uint32_t(m / 1000000000000000000)};
      while (e2 > 0) {
        int const shift = e2 < 29 ? e2 : 29;
        std::uint64_t carry = 0;
        for (auto& limb : limbs) {
          std::uint64_t const t = (std::uint64_t(limb) << shift) + carry;
          limb = std::uint32_t(t % 1000000000);
          carry = t / 1000000000;
        }
        e2 -= shift;
      }
      int const end = len + fixed_length;
      for (int i = 0, pos = end; pos > len; ++i) {
        auto limb = limbs[i];
        for (int j = 0; j < 9 && pos > len; ++j, limb /= 10) buf[--pos] = digit_chars.data[limb % 10];
      }
      len = end;
    } else if (-exponent < n) {
      put_significand(n + exponent);
    } else {
      buf[len++] = digit_chars.data[0];
      buf[len++] = dot.data[0];
      for (int i = 0; i < -exponent - n; ++i) buf[len++] = digit_chars.data[0];
      put_significand(0);
    }
    return make_string(buf, len);
  }

  put_significand(1);
  buf[len++] = e.data[0];
  buf[len++] = scientific_exponent < 0 ? minus.data[0] : plus.data[0];
  if (exponent_digits == 3) buf[len++] = digit_chars.data[std::size_t(abs_exponent / 100)];
  buf[len++] = digit_chars.data[std::size_t(abs_exponent / 10 % 10)];
  buf[len++] = digit_chars.data[std::size_t(abs_exponent % 10)];
  return make_string(buf, len);
}

}  // namespace yk::json20::detail::number

#endif  // YK_ZZ_JSON20_DETAIL_NUMBER_FORMAT_FLOAT_HPP
