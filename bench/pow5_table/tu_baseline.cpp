// compile-time baseline: the shared header and the standard headers both
// variants include (json20.hpp already includes <array>)
#include "common.hpp"

#include <array>
#include <bit>
#include <cstddef>

std::uint64_t use_table(int q) { return pow5_table::mul64(std::uint64_t(q), 5).lo; }
