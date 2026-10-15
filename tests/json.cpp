#include <yk/json20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>

using json = yk::json20::basic_json<char>;

TEST_CASE("ctor", "[json]")
{
  {
    // a default-constructed json is null, not an empty object
    json a;
    CHECK(a.get_kind() == yk::json20::json_value_kind::null);
    CHECK_THROWS_AS(a.as_object(), yk::json20::bad_json_access);
  }
  {
    json a = json::object();
    CHECK(a.get_kind() == yk::json20::json_value_kind::object);
    CHECK(a.as_object().empty());
  }
  {
    json a = json::array();
    CHECK(a.get_kind() == yk::json20::json_value_kind::array);
    CHECK(a.as_array().empty());
  }
  {
    json a(true);
    CHECK(a.as_boolean() == true);
  }
  {
    json a(false);
    CHECK(a.as_boolean() == false);
  }
  {
    json a(42u);
    CHECK(a.as_unsigned_integer<unsigned>() == 42u);
  }
  {
    json a(42);
    CHECK(a.as_signed_integer<signed>() == 42);
  }
  {
    json a(3.14);
    CHECK(a.as_floating_point<double>() == 3.14);
  }
  {
    json a("foo");
    CHECK(a.as_string() == "foo");
  }
  {
    json a{42, 3.14, "foo", {std::pair{"bar", 42}}};
    CHECK(a.at(0).as_signed_integer<signed>() == 42);
    CHECK(a.at(1).as_floating_point<double>() == 3.14);
    CHECK(a.at(2).as_string() == "foo");
    CHECK(a.at(3).at("bar").as_signed_integer<signed>() == 42);
  }
  {
    json a{std::pair{"foo", 42}, {"bar", 3.14}};
    CHECK(a.at("foo").as_signed_integer<signed>() == 42);
    CHECK(a.at("bar").as_floating_point<double>() == 3.14);
  }
  {
    json a = json::array({42, 3.14, "foo", json::object({{"bar", 42}})});
    CHECK(a.at(0).as_signed_integer<signed>() == 42);
    CHECK(a.at(1).as_floating_point<double>() == 3.14);
    CHECK(a.at(2).as_string() == "foo");
    CHECK(a.at(3).at("bar").as_signed_integer<signed>() == 42);
  }
  {
    json a = json::object({{"foo", 42}, {"bar", 3.14}});
    CHECK(a.at("foo").as_signed_integer<signed>() == 42);
    CHECK(a.at("bar").as_floating_point<double>() == 3.14);
  }
}

TEST_CASE("assignment", "[json]")
{
  json a;
  CHECK(a.get_kind() == yk::json20::json_value_kind::null);
  a = 42u;
  CHECK(a.as_unsigned_integer<unsigned>() == 42u);
  a = 42;
  CHECK(a.as_signed_integer<signed>() == 42);
  a = 3.14;
  CHECK(a.as_floating_point<double>() == 3.14);
  a = "foo";
  CHECK(a.as_string() == "foo");
  a = {42, 3.14, {std::pair{"foo", 42}}};
  CHECK(a.at(0).as_signed_integer<signed>() == 42);
  CHECK(a.at(1).as_floating_point<double>() == 3.14);
  CHECK(a.at(2).at("foo").as_signed_integer<signed>() == 42);
  a = {std::pair{"foo", 42}, {"bar", 3.14}};
  CHECK(a.at("foo").as_signed_integer<signed>() == 42);
  CHECK(a.at("bar").as_floating_point<double>() == 3.14);

  // bool satisfies std::unsigned_integral, but stays a boolean
  a = true;
  CHECK(a.get_kind() == yk::json20::json_value_kind::boolean);
  CHECK(a.as_boolean() == true);
  json o = json::object();
  o["flag"] = false;
  CHECK(o.at("flag").try_as_boolean() == false);
  yk::json20::wjson w = yk::json20::wjson::array();
  w = true;
  CHECK(w.as_boolean() == true);
}

