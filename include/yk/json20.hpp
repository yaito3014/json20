#ifndef YK_JSON20_HPP
#define YK_JSON20_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#define YK_JSON20_WIDEN_STRING(charT, strLiteral) \
  ::yk::json20::detail::select_str<charT, strLiteral, L##strLiteral, u8##strLiteral, u##strLiteral, U##strLiteral>::value

namespace yk {

namespace json20 {

namespace detail {

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

  constexpr operator std::basic_string_view<charT>() const noexcept { return {begin(), end()}; }
  constexpr size_type size() const noexcept { return N; }
};

template <class charT, std::size_t N>
basic_fixed_string(const charT (&)[N]) -> basic_fixed_string<charT, N - 1>;

template <class charT, basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str {};

template <basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<char, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = Str;
};

template <basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<wchar_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = WStr;
};

template <basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<char8_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U8Str;
};

template <basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
struct select_str<char16_t, Str, WStr, U8Str, U16Str, U32Str> {
  static constexpr auto value = U16Str;
};

template <basic_fixed_string Str, basic_fixed_string WStr, basic_fixed_string U8Str, basic_fixed_string U16Str, basic_fixed_string U32Str>
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
  static constexpr auto deserialize(std::basic_string_view<charT> str) = delete;
};

template <class T, class charT>
  requires std::integral<T> || std::floating_point<T>
struct deserializer<T, charT> {
  static constexpr auto deserialize(std::basic_string_view<charT> str)
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
  template <class T>
  constexpr std::optional<T> get_unsigned_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_unsigned_integer) return std::nullopt;
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr std::optional<T> get_signed_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_signed_integer) return std::nullopt;
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr std::optional<T> get_floating_point() const noexcept
  {
    if (get_kind() != json_value_kind::number_floating_point) return std::nullopt;
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  constexpr std::optional<std::vector<basic_json<charT>>> get_array() const noexcept
  {
    if (get_kind() != json_value_kind::array) return std::nullopt;
    return std::get<1>(data_);
  }

  constexpr std::optional<std::basic_string<charT>> get_string() const noexcept
  {
    if (get_kind() != json_value_kind::string) return std::nullopt;
    return std::get<0>(data_);
  }

  constexpr json_value_kind get_kind() const noexcept { return kind_; }

private:
  struct private_construct_t {};
  static inline constexpr private_construct_t private_construct{};

public:
  template <class charT2>
  friend class basic_json_visitor;

  template <class... Args>
  constexpr basic_json(private_construct_t, json_value_kind kind, Args&&... args) : kind_(kind), data_(std::forward<Args>(args)...)
  {
  }

private:
  json_value_kind kind_;
  std::variant<
      std::basic_string<charT>,                       //
      std::vector<basic_json>,                        //
      std::map<std::basic_string<charT>, basic_json>  //
      >
      data_;
};

template <class charT>
class basic_json_visitor {
public:
  void on_null(std::basic_string_view<charT> str) noexcept
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::null, std::in_place_index<0>, str.begin(), str.end());
  }
  void on_boolean(std::basic_string_view<charT> str) noexcept
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::boolean, std::in_place_index<0>, str.begin(), str.end());
  }
  void on_number_unsigned_integer(std::basic_string_view<charT> str) noexcept
  {
    stack_.emplace_back(
        std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::number_unsigned_integer, std::in_place_index<0>, str.begin(), str.end()
    );
  }
  void on_number_signed_integer(std::basic_string_view<charT> str) noexcept
  {
    stack_.emplace_back(
        std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::number_signed_integer, std::in_place_index<0>, str.begin(), str.end()
    );
  }
  void on_number_floating_point(std::basic_string_view<charT> str) noexcept
  {
    stack_.emplace_back(
        std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::number_floating_point, std::in_place_index<0>, str.begin(), str.end()
    );
  }
  void on_string(std::basic_string_view<charT> str) noexcept
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::string, std::in_place_index<0>, str.begin(), str.end());
  }

  void on_array_start() noexcept { stack_.emplace_back(start_tag{}); }
  void on_array_finalize() noexcept
  {
    auto rng = std::ranges::find_last_if(stack_, [](const auto& var) { return std::holds_alternative<start_tag>(var); });
    std::vector<basic_json<charT>> vec;
    for (auto&& var : rng | std::views::drop(1)) {
      vec.emplace_back(std::get<basic_json<charT>>(std::move(var)));
    }
    stack_.erase(rng.begin(), rng.end());
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::array, std::in_place_index<1>, std::move(vec));
  }
  void on_array_abort() noexcept { stack_.pop_back(); }

  void on_object_start() noexcept { stack_.emplace_back(start_tag{}); }
  void on_object_finalize() noexcept
  {
    auto rng = std::ranges::find_last_if(stack_, [](const auto& var) { return std::holds_alternative<start_tag>(var); });
    std::map<std::basic_string<charT>, basic_json<charT>> map;
    for (std::size_t i = 1; i < rng.size(); i += 2) {
      map.emplace(std::get<basic_json<charT>>(rng[i]).get_string().value(), std::get<basic_json<charT>>(rng[i + 1]));
    }
    stack_.erase(rng.begin(), rng.end());
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::object, std::in_place_index<2>, std::move(map));
  }
  void on_object_abort() noexcept { stack_.pop_back(); }

  constexpr basic_json<charT> get() const noexcept { return std::get<basic_json<charT>>(stack_.back()); }

