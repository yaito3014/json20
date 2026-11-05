#ifndef YK_JSON20_DETAIL_NUMBER_PARSE_FLOAT_HPP
#define YK_JSON20_DETAIL_NUMBER_PARSE_FLOAT_HPP

#include <yk/json20/detail/fixed_string.hpp>
#include <yk/json20/detail/number/ieee754.hpp>
#include <yk/json20/detail/number/pow5.hpp>
#include <yk/json20/detail/number/u128.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <cassert>

namespace yk {

namespace json20 {

namespace detail::number {

// Decimal to binary: Eisel-Lemire
//
// Adapted from fast_float (https://github.com/fastfloat/fast_float),
// Copyright (c) 2021 The fast_float authors, used under the Boost Software
// License 1.0. Algorithm: D. Lemire, "Number Parsing at a Gigabyte per
// Second" (2021); N. Mushtak, D. Lemire, "Fast Number Parsing Without
// Fallback" (2023) proves the 128-bit product always suffices for a 64-bit
// decimal significand.

template<class Float>
constexpr adjusted_mantissa eisel_lemire(std::int64_t q, std::uint64_t w) noexcept
{
  using format = ieee754<Float>;
  if (w == 0 || q < format::smallest_power_of_ten) return {0, 0};
  if (q > format::largest_power_of_ten) return {0, format::infinite_power};

  int const lz = std::countl_zero(w);
  w <<= lz;

  // w * 5^q, precise to significand_bits + 3 bits
  constexpr int bit_precision = format::significand_bits + 3;
  constexpr std::uint64_t precision_mask = std::uint64_t(0xffffffffffffffff) >> bit_precision;
  u128 const power = pow5::eisel_lemire(int(q));
  u128 product = umul128(w, power.hi);
  if ((product.hi & precision_mask) == precision_mask) {
    u128 const second = umul128(w, power.lo);
    product.lo += second.hi;
    if (second.hi > product.lo) ++product.hi;
  }

  int const upperbit = int(product.hi >> 63);
  int const shift = upperbit + 64 - format::significand_bits - 3;
  adjusted_mantissa answer{product.hi >> shift, 0};
  // floor(log2(5^q)) + q + 63 for q in (-400, 350)
  int const power2 = (((152170 + 65536) * int(q)) >> 16) + 63;
  answer.power2 = power2 + upperbit - lz - (format::exponent_bias);

  if (answer.power2 <= 0) {  // subnormal
    if (-answer.power2 + 1 >= 64) return {0, 0};
    answer.mantissa >>= -answer.power2 + 1;
    answer.mantissa += (answer.mantissa & 1);
    answer.mantissa >>= 1;
    // rounding up may have carried into the smallest normal exponent
    answer.power2 = answer.mantissa < (std::uint64_t(1) << format::significand_bits) ? 0 : 1;
    return answer;
  }

  // round half to even when exactly between two floats, which is only
  // possible when 5^q fits in 64 bits
  if (product.lo <= 1 && q >= format::min_exponent_round_to_even && q <= format::max_exponent_round_to_even && (answer.mantissa & 3) == 1) {
    if ((answer.mantissa << shift) == product.hi) answer.mantissa &= ~std::uint64_t(1);
  }
  answer.mantissa += (answer.mantissa & 1);
  answer.mantissa >>= 1;
  if (answer.mantissa >= (std::uint64_t(2) << format::significand_bits)) {
    answer.mantissa = std::uint64_t(1) << format::significand_bits;
    ++answer.power2;
  }
  answer.mantissa &= ~(std::uint64_t(1) << format::significand_bits);
  if (answer.power2 >= format::infinite_power) return {0, format::infinite_power};
  return answer;
}

// Decimal to binary: exact comparison for more than 19 significant digits
//
// When the 19 leading digits w and w + 1 round to different floats, the
// input lies near the midpoint between them, and only exact arithmetic on
// all digits can tell which side.

class bigint {
public:
  // enough for any comparison made below (about 2,700 bits for double)
  static constexpr std::size_t capacity = 160;

  // *this = *this * m + a
  constexpr void mul_add(std::uint32_t m, std::uint32_t a) noexcept
  {
    std::uint64_t carry = a;
    for (std::size_t i = 0; i < size_; ++i) {
      std::uint64_t const v = std::uint64_t(limbs_[i]) * m + carry;
      limbs_[i] = std::uint32_t(v);
      carry = v >> 32;
    }
    if (carry != 0) {
      assert(size_ < capacity);
      limbs_[size_++] = std::uint32_t(carry);
    }
  }

