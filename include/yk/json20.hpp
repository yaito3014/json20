#ifndef YK_JSON20_HPP
#define YK_JSON20_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <map>
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

template <class charT, class Tuple>
struct deserialize_result {
  typename std::basic_string_view<charT>::iterator it;
  Tuple args;
};

template <class charT, class... Args>
constexpr auto make_deserialize_result(typename std::basic_string_view<charT>::iterator it, Args&&... args) noexcept
{
  return deserialize_result<charT, std::tuple<Args...>>{it, std::forward_as_tuple(std::forward<Args>(args)...)};
}

template <class T, class charT = char>
struct deserializer {
  constexpr auto deserialize(std::basic_string_view<charT> str) const = delete;
};

template <class T, class charT>
  requires std::integral<T> || std::floating_point<T>
struct deserializer<T, charT> {
  constexpr auto deserialize(std::basic_string_view<charT> str) const
  {
    T value{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc{}) throw std::invalid_argument("from_chars error");
    return make_deserialize_result<charT>(str.begin() + (ptr - str.data()), std::move(value));
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
  struct json_parse_result {
    std::optional<basic_json> value;
    std::basic_string_view<charT> rest;

    constexpr explicit operator bool() const noexcept { return value.has_value() && rest.empty(); }
  };

private:
  struct parse_result {
    std::optional<std::basic_string_view<charT>> match;
    std::basic_string_view<charT> rest;

    constexpr explicit operator bool() const noexcept { return match && rest.empty(); }
  };

  static constexpr auto lit(std::basic_string_view<charT> literal) noexcept
  {
    return [literal](std::basic_string_view<charT> str) -> parse_result {
      if (str.starts_with(literal)) {
        return {
            str.substr(0, literal.size()),
            str.substr(literal.size()),
        };
      } else {
        return {
            std::nullopt,
            str,
        };
      }
    };
  }

  template <class Parser1, class Parser2>
  static constexpr auto alt(const Parser1& p1, const Parser2& p2) noexcept
  {
    return [p1, p2](std::basic_string_view<charT> str) -> parse_result {
      if (auto res = p1(str); res.match) return res;
      if (auto res = p2(str); res.match) return res;
      return {
          std::nullopt,
          str,
      };
    };
  }

  template <class Parser1, class Parser2, class Parser3, class... Parsers>
  static constexpr auto alt(const Parser1& p1, const Parser2& p2, const Parser3& p3, const Parsers&... parsers) noexcept
  {
    return alt(p1, alt(p2, p3, parsers...));
  }

  template <class Parser1, class Parser2>
  static constexpr auto seq(const Parser1& p1, const Parser2& p2) noexcept
  {
    return [p1, p2](std::basic_string_view<charT> str) -> parse_result {
      if (auto res1 = p1(str); res1.match) {
        if (auto res2 = p2(res1.rest); res2.match) {
          return {
              std::basic_string_view<charT>{str.begin(), res2.match->end()},
              res2.rest,
          };
        }
      }
      return {
          std::nullopt,
          str,
      };
    };
  }

  template <class Parser1, class Parser2, class Parser3, class... Parsers>
  static constexpr auto seq(const Parser1& p1, const Parser2& p2, const Parser3& p3, const Parsers&... parsers) noexcept
  {
    return seq(p1, seq(p2, p3, parsers...));
  }

  template <class Parser>
  static constexpr auto opt(const Parser& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result {
      if (auto res = p(str); res.match) return res;
      return {
          str.substr(0, 0),
          str,
      };
    };
  }

  template <class Parser>
  static constexpr auto many(const Parser& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result {
      parse_result res = p(str);
      while (res.match) {
        res = p(res.rest);
      }
      return {
          std::basic_string_view<charT>{str.begin(), res.rest.begin()},
          res.rest,
      };
    };
  }

  template <class Parser>
  static constexpr auto many1(const Parser& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result {
      if (parse_result res = p(str); res.match) {
        while (res.match) {
          res = p(res.rest);
        }
        return {
            std::basic_string_view<charT>{str.begin(), res.rest.begin()},
            res.rest,
        };
      } else {
        return res;
      }
    };
  }

  static constexpr json_parse_result parse_null(std::basic_string_view<charT> str) noexcept
  {
    constexpr auto null_string = YK_JSON20_WIDEN_STRING(charT, "null");

    if (auto res = lit(null_string.get())(str); res.match) {
      return {
          basic_json{
              json_value_kind::null,
              std::in_place_index<0>,
              res.match->begin(),
              res.match->end(),
          },
          res.rest,
      };
    } else {
      return {
          std::nullopt,
          res.rest,
      };
    }
  }

  static constexpr json_parse_result parse_boolean(std::basic_string_view<charT> str) noexcept
  {
    constexpr auto true_string = YK_JSON20_WIDEN_STRING(charT, "true");
    constexpr auto false_string = YK_JSON20_WIDEN_STRING(charT, "false");

    if (auto res = alt(lit(true_string.get()), lit(false_string.get()))(str); res.match) {
      return {
          basic_json{
              json_value_kind::boolean,
              std::in_place_index<0>,
              res.match->begin(),
              res.match->end(),
          },
          res.rest,
      };
    } else {
      return {
          std::nullopt,
          res.rest,
      };
    }
  }

  static constexpr json_parse_result parse_number(std::basic_string_view<charT> str) noexcept
  {
    const auto parse0 = lit(YK_JSON20_WIDEN_STRING(charT, "0").get());
    const auto parse1to9 =
        alt(                                                //
            lit(YK_JSON20_WIDEN_STRING(charT, "1").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "2").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "3").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "4").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "5").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "6").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "7").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "8").get()),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "9").get())   //
        );
    const auto parse0to9 = alt(parse0, parse1to9);

    const auto parse_minus = lit(YK_JSON20_WIDEN_STRING(charT, "-").get());
    const auto parse_plus = lit(YK_JSON20_WIDEN_STRING(charT, "+").get());
    const auto parse_sign = alt(parse_minus, parse_plus);

    const auto parse_dot = lit(YK_JSON20_WIDEN_STRING(charT, ".").get());
    const auto parse_e =
        alt(lit(YK_JSON20_WIDEN_STRING(charT, "e").get()), lit(YK_JSON20_WIDEN_STRING(charT, "E").get()));

    const auto parse_frac = seq(parse_dot, many(parse0to9));
    const auto parse_exp = seq(parse_e, opt(parse_sign), many(parse0to9));

    const auto res1 = parse_minus(str);
    const bool has_minus_sign = bool(res1.match);
    const auto int_frac_exp = res1.rest;

    const auto parser = alt(parse0, seq(parse1to9, many(parse0to9)));
    if (auto res2 = parser(int_frac_exp); res2.match) {
      const auto frac_exp = res2.rest;

      auto res3 = parse_frac(frac_exp);
      const bool has_frac = bool(res3.match);
      const auto exp = res3.rest;

      auto res4 = parse_exp(exp);
      const bool has_exp = bool(res4.match);
      const auto rest = res4.rest;

      return {
          basic_json{
              has_frac || has_exp ? json_value_kind::number_floating_point
              : has_minus_sign    ? json_value_kind::number_signed_integer
                                  : json_value_kind::number_unsigned_integer,
              std::in_place_index<0>,
              str.begin(),
              rest.begin(),
          },
          rest,
      };

    } else {
      return {
          std::nullopt,
          res2.rest,
      };
    }
  }

