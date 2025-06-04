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
    BOOST_ASSERT(x.get_unsigned_integer<unsigned>().value() == 0);
  }
  {
    const auto x = json_parser::parse("1234");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
    BOOST_ASSERT(x.get_unsigned_integer<unsigned>().value() == 1234);
  }
}

BOOST_AUTO_TEST_CASE(number_signed_integer)
{
  {
    const auto x = json_parser::parse("-1234");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_signed_integer));
    BOOST_ASSERT(x.get_signed_integer<int>().value() == -1234);
  }
}

BOOST_AUTO_TEST_CASE(number_floating_point)
{
  {
    const auto x = json_parser::parse("1234e5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.get_floating_point<double>().value() == 1234e5);
  }
  {
    const auto x = json_parser::parse("1234e+5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.get_floating_point<double>().value() == 1234e+5);
  }
  {
    const auto x = json_parser::parse("1234e-5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.get_floating_point<double>().value() == 1234e-5);
  }
  {
    const auto x = json_parser::parse("12.34");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.get_floating_point<double>().value() == 12.34);
  }
  {
    const auto x = json_parser::parse("12.34e5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.get_floating_point<double>().value() == 12.34e5);
  }
  {
    const auto x = json_parser::parse("12.34e+5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.get_floating_point<double>().value() == 12.34e+5);
  }
  {
    const auto x = json_parser::parse("12.34e-5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    BOOST_ASSERT(x.get_floating_point<double>().value() == 12.34e-5);
  }
}

BOOST_AUTO_TEST_CASE(array)
{
  {
    const auto x = json_parser::parse("[]");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::array));
    BOOST_ASSERT(x.get_array().value().empty());
  }
  {
    const auto x = json_parser::parse("[ ]");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::array));
    BOOST_ASSERT(x.get_array().value().empty());
  }
  {
    const auto x = json_parser::parse("[12]");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::array));
    BOOST_ASSERT(x.get_array().value()[0].get_unsigned_integer<unsigned>().value() == 12);
  }
}

BOOST_AUTO_TEST_CASE(string)
{
  {
    const auto x = json_parser::parse("\"foo\"");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::string));
    BOOST_ASSERT(x.get_string().value() == "foo");
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
  }
}

BOOST_AUTO_TEST_SUITE_END()
