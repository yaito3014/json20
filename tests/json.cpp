#include <yk/json20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>

using json = yk::json20::basic_json<char>;

TEST_CASE("ctor", "[json]")
{
  {
    json a;
    CHECK(a.as_object().empty());
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
  CHECK(a.as_object().empty());
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
}

TEST_CASE("subscript", "[json]")
{
  json a = json::array({});
  a[0] = 42;
  CHECK(a.at(0).as_signed_integer<signed>() == 42);
  a[1]["foo"] = 42;
  CHECK(a.at(1).at("foo").as_signed_integer<signed>() == 42);
}

TEST_CASE("insert", "[json]")
{
  json a;
  a.insert("foo", json{});
  CHECK(a.at("foo").as_object().empty());
}

TEST_CASE("emplace", "[json]")
{
  json a;
  a.emplace("foo");
  CHECK(a.at("foo").as_object().empty());
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