TEST_CASE("subscript", "[json]")
{
  json a = json::array({});
  a[0] = 42;
  CHECK(a.at(0).as_signed_integer<signed>() == 42);

  // growing the array fills the gap with null, per JSON semantics
  a[3] = 1;
  CHECK(a.at(1).get_kind() == yk::json20::json_value_kind::null);
  CHECK(a.at(2).get_kind() == yk::json20::json_value_kind::null);
  CHECK(a.at(3).as_signed_integer<signed>() == 1);

  // no implicit null -> object promotion: kind mismatch always throws
  CHECK_THROWS_AS(a[1]["foo"], yk::json20::bad_json_access);
  json n;
  CHECK_THROWS_AS(n["foo"], yk::json20::bad_json_access);
  CHECK_THROWS_AS(n[0], yk::json20::bad_json_access);

  a[1] = json::object();
  a[1]["foo"] = 42;
  CHECK(a.at(1).at("foo").as_signed_integer<signed>() == 42);
}

TEST_CASE("insert", "[json]")
{
  json a = json::object();
  a.insert("foo", json{});
  CHECK(a.at("foo").get_kind() == yk::json20::json_value_kind::null);
}

TEST_CASE("emplace", "[json]")
{
  json a = json::object();
  a.emplace("foo");
  CHECK(a.at("foo").get_kind() == yk::json20::json_value_kind::null);
}

TEST_CASE("object duplicate keys", "[json]")
{
  // duplicates written in source are always a mistake
  CHECK_THROWS_AS(json::object({{"a", 1}, {"a", 2}}), std::invalid_argument);
  CHECK_THROWS_AS((json{std::pair{"a", 1}, {"a", 2}}), std::invalid_argument);

  // a rejected assignment leaves the target untouched
  json a = json::object({{"x", 1}});
  CHECK_THROWS_AS((a = {std::pair{"a", 1}, {"a", 2}}), std::invalid_argument);
  CHECK(a.as_object().size() == 1);
  CHECK(a.at("x").as_signed_integer<int>() == 1);
}

TEST_CASE("try_as", "[json]")
{
  json a(42u);
  CHECK(a.try_as_unsigned_integer<unsigned>() == 42u);
  CHECK_FALSE(a.try_as_signed_integer<int>().has_value());
  CHECK_FALSE(a.try_as_boolean().has_value());

  json b(-42);
  CHECK(b.try_as_signed_integer<int>() == -42);
  CHECK_FALSE(b.try_as_unsigned_integer<unsigned>().has_value());

  json c(3.25);
  CHECK(c.try_as_floating_point<double>() == 3.25);
  CHECK_FALSE(c.try_as_signed_integer<int>().has_value());

  json d(true);
  CHECK(d.try_as_boolean() == true);

  // stored values are returned by pointer, std::get_if style
  json s("foo");
  REQUIRE(s.try_as_string() != nullptr);
  CHECK(*s.try_as_string() == "foo");
  CHECK(s.try_as_array() == nullptr);
  CHECK(s.try_as_object() == nullptr);
  CHECK(s.try_at(0) == nullptr);

  json arr = json::array({1, 2});
  REQUIRE(arr.try_as_array() != nullptr);
  CHECK(arr.try_as_array()->size() == 2);
  CHECK(arr.try_as_array() == &arr.as_array());  // no copy: same storage
  REQUIRE(arr.try_at(1) != nullptr);
  CHECK(arr.try_at(1)->as_signed_integer<int>() == 2);
  CHECK(arr.try_at(2) == nullptr);

  json obj = json::object({{"foo", 42}});
  REQUIRE(obj.try_as_object() != nullptr);
  CHECK(obj.try_as_object()->size() == 1);
  REQUIRE(obj.try_at("foo") != nullptr);
  CHECK(obj.try_at("foo")->as_signed_integer<int>() == 42);
  CHECK(obj.try_at("bar") == nullptr);
}

