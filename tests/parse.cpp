#include <yk/json20.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(parse)

BOOST_AUTO_TEST_CASE(null)
{
  {
    using json = yk::json20::basic_json<char>;
    const auto x = json::parse("null");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::null));
  }
}

BOOST_AUTO_TEST_CASE(boolean)
{
  {
    using json = yk::json20::basic_json<char>;
    const auto x = json::parse("true");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
  {
    using json = yk::json20::basic_json<char>;
    const auto x = json::parse("false");
    BOOST_ASSERT((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
}

BOOST_AUTO_TEST_SUITE_END()
