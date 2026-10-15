#ifndef POW5_TABLE_COMMON_HPP
#define POW5_TABLE_COMMON_HPP

// Shared by both variants: F(q) is 5^q scaled into [2^127, 2^128) and rounded
// down. Each variant provides `pow5_floor(q)`; the tables the two algorithms
// need are derived from it below (see pow5_reference.py).

#include <bit>
#include <cstdint>

namespace pow5_table {

struct u128 {
  std::uint64_t hi;
  std::uint64_t lo;

  friend constexpr bool operator==(u128, u128) = default;
};

inline constexpr int q_min = -342;
inline constexpr int q_max = 326;

constexpr u128 add_small(u128 x, std::uint64_t d) noexcept
{
  std::uint64_t const lo = x.lo + d;
  return {x.hi + (lo < d ? 1 : 0), lo};
}

// 64 x 64 -> 128 bit product
constexpr u128 mul64(std::uint64_t a, std::uint64_t b) noexcept
{
#if defined(__SIZEOF_INT128__)
  __extension__ using uint128_t = unsigned __int128;
  uint128_t const p = uint128_t(a) * b;
  return {std::uint64_t(p >> 64), std::uint64_t(p)};
#else
  std::uint64_t const a_lo = a & 0xffffffff, a_hi = a >> 32;
  std::uint64_t const b_lo = b & 0xffffffff, b_hi = b >> 32;
  std::uint64_t const ll = a_lo * b_lo, lh = a_lo * b_hi, hl = a_hi * b_lo, hh = a_hi * b_hi;
  std::uint64_t const mid = (ll >> 32) + (lh & 0xffffffff) + (hl & 0xffffffff);
  return {hh + (lh >> 32) + (hl >> 32) + (mid >> 32), (mid << 32) | (ll & 0xffffffff)};
#endif
}

// Eisel-Lemire (fast_float's power_of_five_128), q in [-342, 308]
template<class Variant>
constexpr u128 eisel_lemire_pow5(int q) noexcept
{
  return add_small(Variant::pow5_floor(q), (-27 <= q && q < 0) ? 1 : 0);
}

// Dragonbox binary64 cache (5^k rounded up), k in [-292, 326]
template<class Variant>
constexpr u128 dragonbox_pow10(int k) noexcept
{
  return add_small(Variant::pow5_floor(k), (0 <= k && k <= 55) ? 0 : 1);
}

}  // namespace pow5_table

#endif
