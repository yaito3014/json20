// forces constant evaluation of variant B's table, for measuring step limits
#include "variant_b.hpp"

static_assert(pow5_table::variant_b::pow5_floor(0).hi == 0x8000000000000000);
