#ifndef YK_ZZ_JSON20_CHECKED_STRING_HPP
#define YK_ZZ_JSON20_CHECKED_STRING_HPP

#include <yk/json20/parser.hpp>

#include <concepts>
#include <string_view>

namespace yk {

namespace json20 {

template<class charT>
struct basic_noop_visitor {
  constexpr void on_null(std::basic_string_view<charT>) noexcept {}
  constexpr void on_boolean(std::basic_string_view<charT>) noexcept {}
  constexpr void on_number_unsigned_integer(std::basic_string_view<charT>) noexcept {}
  constexpr void on_number_signed_integer(std::basic_string_view<charT>) noexcept {}
  constexpr void on_number_floating_point(std::basic_string_view<charT>) noexcept {}
  constexpr void on_string(std::basic_string_view<charT>) noexcept {}

  constexpr void on_array_start() noexcept {}
  constexpr void on_array_finalize() noexcept {}
  constexpr void on_array_abort() noexcept {}

  constexpr void on_object_start() noexcept {}
  constexpr void on_object_finalize() noexcept {}
  constexpr void on_object_abort() noexcept {}
};

// Guarantees that the literal is RFC 8259 JSON syntax. Duplicate keys are
// syntactically valid, so they are not checked here; building a DOM decides
// about them (see parse_options).
template<class charT>
class basic_checked_string {
public:
  template<class S>
    requires std::convertible_to<S, std::basic_string_view<charT>>
  consteval basic_checked_string(S const& str) : str_(str)
  {
    basic_noop_visitor<charT> vis;
    basic_json_parser<charT>::parse(vis, str);
  }

  constexpr std::basic_string_view<charT> get() const noexcept { return str_; }

private:
  std::basic_string_view<charT> str_;
};

using checked_string = basic_checked_string<char>;
using wchecked_string = basic_checked_string<wchar_t>;
using u8checked_string = basic_checked_string<char8_t>;
using u16checked_string = basic_checked_string<char16_t>;
using u32checked_string = basic_checked_string<char32_t>;

}  // namespace json20

}  // namespace yk

#endif  // YK_ZZ_JSON20_CHECKED_STRING_HPP