TEST_CASE("numeric conversion failure", "[json]")
{
  // kind matches but the value does not fit the requested type
  json a(300u);
  CHECK_THROWS_AS(a.as_unsigned_integer<std::uint8_t>(), std::invalid_argument);
  CHECK_THROWS_AS(a.as_unsigned_integer_unchecked<std::uint8_t>(), std::invalid_argument);
  CHECK_FALSE(a.try_as_unsigned_integer<std::uint8_t>().has_value());
  CHECK(a.try_as_unsigned_integer<unsigned>() == 300u);

  json b(-129);
  CHECK_THROWS_AS(b.as_signed_integer<std::int8_t>(), std::invalid_argument);
  CHECK_FALSE(b.try_as_signed_integer<std::int8_t>().has_value());
  CHECK(b.try_as_signed_integer<int>() == -129);

  // converting accessors may throw, so they must not be noexcept
  static_assert(!noexcept(a.as_unsigned_integer_unchecked<unsigned>()));
  static_assert(!noexcept(b.as_signed_integer_unchecked<int>()));
  static_assert(!noexcept(b.as_floating_point_unchecked<double>()));
  static_assert(!noexcept(b.as_boolean_unchecked()));
  // try_as reports failure through the optional instead
  static_assert(noexcept(a.try_as_unsigned_integer<unsigned>()));
}

namespace {

template<class charT, class T>
void check_integer_roundtrip(T x)
{
  yk::json20::basic_json<charT> j(x);
  if constexpr (std::is_signed_v<T>)
    CHECK(j.template as_signed_integer<T>() == x);
  else
    CHECK(j.template as_unsigned_integer<T>() == x);
}

template<class charT>
void check_integer_roundtrips()
{
  check_integer_roundtrip<charT>(std::numeric_limits<std::int8_t>::min());
  check_integer_roundtrip<charT>(std::numeric_limits<std::int8_t>::max());
  check_integer_roundtrip<charT>(std::numeric_limits<std::uint8_t>::max());
  check_integer_roundtrip<charT>(std::numeric_limits<std::int32_t>::min());
  check_integer_roundtrip<charT>(std::numeric_limits<std::uint32_t>::max());
  check_integer_roundtrip<charT>(std::numeric_limits<std::int64_t>::min());
  check_integer_roundtrip<charT>(std::numeric_limits<std::int64_t>::max());
  check_integer_roundtrip<charT>(std::numeric_limits<std::uint64_t>::max());
  check_integer_roundtrip<charT>(std::int64_t{0});
  check_integer_roundtrip<charT>(std::uint64_t{0});
  check_integer_roundtrip<charT>(std::int32_t{-1});
  check_integer_roundtrip<charT>(std::uint32_t{1000000});
}

}  // namespace

