// Checks both variants against reference.inc (exact values from
// pow5_reference.py), at run time and in constant evaluation.

#include "variant_a.hpp"
#include "variant_b.hpp"

#include <array>
#include <cstdio>

namespace {

constexpr std::array<pow5_table::u128, pow5_table::q_max - pow5_table::q_min + 1> reference = {{
#include "reference.inc"
}};

template<class Variant>
constexpr int mismatches()
{
  int bad = 0;
  for (int q = pow5_table::q_min; q <= pow5_table::q_max; ++q) {
    if (!(Variant::pow5_floor(q) == reference[std::size_t(q - pow5_table::q_min)])) ++bad;
  }
  return bad;
}

static_assert(mismatches<pow5_table::variant_a>() == 0);
static_assert(mismatches<pow5_table::variant_b>() == 0);

}  // namespace

int main()
{
  int const a = mismatches<pow5_table::variant_a>();
  int const b = mismatches<pow5_table::variant_b>();
  std::printf("variant A: %d mismatches, variant B: %d mismatches (of %d)\n", a, b, pow5_table::q_max - pow5_table::q_min + 1);
  return a + b == 0 ? 0 : 1;
}
