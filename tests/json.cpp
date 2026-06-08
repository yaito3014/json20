#include <yk/json20.hpp>

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("erase", "[json]")
{
  json a = json::object({{"foo", 42}, {"bar", 3.14}});
  CHECK(a.at("foo").as_signed_integer<signed>() == 42);
  CHECK(a.at("bar").as_floating_point<double>() == 3.14);
  a.erase("foo");
  CHECK(a.at("bar").as_floating_point<double>() == 3.14);
}
