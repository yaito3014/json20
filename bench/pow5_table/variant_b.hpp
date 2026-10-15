#ifndef POW5_TABLE_VARIANT_B_HPP
#define POW5_TABLE_VARIANT_B_HPP

// Variant B: no constants in the source. All 669 entries of F(q) are computed
// in constant evaluation with schoolbook big integers (32-bit limbs, so every
// step is a native 64-bit operation on every compiler):
//   q >= 0: P = 5^q by repeated multiplication by 5
//   q <  0: A = floor(2^959 / 5^-q) by repeated division by 5 (floor of a
//           floor is the floor, so no error accumulates; 2^959 leaves at
//           least 164 bits after dividing by 5^342)
// F(q) is the top 128 bits. The table is a variable template, so only
// translation units that use it pay for generating it.

#include "common.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace pow5_table {

namespace variant_b_detail {

template<std::size_t N>
using bigint = std::array<std::uint32_t, N>;  // little-endian limbs

template<std::size_t N>
constexpr u128 top128(bigint<N> const& x) noexcept
{
  std::size_t top = N - 1;
  while (x[top] == 0) --top;
  int const bits = int(top) * 32 + (32 - std::countl_zero(x[top]));
  auto const limb = [&](int i) -> std::uint64_t { return i >= 0 && std::size_t(i) < N ? x[std::size_t(i)] : 0; };

  int const shift = bits - 128;
  if (shift <= 0) {
    // fewer than 128 bits: the low 4 limbs, shifted left
    u128 const v{(limb(3) << 32) | limb(2), (limb(1) << 32) | limb(0)};
    int const s = -shift;
    if (s == 0) return v;
    if (s >= 64) return {v.lo << (s - 64), 0};
    return {(v.hi << s) | (v.lo >> (64 - s)), v.lo << s};
  }

  // x >> shift, assembled 32 bits at a time
  int const w = shift / 32, b = shift % 32;
  auto const chunk = [&](int j) -> std::uint64_t {
    std::uint64_t const low_part = limb(w + j) >> b;
    std::uint64_t const high_part = b == 0 ? 0 : (limb(w + j + 1) << (32 - b)) & 0xffffffff;
    return low_part | high_part;
  };
  return {(chunk(3) << 32) | chunk(2), (chunk(1) << 32) | chunk(0)};
}

constexpr std::array<u128, q_max - q_min + 1> generate() noexcept
{
  std::array<u128, q_max - q_min + 1> table{};

  bigint<24> p{};  // 5^326 < 2^757
  p[0] = 1;
  for (int q = 0; q <= q_max; ++q) {
    table[std::size_t(q - q_min)] = top128(p);
    std::uint64_t carry = 0;
    for (auto& limb : p) {
      std::uint64_t const v = std::uint64_t(limb) * 5 + carry;
      limb = std::uint32_t(v);
      carry = v >> 32;
    }
  }

  bigint<30> a{};  // 2^959
  a[29] = std::uint32_t(1) << 31;
  for (int q = -1; q >= q_min; --q) {
    std::uint64_t rem = 0;
    for (std::size_t i = a.size(); i-- > 0;) {
      std::uint64_t const v = (rem << 32) | a[i];
      a[i] = std::uint32_t(v / 5);
      rem = v % 5;
    }
    table[std::size_t(q - q_min)] = top128(a);
  }
  return table;
}

template<class = void>
inline constexpr std::array<u128, q_max - q_min + 1> table = generate();

}  // namespace variant_b_detail

struct variant_b {
  static constexpr u128 pow5_floor(int q) noexcept { return variant_b_detail::table<>[std::size_t(q - q_min)]; }
};

}  // namespace pow5_table

#endif
