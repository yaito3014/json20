#include <yk/json20.hpp>
// constant-evaluation cost: many checked_string literals
using cs = yk::json20::checked_string;
constexpr cs a[] = {
  "{\"name\":\"json20\",\"version\":[0,0,1],\"tags\":[\"c++20\",\"constexpr\",\"header-only\"]}",
  "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]",
  "{\"a\":{\"b\":{\"c\":{\"d\":[true,false,null,1.5e10,-0.25]}}}}",
  "[\"lorem ipsum dolor sit amet\",\"consectetur adipiscing elit\",\"sed do eiusmod\"]",
  "{\"k1\":1,\"k2\":2,\"k3\":3,\"k4\":4,\"k5\":5,\"k6\":6,\"k7\":7,\"k8\":8}",
};
int main() { return sizeof(a) == 0; }
