#include <yk/json20.hpp>

#include <boost/test/unit_test.hpp>

#include <string_view>
#include <type_traits>

BOOST_AUTO_TEST_SUITE(util)

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