TEST_CASE("integer conversion", "[json]")
{
  using yk::json20::deserializer;
  using yk::json20::serializer;

  // self-implemented: constexpr and independent of the platform's from_chars / to_chars
  static_assert(serializer<int, char>::serialize(-1234) == "-1234");
  static_assert(serializer<int, char>::serialize(0) == "0");
  static_assert(serializer<unsigned long long, char16_t>::serialize(18446744073709551615ull) == u"18446744073709551615");
  static_assert(serializer<signed char, char32_t>::serialize(-128) == U"-128");
  static_assert(std::get<0>(deserializer<int, char8_t>::deserialize(u8"-2147483648").args) == -2147483647 - 1);
  static_assert(std::get<0>(deserializer<unsigned, wchar_t>::deserialize(L"4294967295").args) == 4294967295u);

  check_integer_roundtrips<char>();
  check_integer_roundtrips<wchar_t>();
  check_integer_roundtrips<char8_t>();
  check_integer_roundtrips<char16_t>();
  check_integer_roundtrips<char32_t>();

  CHECK(yk::json20::wjson_parser::parse(L"-42").as_signed_integer<int>() == -42);
  CHECK(yk::json20::u32json_parser::parse(U"[18446744073709551615]").at(0).as_unsigned_integer<std::uint64_t>() == 18446744073709551615ull);

  // conversion stops at the first non-digit, like from_chars
  {
    std::string_view const s = "12.5";
    auto const r = deserializer<int, char>::deserialize(s);
    CHECK(std::get<0>(r.args) == 12);
    CHECK(std::to_address(r.it) == s.data() + 2);
  }

  // out of range or no digits at all
  CHECK_THROWS_AS((deserializer<std::int64_t, char>::deserialize("9223372036854775808")), std::invalid_argument);
  CHECK_THROWS_AS((deserializer<std::int64_t, char>::deserialize("-9223372036854775809")), std::invalid_argument);
  CHECK_THROWS_AS((deserializer<std::uint64_t, char>::deserialize("18446744073709551616")), std::invalid_argument);
  CHECK_THROWS_AS((deserializer<std::uint8_t, char16_t>::deserialize(u"256")), std::invalid_argument);
  CHECK_THROWS_AS((deserializer<unsigned, char>::deserialize("-1")), std::invalid_argument);
  CHECK_THROWS_AS((deserializer<int, char>::deserialize("-")), std::invalid_argument);
  CHECK_THROWS_AS((deserializer<int, char>::deserialize("")), std::invalid_argument);
}

TEST_CASE("boolean conversion", "[json]")
{
  using yk::json20::deserializer;
  using yk::json20::serializer;

  // every character type, like the other scalars
  static_assert(serializer<bool, char>::serialize(true) == "true");
  static_assert(serializer<bool, char8_t>::serialize(false) == u8"false");
  static_assert(std::get<0>(deserializer<bool, char16_t>::deserialize(u"true").args) == true);
  static_assert(std::get<0>(deserializer<bool, char32_t>::deserialize(U"false").args) == false);

  CHECK(yk::json20::wjson(true).as_boolean() == true);
  CHECK(yk::json20::u8json(false).as_string_unchecked() == u8"false");
  CHECK(yk::json20::u16json_parser::parse(u"[true, false]").at(1).as_boolean() == false);
  CHECK(yk::json20::u32json_parser::parse(U"{\"k\": true}").at(U"k").try_as_boolean() == true);

  CHECK_THROWS_AS((deserializer<bool, char16_t>::deserialize(u"null")), std::invalid_argument);
}

TEST_CASE("number roundtrip", "[json]")
{
  {
    json a(std::numeric_limits<std::int64_t>::min());
    CHECK(a.as_signed_integer<std::int64_t>() == std::numeric_limits<std::int64_t>::min());
  }
  {
    json a(std::numeric_limits<std::uint64_t>::max());
    CHECK(a.as_unsigned_integer<std::uint64_t>() == std::numeric_limits<std::uint64_t>::max());
  }
  {
    json a(std::numeric_limits<double>::max());
    CHECK(a.as_floating_point<double>() == std::numeric_limits<double>::max());
  }
  {
    json a(0.1);
    CHECK(a.as_floating_point<double>() == 0.1);
    CHECK(a.as_string_unchecked() == "0.1");
  }
  {
    json a(42u);
    CHECK(a.as_string_unchecked() == "42");
  }
}

TEST_CASE("erase", "[json]")
{
  json a = json::object({{"foo", 42}, {"bar", 3.14}});
  CHECK(a.at("foo").as_signed_integer<signed>() == 42);
  CHECK(a.at("bar").as_floating_point<double>() == 3.14);

  CHECK(a.erase("foo"));
  CHECK(a.try_at("foo") == nullptr);
  CHECK(a.as_object().size() == 1);
  CHECK(a.at("bar").as_floating_point<double>() == 3.14);

  // erasing a missing key is a no-op
  CHECK_FALSE(a.erase("baz"));
  CHECK(a.as_object().size() == 1);
  CHECK(a.at("bar").as_floating_point<double>() == 3.14);
}
