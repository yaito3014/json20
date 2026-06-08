#include <yk/json20.hpp>

#include <catch2/catch_test_macros.hpp>

using json_parser = yk::json20::basic_json_parser<char>;

TEST_CASE("null", "[parse]")
{
  {
    const auto x = json_parser::parse("null");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::null));
  }
}

TEST_CASE("boolean", "[parse]")
{
  {
    const auto x = json_parser::parse("true");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
  {
    const auto x = json_parser::parse("false");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
}

TEST_CASE("number_unsigned_integer", "[parse]")
{
  {
    const auto x = json_parser::parse("0");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
    REQUIRE(x.as_unsigned_integer<unsigned>() == 0);
  }
  {
    const auto x = json_parser::parse("1234");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
    REQUIRE(x.as_unsigned_integer<unsigned>() == 1234);
  }
}

TEST_CASE("number_signed_integer", "[parse]")
{
  {
    const auto x = json_parser::parse("-1234");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_signed_integer));
    REQUIRE(x.as_signed_integer<int>() == -1234);
  }
}

TEST_CASE("number_floating_point", "[parse]")
{
  {
    const auto x = json_parser::parse("1234e5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 1234e5);
  }
  {
    const auto x = json_parser::parse("1234e+5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 1234e+5);
  }
  {
    const auto x = json_parser::parse("1234e-5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 1234e-5);
  }
  {
    const auto x = json_parser::parse("12.34");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34);
  }
  {
    const auto x = json_parser::parse("12.34e5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34e5);
  }
  {
    const auto x = json_parser::parse("12.34e+5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34e+5);
  }
  {
    const auto x = json_parser::parse("12.34e-5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34e-5);
  }
}

TEST_CASE("array", "[parse]")
{
  {
    const auto x = json_parser::parse("[]");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::array));
    REQUIRE(x.as_array().empty());
  }
  {
    const auto x = json_parser::parse("[ ]");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::array));
    REQUIRE(x.as_array().empty());
  }
  {
    const auto x = json_parser::parse("[12]");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::array));
    REQUIRE(x.at(0).as_unsigned_integer<unsigned>() == 12);
  }
}

TEST_CASE("string", "[parse]")
{
  {
    const auto x = json_parser::parse("\"foo\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "foo");
  }
  {
    const auto x = json_parser::parse("\"\\\\\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\\");
  }
  {
    const auto x = json_parser::parse("\"\\\"\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\"");
  }
  {
    const auto x = json_parser::parse("\"\\/\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "/");
  }
  {
    const auto x = json_parser::parse("\"\\/\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "/");
  }
  {
    const auto x = json_parser::parse("\"\\b\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\b");
  }
  {
    const auto x = json_parser::parse("\"\\f\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\f");
  }
  {
    const auto x = json_parser::parse("\"\\n\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\n");
  }
  {
    const auto x = json_parser::parse("\"\\r\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\r");
  }
  {
    const auto x = json_parser::parse("\"\\t\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\t");
  }
}

TEST_CASE("object", "[parse]")
{
  {
    const auto x = json_parser::parse("{}");
    CHECK((x.get_kind() == yk::json20::json_value_kind::object));
  }
  {
    const auto x = json_parser::parse("{\"foo\":1234}");
    CHECK((x.get_kind() == yk::json20::json_value_kind::object));
    CHECK(x.at("foo").as_unsigned_integer<unsigned>() == 1234);
  }
}

TEST_CASE("checked_string", "[parse]")
{
  const auto f = [](yk::json20::basic_checked_string<char>) {};
  f("null");
  f("true");
  f("false");
  f("1234");
  f("-1234");
  f("1234e5");
  f("1234e+5");
  f("1234e-5");
  f("12.34");
  f("-12.34");
  f("12.34e5");
  f("12.34e+5");
  f("12.34e-5");
  f("\"foo\"");
  f("{\"foo\":123}");
  f("[123,3.14]");
}
