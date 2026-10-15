// a function that needs every entry at run time, which forces the table
// (variant B) or the recovery code and constants (variant A) into the object
#include "variant_a.hpp"

std::uint64_t use_table(int q) { return pow5_table::variant_a::pow5_floor(q).hi; }