  constexpr void mul_pow5(std::int64_t n) noexcept
  {
    for (; n >= 13; n -= 13) mul_add(1220703125, 0);  // 5^13
    std::uint32_t p = 1;
    for (; n > 0; --n) p *= 5;
    mul_add(p, 0);
  }

  constexpr void shl(std::int64_t n) noexcept
  {
    if (size_ == 0) return;
    auto const words = std::size_t(n / 32);
    auto const bits = int(n % 32);
    assert(size_ + words + 1 <= capacity);
    if (bits != 0) {
      std::uint32_t carry = 0;
      for (std::size_t i = 0; i < size_; ++i) {
        std::uint32_t const v = limbs_[i];
        limbs_[i] = (v << bits) | carry;
        carry = v >> (32 - bits);
      }
      if (carry != 0) limbs_[size_++] = carry;
    }
    if (words != 0) {
      for (std::size_t i = size_; i-- > 0;) limbs_[i + words] = limbs_[i];
      for (std::size_t i = 0; i < words; ++i) limbs_[i] = 0;
      size_ += words;
    }
  }

  friend constexpr int compare(bigint const& a, bigint const& b) noexcept
  {
    if (a.size_ != b.size_) return a.size_ < b.size_ ? -1 : 1;
    for (std::size_t i = a.size_; i-- > 0;) {
      if (a.limbs_[i] != b.limbs_[i]) return a.limbs_[i] < b.limbs_[i] ? -1 : 1;
    }
    return 0;
  }

private:
  std::array<std::uint32_t, capacity> limbs_{};
  std::size_t size_ = 0;
};

// The significant digits of a decimal number: value = digits * 10^exponent,
// plus something nonzero beyond the digits kept when `truncated` is set.
template<class charT>
struct decimal_digits {
  std::basic_string_view<charT> integer;
  std::basic_string_view<charT> fraction;
  std::int64_t explicit_exponent;
};

// Rounds the decimal number to the nearer of the float `am` and the next
// float up, ties to even. The decimal is known to lie between them.
template<class Float, class charT>
constexpr adjusted_mantissa round_exactly(decimal_digits<charT> const& d, adjusted_mantissa am) noexcept
{
  using format = ieee754<Float>;
  constexpr auto zero = YK_JSON20_WIDEN_STRING(charT, "0");

  // the decimal as (all significant digits) * 10^exponent, keeping at most
  // max_digits of them
  bigint value;
  std::size_t kept = 0;
  bool truncated = false;
  std::int64_t exponent = d.explicit_exponent - std::int64_t(d.fraction.size());
  bool leading = true;
  for (auto const part : {d.integer, d.fraction}) {
    for (charT const c : part) {
      auto const digit = std::uint32_t(static_cast<long long>(c) - static_cast<long long>(zero.data[0]));
      if (leading && digit == 0) continue;
      leading = false;
      if (kept < format::max_digits) {
        value.mul_add(10, digit);
        ++kept;
      } else {
        ++exponent;
        truncated = truncated || digit != 0;
      }
    }
  }

  // the midpoint between am and the next float: (2m + 1) * 2^(e - 1)
  std::uint64_t const bits = am.mantissa | (std::uint64_t(am.power2) << format::significand_bits);
  std::uint64_t m = bits & ((std::uint64_t(1) << format::significand_bits) - 1);
  std::int64_t e = 1 + format::exponent_bias - format::significand_bits;
  if (int const biased = int(bits >> format::significand_bits); biased != 0) {
    m |= std::uint64_t(1) << format::significand_bits;
    e += biased - 1;
  }
  bigint midpoint;
  midpoint.mul_add(1, std::uint32_t((2 * m + 1) >> 32));
  midpoint.shl(32);
  midpoint.mul_add(1, std::uint32_t(2 * m + 1));

  // compare digits * 5^exponent * 2^exponent with midpoint * 2^(e - 1)
  if (exponent >= 0)
    value.mul_pow5(exponent);
  else
    midpoint.mul_pow5(-exponent);
  std::int64_t const twos = exponent - (e - 1);
  if (twos >= 0)
    value.shl(twos);
  else
    midpoint.shl(-twos);

  int const c = compare(value, midpoint);
  bool const round_up = c > 0 || (c == 0 && (truncated || (m & 1) != 0));
  if (!round_up) return am;
  std::uint64_t const next = bits + 1;
  return {next & ((std::uint64_t(1) << format::significand_bits) - 1), int(next >> format::significand_bits)};
}

// Decimal to binary: scanning and the overall conversion

template<class Float>
constexpr Float to_float(bool negative, adjusted_mantissa am) noexcept
{
  using carrier_uint = typename ieee754<Float>::carrier_uint;
  constexpr int significand_bits = ieee754<Float>::significand_bits;
  constexpr int sign_index = significand_bits + ieee754<Float>::exponent_bits;
  auto const word = carrier_uint(am.mantissa | (carrier_uint(am.power2) << significand_bits) | (carrier_uint(negative) << sign_index));
  return std::bit_cast<Float>(word);
}

// Parses a JSON number at the start of `str` (stopping at the first
// character that cannot continue it, like from_chars) and returns the
// correctly rounded value and the number of characters consumed. Throws
// std::invalid_argument if there is no number or it is out of range: its
// magnitude rounds to infinity, or a nonzero value rounds to zero.
template<class Float, class charT>
  requires ieee754_binary<Float>
constexpr std::pair<Float, std::size_t> parse_float(std::basic_string_view<charT> str)
{
  constexpr auto zero = YK_JSON20_WIDEN_STRING(charT, "0");
  constexpr auto minus = YK_JSON20_WIDEN_STRING(charT, "-");
  constexpr auto plus = YK_JSON20_WIDEN_STRING(charT, "+");
  constexpr auto dot = YK_JSON20_WIDEN_STRING(charT, ".");
  constexpr auto e_lower = YK_JSON20_WIDEN_STRING(charT, "e");
  constexpr auto e_upper = YK_JSON20_WIDEN_STRING(charT, "E");

  auto const digit_at = [&](std::size_t i) -> int {
    if (i >= str.size()) return -1;
    auto const offset = static_cast<long long>(str[i]) - static_cast<long long>(zero.data[0]);
    return offset >= 0 && offset <= 9 ? int(offset) : -1;
  };
  auto const digits_from = [&](std::size_t i) {
    std::size_t j = i;
    while (digit_at(j) >= 0) ++j;
    return j;
  };

  std::size_t i = 0;
  bool const negative = !str.empty() && str[0] == minus.data[0];
  if (negative) ++i;

  decimal_digits<charT> d{{}, {}, 0};
  std::size_t const integer_end = digits_from(i);
  if (integer_end == i) throw std::invalid_argument("no digits");
  d.integer = str.substr(i, integer_end - i);
  i = integer_end;

  if (i < str.size() && str[i] == dot.data[0] && digit_at(i + 1) >= 0) {
    std::size_t const fraction_end = digits_from(i + 1);
    d.fraction = str.substr(i + 1, fraction_end - i - 1);
    i = fraction_end;
  }

  if (i < str.size() && (str[i] == e_lower.data[0] || str[i] == e_upper.data[0])) {
    std::size_t j = i + 1;
    bool exponent_negative = false;
    if (j < str.size() && (str[j] == minus.data[0] || str[j] == plus.data[0])) {
      exponent_negative = str[j] == minus.data[0];
      ++j;
    }
    if (digit_at(j) >= 0) {
      std::int64_t exponent = 0;
      for (; digit_at(j) >= 0; ++j) {
        // saturate; anything this large is out of range either way
        if (exponent < 1000000) exponent = exponent * 10 + digit_at(j);
      }
      d.explicit_exponent = exponent_negative ? -exponent : exponent;
      i = j;
    }
  }

  // the first 19 significant digits as w; value = w * 10^q (+ tail)
  std::uint64_t w = 0;
  std::int64_t significant = 0;
  bool truncated = false;
  for (auto const part : {d.integer, d.fraction}) {
    for (charT const c : part) {
      auto const digit = std::uint64_t(static_cast<long long>(c) - static_cast<long long>(zero.data[0]));
      if (significant == 0 && digit == 0) continue;
      if (significant < 19)
        w = w * 10 + digit;
      else
        truncated = truncated || digit != 0;
      ++significant;
    }
  }
  std::int64_t const q = d.explicit_exponent - std::int64_t(d.fraction.size()) + (significant > 19 ? significant - 19 : 0);

  if (w == 0) return {to_float<Float>(negative, {0, 0}), i};

  adjusted_mantissa am = eisel_lemire<Float>(q, w);
  if (truncated) {
    adjusted_mantissa const above = eisel_lemire<Float>(q, w + 1);
    if (!(am == above)) am = round_exactly<Float>(d, am);
  }

  if (am.power2 == ieee754<Float>::infinite_power) throw std::invalid_argument("number out of range");
  if (am.power2 == 0 && am.mantissa == 0) throw std::invalid_argument("number out of range");
  return {to_float<Float>(negative, am), i};
}

}  // namespace detail::number

}  // namespace json20

}  // namespace yk

#endif  // YK_JSON20_DETAIL_NUMBER_PARSE_FLOAT_HPP
