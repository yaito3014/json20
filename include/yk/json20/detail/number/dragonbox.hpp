#ifndef YK_ZZ_JSON20_DETAIL_NUMBER_DRAGONBOX_HPP
#define YK_ZZ_JSON20_DETAIL_NUMBER_DRAGONBOX_HPP

#include <yk/json20/detail/number/ieee754.hpp>
#include <yk/json20/detail/number/pow5.hpp>
#include <yk/json20/detail/number/u128.hpp>

#include <bit>
#include <cstdint>

namespace yk {

namespace json20 {

namespace detail::number {

// Adapted from the simple implementation of Dragonbox
// (https://github.com/jk-jeon/dragonbox), Copyright 2024-2025 Junekey Jeon,
// Toby Bell, used under the Boost Software License 1.0. The caches come from
// the shared power-of-five table instead of Dragonbox's own.

namespace dragonbox {

constexpr int floor_log10_pow2(int e) noexcept { return (e * 315653) >> 20; }
constexpr int floor_log2_pow10(int e) noexcept { return (e * 1741647) >> 19; }
constexpr int floor_log10_pow2_minus_log10_4_over_3(int e) noexcept { return (e * 631305 - 261663) >> 21; }
constexpr int floor_log5_pow2(int e) noexcept { return (e * 225799) >> 19; }
constexpr int floor_log5_pow2_minus_log5_3(int e) noexcept { return (e * 451597 - 715764) >> 20; }

template<class Int>
constexpr Int compute_power(Int a, int k) noexcept
{
  Int p = 1;
  for (; k > 0; k /= 2) {
    if (k % 2) p *= a;
    a *= a;
  }
  return p;
}

template<class UInt>
constexpr int count_factors(UInt n, int a) noexcept
{
  int c = 0;
  while (n % UInt(a) == 0) {
    n /= UInt(a);
    ++c;
  }
  return c;
}

template<class UInt>
struct mul_result {
  UInt integer_part;
  bool is_integer;
};

struct mul_parity_result {
  bool parity;
  bool is_integer;
};

template<class Float>
struct format;

template<>
struct format<float> {
  using carrier_uint = std::uint32_t;
  using cache_type = std::uint64_t;
  static constexpr int cache_bits = 64;

  static constexpr cache_type cache(int k) noexcept { return pow5::dragonbox_binary32(k); }

  static constexpr void remove_trailing_zeros(std::uint32_t& significand, int& exponent) noexcept
  {
    auto r = std::rotr(std::uint32_t(significand * UINT32_C(184254097)), 4);
    auto b = r < UINT32_C(429497);
    auto s = int(b);
    significand = b ? r : significand;

    r = std::rotr(std::uint32_t(significand * UINT32_C(42949673)), 2);
    b = r < UINT32_C(42949673);
    s = s * 2 + b;
    significand = b ? r : significand;

    r = std::rotr(std::uint32_t(significand * UINT32_C(1288490189)), 1);
    b = r < UINT32_C(429496730);
    s = s * 2 + b;
    significand = b ? r : significand;

    exponent += s;
  }

  static constexpr mul_result<std::uint32_t> compute_mul(std::uint32_t u, std::uint64_t cache) noexcept
  {
    // upper 64 bits of the 96-bit product u * cache
    u128 const p = umul128(u, cache);
    std::uint64_t const r = (p.hi << 32) | (p.lo >> 32);
    return {std::uint32_t(r >> 32), std::uint32_t(r) == 0};
  }

  static constexpr std::uint32_t compute_delta(std::uint64_t cache, int beta) noexcept { return std::uint32_t(cache >> (cache_bits - 1 - beta)); }

  static constexpr mul_parity_result compute_mul_parity(std::uint32_t two_f, std::uint64_t cache, int beta) noexcept
  {
    std::uint64_t const r = two_f * cache;
    return {((r >> (64 - beta)) & 1) != 0, (UINT32_C(0xffffffff) & (r >> (32 - beta))) == 0};
  }

  static constexpr std::uint32_t compute_left_endpoint_for_shorter_interval_case(std::uint64_t cache, int beta) noexcept
  {
    return std::uint32_t((cache - (cache >> (23 + 2))) >> (cache_bits - 23 - 1 - beta));
  }

  static constexpr std::uint32_t compute_right_endpoint_for_shorter_interval_case(std::uint64_t cache, int beta) noexcept
  {
    return std::uint32_t((cache + (cache >> (23 + 1))) >> (cache_bits - 23 - 1 - beta));
  }