public:
  static constexpr basic_json parse(std::basic_string_view<charT> str)
  {
    if (auto res = parse_null(str)) return *std::move(res).value;
    if (auto res = parse_boolean(str)) return *std::move(res).value;
    if (auto res = parse_number(str)) return *std::move(res).value;
    throw std::invalid_argument("invalid JSON");
  }

  template <class T>
  constexpr std::optional<T> get_unsigned_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_unsigned_integer) return std::nullopt;
    const deserializer<T, charT> de;
    return std::make_from_tuple<T>(de.deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr std::optional<T> get_signed_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_signed_integer) return std::nullopt;
    const deserializer<T, charT> de;
    return std::make_from_tuple<T>(de.deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr std::optional<T> get_floating_point() const noexcept
  {
    if (get_kind() != json_value_kind::number_floating_point) return std::nullopt;
    const deserializer<T, charT> de;
    return std::make_from_tuple<T>(de.deserialize(std::get<0>(data_)).args);
  }

  constexpr json_value_kind get_kind() const noexcept { return kind_; }

private:
  template <class... Args>
  constexpr basic_json(json_value_kind kind, Args&&... args) : kind_(kind), data_(std::forward<Args>(args)...)
  {
  }

  json_value_kind kind_;
  std::variant<
      std::basic_string<charT>,                       //
      std::vector<basic_json>,                        //
      std::map<std::basic_string<charT>, basic_json>  //
      >
      data_;
};

}  // namespace json20

}  // namespace yk

#endif  // YK_JSON20_HPP
