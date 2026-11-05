#ifndef YK_ZZ_JSON20_DETAIL_SELECT_STR_HPP
#define YK_ZZ_JSON20_DETAIL_SELECT_STR_HPP

#include <yk/json20/detail/fixed_string.hpp>

#define YK_JSON20_WIDEN_STRING(charT, strLiteral) \
  ::yk::json20::detail::select_str<charT, strLiteral, L##strLiteral, u8##strLiteral, u##strLiteral, U##strLiteral>::value

namespace yk::json20::detail {

template<class charT, basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str {};

template<basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<char, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = Str;
};

template<basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<wchar_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = WStr;
};

template<basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<char8_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U8Str;
};

template<basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<char16_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U16Str;
};

template<basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<char32_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U32Str;
};

}  // namespace yk::json20::detail

#endif  // YK_ZZ_JSON20_DETAIL_SELECT_STR_HPP
