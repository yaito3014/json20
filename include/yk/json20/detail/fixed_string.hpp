#ifndef YK_ZZ_JSON20_DETAIL_FIXED_STRING_HPP
#define YK_ZZ_JSON20_DETAIL_FIXED_STRING_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#define YK_JSON20_WIDEN_STRING(charT, strLiteral) \
  ::yk::json20::detail::select_str<charT, strLiteral, L##strLiteral, u8##strLiteral, u##strLiteral, U##strLiteral>::value

namespace yk {

namespace json20 {

namespace detail {

template<class charT, std::size_t N>
struct basic_fixed_string {
  using storage_type = std::array<charT, N + 1>;
  storage_type data{};

  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;
  using size_type = typename storage_type::size_type;

  constexpr basic_fixed_string(charT const (&str)[N + 1]) noexcept { std::ranges::copy(str, data.begin()); }

  constexpr iterator begin() noexcept { return data.begin(); }
  constexpr const_iterator begin() const noexcept { return data.begin(); }
  constexpr iterator end() noexcept { return data.end() - 1; }
  constexpr const_iterator end() const noexcept { return data.end() - 1; }

  constexpr operator std::basic_string_view<charT>() const noexcept { return {begin(), end()}; }
  constexpr size_type size() const noexcept { return N; }
};

template<class charT, std::size_t N>
basic_fixed_string(charT const (&)[N]) -> basic_fixed_string<charT, N - 1>;

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

}  // namespace detail

}  // namespace json20

}  // namespace yk

#endif  // YK_ZZ_JSON20_DETAIL_FIXED_STRING_HPP
