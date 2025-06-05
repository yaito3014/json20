#include <yk/json20.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(parse)

using json_parser = yk::json20::basic_json_parser<char>;

BOOST_AUTO_TEST_CASE(null)
{
  {
    const auto x = json_parser::parse("null");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::null));
  }
}

BOOST_AUTO_TEST_CASE(boolean)
{
  {
    const auto x = json_parser::parse("true");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
  {
    const auto x = json_parser::parse("false");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
}

BOOST_AUTO_TEST_CASE(number_unsigned_integer)
{
  {
    const auto x = json_parser::parse("0");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
    BOOST_ASSERT(x.as_unsigned_integer<unsigned>().value() == 0);
  }
  {
    const auto x = json_parser::parse("1234");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
    BOOST_ASSERT(x.as_unsigned_integer<unsigned>().value() == 1234);
  }
}

BOOST_AUTO_TEST_CASE(number_signed_integer)
{
  {
    const auto x = json_parser::parse("-1234");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_signed_integer));
    BOOST_ASSERT(x.as_signed_integer<int>().value() == -1234);
  }
}

BOOST_AUTO_TEST_CASE(number_floating_point)
{
  {
    const auto x = json_parser::parse("1234e5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.as_floating_point<double>().value() == 1234e5);
  }
  {
    const auto x = json_parser::parse("1234e+5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.as_floating_point<double>().value() == 1234e+5);
  }
  {
    const auto x = json_parser::parse("1234e-5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.as_floating_point<double>().value() == 1234e-5);
  }
  {
    const auto x = json_parser::parse("12.34");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.as_floating_point<double>().value() == 12.34);
  }
  {
    const auto x = json_parser::parse("12.34e5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.as_floating_point<double>().value() == 12.34e5);
  }
  {
    const auto x = json_parser::parse("12.34e+5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.as_floating_point<double>().value() == 12.34e+5);
  }
  {
    const auto x = json_parser::parse("12.34e-5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.as_floating_point<double>().value() == 12.34e-5);
  }
}

BOOST_AUTO_TEST_CASE(array)
{
  {
    const auto x = json_parser::parse("[]");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::array));
    BOOST_ASSERT(x.as_array().value().empty());
  }
  {
    const auto x = json_parser::parse("[ ]");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::array));
    BOOST_ASSERT(x.as_array().value().empty());
  }
  {
    const auto x = json_parser::parse("[12]");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::array));
    BOOST_ASSERT(x.get(0).value().as_unsigned_integer<unsigned>().value() == 12);
  }
}

BOOST_AUTO_TEST_CASE(string)
{
  {
    const auto x = json_parser::parse("\"foo\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "foo");
  }
  {
    const auto x = json_parser::parse("\"\\\\\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "\\");
  }
  {
    const auto x = json_parser::parse("\"\\\"\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "\"");
  }
  {
    const auto x = json_parser::parse("\"\\/\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "/");
  }
  {
    const auto x = json_parser::parse("\"\\/\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "/");
  }
  {
    const auto x = json_parser::parse("\"\\b\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "\b");
  }
  {
    const auto x = json_parser::parse("\"\\f\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "\f");
  }
  {
    const auto x = json_parser::parse("\"\\n\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "\n");
  }
  {
    const auto x = json_parser::parse("\"\\r\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "\r");
  }
  {
    const auto x = json_parser::parse("\"\\t\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.as_string().value() == "\t");
  }
}

BOOST_AUTO_TEST_CASE(object)
{
  {
    const auto x = json_parser::parse("{}");
    BOOST_TEST((x.get_kind() == yk::json20::json_value_kind::object));
  }
  {
    const auto x = json_parser::parse("{\"foo\":1234}");
    BOOST_TEST((x.get_kind() == yk::json20::json_value_kind::object));
    BOOST_TEST(x.get("foo").value().as_unsigned_integer<unsigned>().value() == 1234);
  }
}

BOOST_AUTO_TEST_CASE(checked_string)
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

BOOST_AUTO_TEST_SUITE_END()
