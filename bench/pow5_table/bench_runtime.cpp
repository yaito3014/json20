// lookups of random q through each variant's pow5_floor, inlined as the
// parsing and printing algorithms would use it
#include "variant_a.hpp"
#include "variant_b.hpp"

#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

template<class Variant>
double ns_per_lookup(std::vector<int> const& qs, int rounds, std::uint64_t& sink)
{
  auto const start = std::chrono::steady_clock::now();
  std::uint64_t acc = 0;
  for (int r = 0; r < rounds; ++r)
    for (int q : qs) {
      auto const v = Variant::pow5_floor(q);
      acc += v.hi ^ v.lo;
    }
  auto const stop = std::chrono::steady_clock::now();
  sink += acc;
  return std::chrono::duration<double, std::nano>(stop - start).count() / (double(qs.size()) * rounds);
}

int main()
{
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(pow5_table::q_min, pow5_table::q_max);
  std::vector<int> qs(1 << 16);
  for (auto& q : qs) q = dist(rng);

  std::uint64_t sink = 0;
  int const rounds = 1500;  // ~1e8 lookups per variant
  ns_per_lookup<pow5_table::variant_a>(qs, 50, sink);  // warm up
  ns_per_lookup<pow5_table::variant_b>(qs, 50, sink);
  double const a = ns_per_lookup<pow5_table::variant_a>(qs, rounds, sink);
  double const b = ns_per_lookup<pow5_table::variant_b>(qs, rounds, sink);
  std::printf("A: %.3f ns/lookup  B: %.3f ns/lookup  (sink %llu)\n", a, b, (unsigned long long)(sink & 1));
}
