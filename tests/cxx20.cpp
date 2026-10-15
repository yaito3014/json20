// Built as C++20, the library's minimum standard (the other tests use C++23
// features). Successful compilation is most of the test.

#include <yk/json20.hpp>

#include <cstddef>
#include <type_traits>

using yk::json20::json;

// nullptr is not a string: in C++20 it converts to std::string_view (through
// const char*), and a string would be built from a null pointer. Null is json().
static_assert(!std::is_constructible_v<json, std::nullptr_t>);
static_assert(!std::is_assignable_v<json&, std::nullptr_t>);
static_assert(std::is_constructible_v<json, char const*>);

int main()
{
  json const doc = yk::json20::json_parser::parse(R"({"a": [1, -2, true, null, "x"]})");
  return doc.at("a").at(0).as_unsigned_integer<unsigned>() == 1 ? 0 : 1;
}