  static constexpr std::uint32_t compute_round_up_for_shorter_interval_case(std::uint64_t cache, int beta) noexcept
  {
    return (std::uint32_t(cache >> (cache_bits - 23 - 2 - beta)) + 1) / 2;
  }
};

template<>
struct format<double> {
  using carrier_uint = std::uint64_t;
  using cache_type = u128;
  static constexpr int cache_bits = 128;

  static constexpr cache_type cache(int k) noexcept { return pow5::dragonbox_binary64(k); }

  static constexpr void remove_trailing_zeros(std::uint64_t& significand, int& exponent) noexcept
  {
    auto r = std::rotr(std::uint64_t(significand * UINT64_C(28999941890838049)), 8);
    auto b = r < UINT64_C(184467440738);
    auto s = int(b);
    significand = b ? r : significand;

    r = std::rotr(std::uint64_t(significand * UINT64_C(182622766329724561)), 4);
    b = r < UINT64_C(1844674407370956);
    s = s * 2 + b;
    significand = b ? r : significand;

    r = std::rotr(std::uint64_t(significand * UINT64_C(10330176681277348905)), 2);
    b = r < UINT64_C(184467440737095517);
    s = s * 2 + b;
    significand = b ? r : significand;

    r = std::rotr(std::uint64_t(significand * UINT64_C(14757395258967641293)), 1);
    b = r < UINT64_C(1844674407370955162);
    s = s * 2 + b;
    significand = b ? r : significand;

    exponent += s;
  }

  // upper 128 bits of the 192-bit product u * cache
  static constexpr mul_result<std::uint64_t> compute_mul(std::uint64_t u, u128 cache) noexcept
  {
    u128 const r = add(umul128(u, cache.hi), umul128(u, cache.lo).hi);
    return {r.hi, r.lo == 0};
  }

  static constexpr std::uint64_t compute_delta(u128 cache, int beta) noexcept { return cache.hi >> (64 - 1 - beta); }

  static constexpr mul_parity_result compute_mul_parity(std::uint64_t two_f, u128 cache, int beta) noexcept
  {
    // lower 128 bits of the 192-bit product two_f * cache
    u128 const low = umul128(two_f, cache.lo);
    u128 const r{two_f * cache.hi + low.hi, low.lo};
    return {((r.hi >> (64 - beta)) & 1) != 0, ((r.hi << beta) | (r.lo >> (64 - beta))) == 0};
  }

  static constexpr std::uint64_t compute_left_endpoint_for_shorter_interval_case(u128 cache, int beta) noexcept
  {
    return (cache.hi - (cache.hi >> (52 + 2))) >> (64 - 52 - 1 - beta);
  }

  static constexpr std::uint64_t compute_right_endpoint_for_shorter_interval_case(u128 cache, int beta) noexcept
  {
    return (cache.hi + (cache.hi >> (52 + 1))) >> (64 - 52 - 1 - beta);
  }

