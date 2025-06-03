#include <yk/json20.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(parse)

using json = yk::json20::basic_json<char>;

BOOST_AUTO_TEST_CASE(null)
{
  {
    const auto x = json::parse("null");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::null));
  }
}

BOOST_AUTO_TEST_CASE(boolean)
{
  {
    const auto x = json::parse("true");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
  {
    const auto x = json::parse("false");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
}

BOOST_AUTO_TEST_CASE(number_unsigned_integer)
{
  {
    const auto x = json::parse("0");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
  }
  {
    const auto x = json::parse("1234");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
  }
  {
    const auto x = json::parse("1234e5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
  }
  {
    const auto x = json::parse("1234e+5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
  }
}

BOOST_AUTO_TEST_CASE(number_signed_integer)
{
  {
    const auto x = json::parse("-1234");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_signed_integer));
  }
  {
    const auto x = json::parse("-1234e5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_signed_integer));
  }
  {
    const auto x = json::parse("-1234e+5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_signed_integer));
  }
}

BOOST_AUTO_TEST_CASE(number_floating_point)
{
  {
    const auto x = json::parse("1234e-5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
  }
  {
    const auto x = json::parse("12.34");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
  }
  {
    const auto x = json::parse("12.34e5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
  }
  {
    const auto x = json::parse("12.34e+5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
  }
  {
    const auto x = json::parse("12.34e-5");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
  }
}

BOOST_AUTO_TEST_SUITE_END()