private:
  struct start_tag {};
  std::vector<std::variant<start_tag, basic_json<charT>>> stack_;
};

template <class charT>
class basic_json_parser {
public:
  template <class T = std::basic_string_view<charT>>
  struct parse_result {
    using value_type = T;
    std::optional<T> match;
    std::basic_string_view<charT> rest;

    constexpr explicit operator bool() const noexcept { return match.has_value() && rest.empty(); }
  };

private:
  struct lit_parser {
    std::basic_string_view<charT> literal;

    constexpr auto operator()(std::basic_string_view<charT> str) const noexcept -> parse_result<>
    {
      if (str.starts_with(literal)) {
        return {
            str.substr(0, literal.size()),
            str.substr(literal.size()),
        };
      } else {
        return {std::nullopt, str};
      }
    };
  };

  static constexpr auto lit(std::basic_string_view<charT> literal) noexcept { return lit_parser{literal}; }

  template <class Parser>
  static constexpr auto except(const Parser& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result<> {
      if (auto res = p(str); res.match)
        return {std::nullopt, str};
      else
        return {str.substr(0, 1), str.substr(1)};
    };
  }

  template <class T, class U>
  struct alt_result {
    using type = std::variant<T, U>;
  };
  template <class T>
  struct alt_result<T, T> {
    using type = T;
  };

  template <class Parser1, class Parser2>
  struct alt_parser {
    using T = typename std::invoke_result_t<Parser1, std::basic_string_view<charT>>::value_type;
    using U = typename std::invoke_result_t<Parser2, std::basic_string_view<charT>>::value_type;

    [[no_unique_address]] Parser1 p1;
    [[no_unique_address]] Parser2 p2;

    constexpr auto operator()(std::basic_string_view<charT> str) const noexcept -> parse_result<typename alt_result<T, U>::type>
    {
      if (auto res1 = p1(str); res1.match) return {*res1.match, res1.rest};
      if (auto res2 = p2(str); res2.match) return {*res2.match, res2.rest};
      return {std::nullopt, str};
    };
  };

  template <class Parser1, class Parser2>
  static constexpr auto alt(const Parser1& p1, const Parser2& p2) noexcept
  {
    return alt_parser{p1, p2};
  }

  template <class Parser1, class Parser2, class Parser3, class... Parsers>
  static constexpr auto alt(const Parser1& p1, const Parser2& p2, const Parser3& p3, const Parsers&... parsers) noexcept
  {
    return alt(p1, alt(p2, p3, parsers...));
  }

  template <class Parser1, class Parser2>
  struct seq_parser {
    using T = typename std::invoke_result_t<Parser1, std::basic_string_view<charT>>::value_type;
    using U = typename std::invoke_result_t<Parser2, std::basic_string_view<charT>>::value_type;

    [[no_unique_address]] Parser1 p1;
    [[no_unique_address]] Parser2 p2;