  static constexpr std::uint64_t compute_round_up_for_shorter_interval_case(u128 cache, int beta) noexcept
  {
    return ((cache.hi >> (64 - 52 - 2 - beta)) + 1) / 2;
  }
};

template<class UInt>
struct decimal_fp {
  UInt significand;
  int exponent;
};

// The shortest decimal significand * 10^exponent that rounds back to the
// given finite, nonzero float (ties broken towards the closest, then even).
template<class Float>
constexpr decimal_fp<typename ieee754<Float>::carrier_uint> to_decimal(Float x) noexcept
{
  using fmt = format<Float>;
  using carrier_uint = typename ieee754<Float>::carrier_uint;
  constexpr int significand_bits = ieee754<Float>::significand_bits;
  constexpr int exponent_bits = ieee754<Float>::exponent_bits;
  constexpr int carrier_bits = 8 * int(sizeof(carrier_uint));
  constexpr int kappa = floor_log10_pow2(carrier_bits - significand_bits - 2) - 1;
  constexpr carrier_uint big_divisor = compute_power(carrier_uint(10), kappa + 1);
  constexpr carrier_uint small_divisor = compute_power(carrier_uint(10), kappa);

  constexpr int left_endpoint_upper_threshold =
      2 + (int(std::bit_width(compute_power(std::uint64_t(10), count_factors((carrier_uint(1) << (significand_bits + 2)) - 1, 5) + 1) / 3)) - 1);
  constexpr int shorter_interval_tie_lower_threshold = -floor_log5_pow2_minus_log5_3(significand_bits + 4) - 2 - significand_bits;
  constexpr int shorter_interval_tie_upper_threshold = -floor_log5_pow2(significand_bits + 2) - 2 - significand_bits;

  auto const bits = std::bit_cast<carrier_uint>(x);
  carrier_uint binary_significand = bits & ((carrier_uint(1) << significand_bits) - 1);
  int binary_exponent = int((bits >> significand_bits) & ((carrier_uint(1) << exponent_bits) - 1));

  bool const is_even = binary_significand % 2 == 0;
  carrier_uint two_fc = binary_significand * 2;

  if (binary_exponent != 0) {
    binary_exponent += ieee754<Float>::exponent_bias - significand_bits;

    // shorter interval case: the significand is a power of two
    if (two_fc == 0) {
      int const minus_k = floor_log10_pow2_minus_log10_4_over_3(binary_exponent);
      int const beta = binary_exponent + floor_log2_pow10(-minus_k);
      auto const cache = fmt::cache(-minus_k);

      auto xi = fmt::compute_left_endpoint_for_shorter_interval_case(cache, beta);
      auto const zi = fmt::compute_right_endpoint_for_shorter_interval_case(cache, beta);
      // both endpoints are included since the significand is even; an
      // endpoint that is not an integer is rounded inwards
      if (!(binary_exponent >= 2 && binary_exponent <= left_endpoint_upper_threshold)) ++xi;

      carrier_uint decimal_significand = zi / 10;
      if (decimal_significand * 10 >= xi) {
        int decimal_exponent = minus_k + 1;
        fmt::remove_trailing_zeros(decimal_significand, decimal_exponent);
        return {decimal_significand, decimal_exponent};
      }

      decimal_significand = fmt::compute_round_up_for_shorter_interval_case(cache, beta);
      if (decimal_significand % 2 != 0 && binary_exponent >= shorter_interval_tie_lower_threshold &&
          binary_exponent <= shorter_interval_tie_upper_threshold) {
        --decimal_significand;
      } else if (decimal_significand < xi) {
        ++decimal_significand;
      }
      return {decimal_significand, minus_k};
    }

    two_fc |= carrier_uint(1) << (significand_bits + 1);
  } else {
    binary_exponent = ieee754<Float>::min_exponent - significand_bits;
  }

  // step 1: Schubfach multiplier calculation
  int const minus_k = floor_log10_pow2(binary_exponent) - kappa;
  auto const cache = fmt::cache(-minus_k);
  int const beta = binary_exponent + floor_log2_pow10(-minus_k);

  auto const deltai = fmt::compute_delta(cache, beta);
  auto const z_result = fmt::compute_mul(carrier_uint((two_fc | 1) << beta), cache);

  // step 2: try the larger divisor; remove trailing zeros if necessary
  carrier_uint decimal_significand = z_result.integer_part / big_divisor;
  auto r = carrier_uint(z_result.integer_part - big_divisor * decimal_significand);

  do {
    if (r < deltai) {
      // exclude the right endpoint if necessary
      if ((r | carrier_uint(!z_result.is_integer) | carrier_uint(is_even)) == 0) {
        --decimal_significand;
        r = big_divisor;
        break;
      }
    } else if (r > deltai) {
      break;
    } else {
      // r == deltai; compare fractional parts
      auto const x_result = fmt::compute_mul_parity(carrier_uint(two_fc - 1), cache, beta);
      if (!(x_result.parity | (x_result.is_integer & is_even))) break;
    }

    int decimal_exponent = minus_k + kappa + 1;
    fmt::remove_trailing_zeros(decimal_significand, decimal_exponent);
    return {decimal_significand, decimal_exponent};
  } while (false);

  // step 3: find the significand with the smaller divisor
  decimal_significand *= 10;

  auto dist = carrier_uint(r - (deltai / 2) + (small_divisor / 2));
  bool const approx_y_parity = ((dist ^ (small_divisor / 2)) & 1) != 0;

  bool const divisible_by_small_divisor = dist % small_divisor == 0;
  dist /= small_divisor;
  decimal_significand += dist;

  if (divisible_by_small_divisor) {
    auto const y_result = fmt::compute_mul_parity(two_fc, cache, beta);
    if (y_result.parity != approx_y_parity) {
      --decimal_significand;
    } else if ((decimal_significand % 2) & y_result.is_integer) {
      // tie: choose the even one
      --decimal_significand;
    }
  }

  return {decimal_significand, minus_k + kappa};
}

}  // namespace dragonbox

}  // namespace detail::number

}  // namespace json20

}  // namespace yk

#endif  // YK_ZZ_JSON20_DETAIL_NUMBER_DRAGONBOX_HPP
