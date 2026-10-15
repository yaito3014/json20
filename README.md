# json20

json20 is a header-only C++20 JSON library whose parser and DOM are `constexpr`,
so JSON can be validated and read at compile time as well as at run time.

- **Compile-time JSON**: `checked_string` rejects a malformed JSON literal at compile time, and
  parsing and DOM access can run inside constant expressions
- **Every character type**: `char`, `wchar_t`, `char8_t`, `char16_t` and `char32_t`. The grammar
  is written in terms of code points, so parsing also works under non-ASCII execution character
  sets such as EBCDIC
- **Strict**: input must be RFC 8259 JSON, duplicate object keys are rejected by default, and
  accessors never convert between kinds
- **No dependencies**: a single header, `<yk/json20.hpp>`

## Quick start

```cpp
#include <yk/json20.hpp>

using namespace yk::json20;

json const doc = json_parser::parse(R"({"name": "json20", "version": [0, 0, 1], "stable": false})");

doc.at("name").as_string();                               // "json20"
doc.at("version").at(2).as_unsigned_integer<unsigned>();  // 1
doc.at("stable").as_boolean();                            // false
```

Values can also be built directly:

```cpp
json j = json::object();
j["name"] = "json20";
j["tags"] = {"constexpr", "header-only"};  // an array
j["answer"] = 42;

json const k = json::object({{"x", 1.5}, {"y", json()}});  // json() is null
```

## Compile-time JSON

A function taking `checked_string` only accepts string literals that are valid JSON:

```cpp
void send(yk::json20::checked_string body);

send(R"({"id": 1, "tags": ["a", "b"]})");  // OK
send(R"({"id": 1,})");                     // compile error: not valid JSON
```

Parsing into a DOM works in constant expressions too:

```cpp
constexpr unsigned answer =
    yk::json20::json_parser::parse(R"({"answer": 42})").at("answer").as_unsigned_integer<unsigned>();
static_assert(answer == 42);
```

This needs a standard library whose `std::variant` of `std::string` can be used in constant
evaluation: it works with GCC 14 and Clang 21, but not with Clang 18 and libstdc++.
`checked_string` works everywhere.

## Values are strict

Each value has a kind (`get_kind()`), and numbers keep their original text, converted only when
read. The kind of a number follows its spelling: `5` is an unsigned integer, `-5` a signed
integer, and `5.0` or `5e0` a floating-point number. Accessors check the kind and never convert:

```cpp
json const n = json_parser::parse("5");

n.as_unsigned_integer<unsigned>();  // 5
n.as_signed_integer<int>();         // throws bad_json_access: "5" is unsigned
```

Errors are reported by exceptions, and each operation has a non-throwing form:

| Throws | Does not throw |
|---|---|
| `json_parser::parse(str)` (`std::invalid_argument`) | `json_parser::try_parse(str)` returns `std::optional<json>` |
| `as_string()`, `as_unsigned_integer<T>()`, ... (`bad_json_access`) | `try_as_string()` returns a pointer, `try_as_unsigned_integer<T>()` an `std::optional` |
| `at(key)`, `at(index)` (`std::out_of_range` if missing) | `try_at(key)`, `try_at(index)` return a pointer |

Objects with a repeated key are rejected by default. To accept them, choose which value to keep:

```cpp
json_parser::parse(R"({"a": 1, "a": 2})",
                   {.duplicate_keys = duplicate_key_policy::last_wins});  // {"a": 2}
```

## Other character types

Every type has a version for each character type: `wjson` / `wjson_parser` for `wchar_t`, and
`u8json`, `u16json` and `u32json` likewise.

```cpp
u16json const doc = u16json_parser::parse(u"{\"name\": \"json20\", \"id\": 7}");
doc.at(u"name").as_string();  // u"json20"
```

## Streaming with a visitor

`parse` also accepts a visitor, which receives each value as it is parsed (with its text)
instead of building a DOM:

```cpp
struct number_counter {
  int count = 0;
  void on_number_unsigned_integer(std::string_view) { ++count; }
  void on_number_signed_integer(std::string_view) { ++count; }
  void on_number_floating_point(std::string_view) { ++count; }
  void on_null(std::string_view) {}
  void on_boolean(std::string_view) {}
  void on_string(std::string_view) {}
  void on_array_start() {}
  void on_array_finalize() {}
  void on_array_abort() {}
  void on_object_start() {}
  void on_object_finalize() {}
  void on_object_abort() {}
};

number_counter v;
json_parser::parse(v, R"([1, -2, 3.5, "x"])");  // v.count == 3
```

## Limitations

- There is no serialization yet: a DOM cannot be written back out as JSON text.
- Objects are stored sorted by key; the order of members in the input is not kept.

## Installation

Copy `include/yk/json20.hpp`, or use CMake:

```cmake
include(FetchContent)
FetchContent_Declare(json20 GIT_REPOSITORY https://github.com/yaito3014/json20 GIT_TAG main)
FetchContent_MakeAvailable(json20)

target_link_libraries(your_target PRIVATE yk::json20)
```

After `cmake --install`, `find_package(json20)` provides the same `yk::json20` target.

json20 requires C++20. CI tests C++23 and C++26 with GCC 14, Clang 21 (libstdc++ and libc++),
and MSVC 2022 and 2026.

## Building the tests

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

The tests need C++23 and use Catch2, which CMake downloads automatically. Projects that use
json20 through CMake do not download it.

## License

[Boost Software License 1.0](LICENSE)

Design decisions and their reasons are recorded in [docs/design-notes.md](docs/design-notes.md)
(in Japanese).