    constexpr auto operator()(std::basic_string_view<charT> str) const noexcept -> parse_result<std::tuple<T, U>>
    {
      if (auto res1 = p1(str); res1.match) {
        if (auto res2 = p2(res1.rest); res2.match) {
          return {
              std::make_tuple(*res1.match, *res2.match),
              res2.rest,
          };
        }
      }
      return {std::nullopt, str};
    };
  };

  template <class Parser1, class Parser2>
  static constexpr auto seq(const Parser1& p1, const Parser2& p2) noexcept
  {
    return seq_parser{p1, p2};
  }

  template <class Parser1, class Parser2, class Parser3, class... Parsers>
  static constexpr auto seq(const Parser1& p1, const Parser2& p2, const Parser3& p3, const Parsers&... parsers) noexcept
  {
    return seq(p1, seq(p2, p3, parsers...));
  }

  template <class Parser>
  static constexpr auto opt(const Parser& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result<> {
      if (auto res = p(str); res.match) return res;
      return {str.substr(0, 0), str};
    };
  }

  template <class Parser>
  static constexpr auto many(const Parser& p) noexcept
  {
    using T = typename std::invoke_result_t<Parser, std::basic_string_view<charT>>::value_type;
    return [p](std::basic_string_view<charT> str) -> parse_result<std::vector<T>> {
      std::vector<T> matches;
      parse_result<T> res = p(str);
      while (res.match) {
        matches.emplace_back(*std::move(res.match));
        res = p(res.rest);
      }
      return {std::move(matches), res.rest};
    };
  }

  template <class Parser, class Delim>
  static constexpr auto sep_by(const Parser& p, const Delim& d) noexcept
  {
    return [p, d](std::basic_string_view<charT> str) -> parse_result<> {
      if (auto res = seq(p, many(seq(d, p)))(str); res.match) {
        return {std::basic_string_view<charT>{str.begin(), res.rest.begin()}, res.rest};
      }
      return {std::nullopt, str};
    };
  }

  static constexpr parse_result<> ws(std::basic_string_view<charT> str) noexcept
  {
    const auto space = YK_JSON20_WIDEN_STRING(charT, " ");
    const auto lf = YK_JSON20_WIDEN_STRING(charT, "\n");
    const auto cr = YK_JSON20_WIDEN_STRING(charT, "\r");
    const auto tab = YK_JSON20_WIDEN_STRING(charT, "\t");

    const auto parser = many(alt(lit(space), lit(lf), lit(cr), lit(tab)));
    auto res = parser(str);
    return {std::basic_string_view<charT>{str.begin(), res.rest.begin()}, res.rest};
  }

  static constexpr parse_result<> cntrl(std::basic_string_view<charT> str) noexcept
  {
    if (std::iscntrl(str.front(), std::locale::classic())) {
      return {str.substr(0, 1), str.substr(1)};
    }
    return {std::nullopt, str};
  }

  template <class Visitor>
  static constexpr parse_result<> parse_null(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    constexpr auto null_string = YK_JSON20_WIDEN_STRING(charT, "null");

    auto res = lit(null_string)(str);
    if (res.match) {
      vis.on_null(*res.match);
    }
    return res;
  }

  template <class Visitor>
  static constexpr parse_result<> parse_boolean(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    constexpr auto true_string = YK_JSON20_WIDEN_STRING(charT, "true");
    constexpr auto false_string = YK_JSON20_WIDEN_STRING(charT, "false");

    auto res = alt(lit(true_string), lit(false_string))(str);
    if (res.match) {
      vis.on_boolean(*res.match);
    }
    return res;
  }

