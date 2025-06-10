#include <yk/json20.hpp>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(json)

using json = yk::json20::basic_json<char>;

BOOST_AUTO_TEST_CASE(ctor)
{
  {
    json a;
    BOOST_TEST(a.as_object().empty());
  }
  {
    json a(42u);
    BOOST_TEST(a.as_unsigned_integer<unsigned>() == 42u);
  }
  {
    json a(42);
    BOOST_TEST(a.as_signed_integer<signed>() == 42);
  }
  {
    json a(3.14);
    BOOST_TEST(a.as_floating_point<double>() == 3.14);
  }
  {
    json a{{"foo", 42}, {"bar", 3.14}};
    BOOST_TEST(a.at("foo").as_signed_integer<signed>() == 42);
    BOOST_TEST(a.at("bar").as_floating_point<double>() == 3.14);
  }
}

BOOST_AUTO_TEST_CASE(insert)
{
  json a;
  a.insert("foo", json{});
  BOOST_TEST(a.at("foo").as_object().empty());
}

BOOST_AUTO_TEST_CASE(emplace)
{
  json a;
  a.emplace("foo");
  BOOST_TEST(a.at("foo").as_object().empty());
}

BOOST_AUTO_TEST_SUITE_END()
