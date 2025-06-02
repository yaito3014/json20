#include <yk/json20.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(parse)

BOOST_AUTO_TEST_CASE(null)
{
  {
    using json = yk::json20::basic_json<char>;
    const auto x = json::parse("null");
    BOOST_ASSERT((yk::json20::json_value_kind::null == x.get_kind()));
  }
}

BOOST_AUTO_TEST_SUITE_END()