  template <class Visitor>
  static constexpr parse_result<> parse_number(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    const auto parse0 = lit(YK_JSON20_WIDEN_STRING(charT, "0"));
    const auto parse1to9 =
        alt(                                          //
            lit(YK_JSON20_WIDEN_STRING(charT, "1")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "2")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "3")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "4")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "5")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "6")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "7")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "8")),  //
            lit(YK_JSON20_WIDEN_STRING(charT, "9"))   //
        );
    const auto parse0to9 = alt(parse0, parse1to9);

    const auto parse_minus = lit(YK_JSON20_WIDEN_STRING(charT, "-"));
    const auto parse_plus = lit(YK_JSON20_WIDEN_STRING(charT, "+"));
    const auto parse_sign = alt(parse_minus, parse_plus);

    const auto parse_dot = lit(YK_JSON20_WIDEN_STRING(charT, "."));
    const auto parse_e = alt(lit(YK_JSON20_WIDEN_STRING(charT, "e")), lit(YK_JSON20_WIDEN_STRING(charT, "E")));

    const auto parse_frac = seq(parse_dot, many(parse0to9));
    const auto parse_exp = seq(parse_e, opt(parse_sign), many(parse0to9));

    const auto res1 = parse_minus(str);
    const bool has_minus_sign = bool(res1.match);
    const auto int_frac_exp = res1.rest;

    const auto parser = alt(parse0, seq(parse1to9, many(parse0to9)));

    auto res2 = parser(int_frac_exp);
    if (res2.match) {
      const auto frac_exp = res2.rest;

      auto res3 = parse_frac(frac_exp);
      const bool has_frac = bool(res3.match);
      const auto exp = res3.rest;

      auto res4 = parse_exp(exp);
      const bool has_exp = bool(res4.match);
      const auto rest = res4.rest;

      std::basic_string_view<charT> match{str.begin(), rest.begin()};

      if (has_frac || has_exp) {
        vis.on_number_floating_point(match);
      } else if (has_minus_sign) {
        vis.on_number_signed_integer(match);
      } else {
        vis.on_number_unsigned_integer(match);
      }

      return {match, rest};
    } else {
      return {std::nullopt, res2.rest};
    }
  }

  template <class Visitor>
  static constexpr parse_result<> parse_array(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    const auto open_bracket = YK_JSON20_WIDEN_STRING(charT, "[");
    const auto close_bracket = YK_JSON20_WIDEN_STRING(charT, "]");
    const auto comma = YK_JSON20_WIDEN_STRING(charT, ",");

    const auto parser = seq(lit(open_bracket), alt(sep_by(std::bind_front(parse_value<Visitor>, std::ref(vis)), lit(comma)), ws), lit(close_bracket));

    vis.on_array_start();
    auto res = parser(str);
    if (res.match) {
      vis.on_array_finalize();
      return {std::basic_string_view<charT>{str.begin(), res.rest.begin()}, res.rest};
    }
    vis.on_array_abort();
    return {std::nullopt, str};
  }

  template <class Visitor>
  static constexpr parse_result<> parse_string(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    const auto quote = YK_JSON20_WIDEN_STRING(charT, "\"");
    const auto backslash = YK_JSON20_WIDEN_STRING(charT, "\\");

    const auto hex = alt(
        lit(YK_JSON20_WIDEN_STRING(charT, "0")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "1")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "2")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "3")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "4")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "5")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "6")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "7")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "8")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "9")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "A")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "B")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "C")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "D")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "E")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "F")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "a")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "b")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "c")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "d")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "e")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "f"))   //
    );

    // TODO: implement unicode escape sequence
    const auto escape_sequence = [&](std::basic_string_view<charT> s) -> parse_result<std::basic_string<charT>> {
      if (!s.starts_with(backslash)) return {std::nullopt, s};
      auto sequence = s.substr(backslash.size());

      const auto slash = YK_JSON20_WIDEN_STRING(charT, "/");
      const auto str_b = YK_JSON20_WIDEN_STRING(charT, "b");
      const auto str_f = YK_JSON20_WIDEN_STRING(charT, "f");
      const auto str_n = YK_JSON20_WIDEN_STRING(charT, "n");
      const auto str_r = YK_JSON20_WIDEN_STRING(charT, "r");
      const auto str_t = YK_JSON20_WIDEN_STRING(charT, "t");
      const auto backspace = YK_JSON20_WIDEN_STRING(charT, "\b");
      const auto formfeed = YK_JSON20_WIDEN_STRING(charT, "\f");
      const auto linefeed = YK_JSON20_WIDEN_STRING(charT, "\n");
      const auto cr = YK_JSON20_WIDEN_STRING(charT, "\r");
      const auto tab = YK_JSON20_WIDEN_STRING(charT, "\t");

      if (sequence.starts_with(quote)) return {std::basic_string<charT>{quote}, sequence.substr(quote.size())};
      if (sequence.starts_with(backslash)) return {std::basic_string<charT>{backslash}, sequence.substr(backslash.size())};
      if (sequence.starts_with(slash)) return {std::basic_string<charT>{slash}, sequence.substr(slash.size())};
      if (sequence.starts_with(str_b)) return {std::basic_string<charT>{backspace}, sequence.substr(str_b.size())};
      if (sequence.starts_with(str_f)) return {std::basic_string<charT>{formfeed}, sequence.substr(str_f.size())};
      if (sequence.starts_with(str_n)) return {std::basic_string<charT>{linefeed}, sequence.substr(str_n.size())};
      if (sequence.starts_with(str_r)) return {std::basic_string<charT>{cr}, sequence.substr(str_r.size())};
      if (sequence.starts_with(str_t)) return {std::basic_string<charT>{tab}, sequence.substr(str_t.size())};
      return {std::nullopt, s};
    };

    const auto str_parser = [&](std::basic_string_view<charT> s) -> parse_result<std::basic_string<charT>> {
      const auto res = many(alt(except(alt(lit(quote), lit(backslash), cntrl)), escape_sequence))(s);
      if (!res.match) return {std::nullopt, s};
      std::basic_string<charT> result;
      for (auto& var : *res.match) {
        std::visit([&](auto&& str_like) { result.append(std::move(str_like)); }, std::move(var));
      }
      return {result, res.rest};
    };

    const auto parser = seq(lit(quote), str_parser, lit(quote));

    auto res = parser(str);
    if (res.match) {
      auto content = std::get<0>(std::get<1>(*res.match));
      vis.on_string(content);
      return {std::basic_string_view<charT>{str.begin(), res.rest.begin()}, res.rest};
    }
    return {std::nullopt, str};
  }

  template <class Visitor>
  static constexpr parse_result<> parse_object(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    const auto open_brace = YK_JSON20_WIDEN_STRING(charT, "{");
    const auto close_brace = YK_JSON20_WIDEN_STRING(charT, "}");
    const auto colon = YK_JSON20_WIDEN_STRING(charT, ":");
    const auto comma = YK_JSON20_WIDEN_STRING(charT, ",");

    const auto key_value_parser =
        seq(ws, std::bind_front(parse_string<Visitor>, std::ref(vis)), ws, lit(colon), std::bind_front(parse_value<Visitor>, std::ref(vis)));
    const auto parser = seq(lit(open_brace), alt(sep_by(key_value_parser, lit(comma)), ws), lit(close_brace));

    vis.on_object_start();
    auto res = parser(str);
    if (res.match) {
      vis.on_object_finalize();
      return {std::basic_string_view<charT>{str.begin(), res.rest.begin()}, res.rest};
    }
    vis.on_object_abort();
    return {std::nullopt, str};
  }

  template <class Visitor>
  static constexpr parse_result<> parse_value(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    const auto parser =
        seq(ws,
            alt(
                std::bind_front(parse_null<Visitor>, std::ref(vis)),     //
                std::bind_front(parse_boolean<Visitor>, std::ref(vis)),  //
                std::bind_front(parse_number<Visitor>, std::ref(vis)),   //
                std::bind_front(parse_string<Visitor>, std::ref(vis)),   //
                std::bind_front(parse_array<Visitor>, std::ref(vis)),    //
                std::bind_front(parse_object<Visitor>, std::ref(vis))    //
            ),
            ws);

    auto res = parser(str);
    if (res.match) {
      return {std::basic_string_view<charT>{str.begin(), res.rest.begin()}, res.rest};
    }
    return {std::nullopt, str};
  }

public:
  static constexpr basic_json<charT> parse(std::basic_string_view<charT> str)
  {
    basic_json_visitor<charT> vis;
    if (auto res = parse_value(vis, str)) return vis.get();
    throw std::invalid_argument("invalid JSON");
  }

  template <class Visitor>
  static constexpr void parse(Visitor& vis, std::basic_string_view<charT> str)
  {
    if (auto res = parse_value(vis, str)) return;
    throw std::invalid_argument("invalid JSON");
  }
};

}  // namespace json20

}  // namespace yk

#endif  // YK_JSON20_HPP
