#ifndef YK_JSON20_HPP
#define YK_JSON20_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#define YK_JSON20_WIDEN_CHAR(charT, charLiteral) \
  ::yk::json20::detail::select_char<             \
      charT, charLiteral, L##charLiteral, u8##charLiteral, u##charLiteral, U##charLiteral>::value

#define YK_JSON20_WIDEN_STRING(charT, strLiteral) \
  ::yk::json20::detail::select_str<               \
      charT, strLiteral, L##strLiteral, u8##strLiteral, u##strLiteral, U##strLiteral>::value

namespace yk {

namespace json20 {

namespace detail {

template <class charT, char Char, wchar_t WChar, char8_t Char8, char16_t Char16, char32_t Char32>
struct select_char {};

template <char Char, wchar_t WChar, char8_t Char8, char16_t Char16, char32_t Char32>
struct select_char<char, Char, WChar, Char8, Char16, Char32> {
  static constexpr char value = Char;
};

template <char Char, wchar_t WChar, char8_t Char8, char16_t Char16, char32_t Char32>
struct select_char<wchar_t, Char, WChar, Char8, Char16, Char32> {
  static constexpr wchar_t value = WChar;
};
template <char Char, wchar_t WChar, char8_t Char8, char16_t Char16, char32_t Char32>
struct select_char<char8_t, Char, WChar, Char8, Char16, Char32> {
  static constexpr char8_t value = Char8;
};

template <char Char, wchar_t WChar, char8_t Char8, char16_t Char16, char32_t Char32>
struct select_char<char16_t, Char, WChar, Char8, Char16, Char32> {
  static constexpr char16_t value = Char16;
};

template <char Char, wchar_t WChar, char8_t Char8, char16_t Char16, char32_t Char32>
struct select_char<char32_t, Char, WChar, Char8, Char16, Char32> {
  static constexpr char32_t value = Char32;
};

template <class charT, std::size_t N>
struct basic_fixed_string {
  using storage_type = std::array<charT, N + 1>;
  storage_type data{};

  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;
  using size_type = typename storage_type::size_type;

  constexpr basic_fixed_string(const charT (&str)[N + 1]) noexcept { std::ranges::copy(str, data.begin()); }

  constexpr iterator begin() noexcept { return data.begin(); }
  constexpr const_iterator begin() const noexcept { return data.begin(); }
  constexpr iterator end() noexcept { return data.end() - 1; }
  constexpr const_iterator end() const noexcept { return data.end() - 1; }

  constexpr std::basic_string_view<charT> get() const noexcept { return {begin(), end()}; }
  constexpr size_type size() const noexcept { return N; }

  friend constexpr bool operator==(std::basic_string_view<charT> a, basic_fixed_string b) noexcept
  {
    return a == b.get();
  }
};

template <class charT, std::size_t N>
basic_fixed_string(const charT (&)[N]) -> basic_fixed_string<charT, N - 1>;

template <
    class charT, basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str,
    basic_fixed_string U32Str>
struct select_str {};

template <
    basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str,
    basic_fixed_string U32Str>
struct select_str<char, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = Str;
};

template <
    basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str,
    basic_fixed_string U32Str>
struct select_str<wchar_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = WStr;
};

template <
    basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str,
    basic_fixed_string U32Str>
struct select_str<char8_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U8Str;
};

template <
    basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str,
    basic_fixed_string U32Str>
struct select_str<char16_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U16Str;
};

template <
    basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str,
    basic_fixed_string U32Str>
struct select_str<char32_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U32Str;
};

}  // namespace detail

template <class T, class charT = char>
struct deserializer {
  constexpr auto deserialize(T& ref, std::basic_string_view<charT> str) = delete;
};

template <std::integral T, class charT>
struct deserializer<T, charT> {
  constexpr auto deserialize(T& ref, std::basic_string_view<charT> str)
  {
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), ref);
    if (ec != std::errc{}) throw std::invalid_argument("from_chars error");
    return str.begin() + (ptr - str.data());
  }
};

enum class json_value_kind {
  null,
  boolean,
  string,
  number_signed_integer,
  number_unsigned_integer,
  number_floating_point,
  array,
  object,
};

template <class charT>
class basic_json {
public:
  struct parse_result {
    std::optional<basic_json> value;
    std::basic_string_view<charT> rest;

    constexpr explicit operator bool() const noexcept { return value.has_value() && rest.empty(); }
  };

  static constexpr parse_result parse_null(std::basic_string_view<charT> str)
  {
    constexpr auto null_string = YK_JSON20_WIDEN_STRING(charT, "null");
    if (str.starts_with(null_string.get())) {
      return {
          basic_json{json_value_kind::null, std::in_place_index<0>, null_string.begin(), null_string.end()},
          str.substr(null_string.size()),
      };
    } else {
      return {
          std::nullopt,
          str,
      };
    }
  }

  static constexpr parse_result parse_boolean(std::basic_string_view<charT> str)
  {
    constexpr auto true_string = YK_JSON20_WIDEN_STRING(charT, "true");
    constexpr auto false_string = YK_JSON20_WIDEN_STRING(charT, "false");
    if (str.starts_with(true_string.get())) {
      return {
          basic_json{json_value_kind::boolean, std::in_place_index<0>, true_string.begin(), true_string.end()},
          str.substr(true_string.size()),
      };
    } else if (str.starts_with(false_string.get())) {
      return {
          basic_json{json_value_kind::boolean, std::in_place_index<0>, false_string.begin(), false_string.end()},
          str.substr(false_string.size()),
      };
    } else {
      return {
          std::nullopt,
          str,
      };
    }
  }

  static constexpr basic_json parse(std::basic_string_view<charT> str)
  {
    if (auto res = parse_null(str)) return *std::move(res).value;
    if (auto res = parse_boolean(str)) return *std::move(res).value;
    return basic_json{json_value_kind::object, std::in_place_index<0>, str.begin(), str.end()};
  }

  constexpr json_value_kind get_kind() const noexcept { return kind_; }

private:
  template <class... Args>
  constexpr basic_json(json_value_kind kind, Args&&... args) : kind_(kind), data_(std::forward<Args>(args)...)
  {
  }

  json_value_kind kind_;
  std::variant<
      std::basic_string<charT>,                                        //
      std::vector<std::unique_ptr<basic_json>>,                        //
      std::map<std::basic_string<charT>, std::unique_ptr<basic_json>>  //
      >
      data_;
};

}  // namespace json20

}  // namespace yk

#endif  // YK_JSON20_HPP
