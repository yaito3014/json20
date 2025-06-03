#include <yk/json20.hpp>

#include <boost/test/unit_test.hpp>

#include <string_view>
#include <type_traits>

BOOST_AUTO_TEST_SUITE(util)

BOOST_AUTO_TEST_CASE(widen_char)
{
  {
    constexpr auto c = YK_JSON20_WIDEN_CHAR(char, 'a');
    static_assert(std::is_same_v<decltype(c), const char>);
    static_assert(c == 'a');
  }
  {
    constexpr auto c = YK_JSON20_WIDEN_CHAR(wchar_t, 'a');
    static_assert(std::is_same_v<decltype(c), const wchar_t>);
    static_assert(c == L'a');
  }
  {
    constexpr auto c = YK_JSON20_WIDEN_CHAR(char8_t, 'a');
    static_assert(std::is_same_v<decltype(c), const char8_t>);
    static_assert(c == u8'a');
  }
  {
    constexpr auto c = YK_JSON20_WIDEN_CHAR(char16_t, 'a');
    static_assert(std::is_same_v<decltype(c), const char16_t>);
    static_assert(c == u'a');
  }
  {
    constexpr auto c = YK_JSON20_WIDEN_CHAR(char32_t, 'a');
    static_assert(std::is_same_v<decltype(c), const char32_t>);
    static_assert(c == U'a');
  }
}

template <class FixedString>
struct is_wfixed_string : std::false_type {};

template <std::size_t N>
struct is_wfixed_string<yk::json20::detail::basic_fixed_string<wchar_t, N>> : std::true_type {};

BOOST_AUTO_TEST_CASE(widen_string)
{
  {
    constexpr auto s = YK_JSON20_WIDEN_STRING(char, "foo");
    static_assert(std::is_same_v<decltype(s), const yk::json20::detail::basic_fixed_string<char, 3>>);
  }
  {
    constexpr auto s = YK_JSON20_WIDEN_STRING(wchar_t, "foo");
    // cannot assume wchar_t's encoding to have three code units
    static_assert(is_wfixed_string<std::remove_cv_t<decltype(s)>>::value);
  }
  {
    constexpr auto s = YK_JSON20_WIDEN_STRING(char8_t, "foo");
    static_assert(std::is_same_v<decltype(s), const yk::json20::detail::basic_fixed_string<char8_t, 3>>);
  }
  {
    constexpr auto s = YK_JSON20_WIDEN_STRING(char16_t, "foo");
    static_assert(std::is_same_v<decltype(s), const yk::json20::detail::basic_fixed_string<char16_t, 3>>);
  }
  {
    constexpr auto s = YK_JSON20_WIDEN_STRING(char32_t, "foo");
    static_assert(std::is_same_v<decltype(s), const yk::json20::detail::basic_fixed_string<char32_t, 3>>);
  }
}

BOOST_AUTO_TEST_SUITE_END()
