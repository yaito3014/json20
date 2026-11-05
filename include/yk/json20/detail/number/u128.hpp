#ifndef YK_ZZ_JSON20_DETAIL_NUMBER_U128_HPP
#define YK_ZZ_JSON20_DETAIL_NUMBER_U128_HPP

#include <cstdint>

namespace yk::json20::detail::number {

struct u128 {
  std::uint64_t hi;
  std::uint64_t lo;
};

constexpr u128 add(u128 x, std::uint64_t d) noexcept
{
  std::uint64_t const lo = x.lo + d;
  return {x.hi + (lo < d ? 1 : 0), lo};
}

constexpr u128 umul128(std::uint64_t a, std::uint64_t b) noexcept
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

}  // namespace yk::json20::detail::number

#endif  // YK_ZZ_JSON20_DETAIL_NUMBER_U128_HPP
