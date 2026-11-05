#ifndef YK_ZZ_JSON20_PARSER_HPP
#define YK_ZZ_JSON20_PARSER_HPP

#include <yk/json20/detail/fixed_string.hpp>
#include <yk/json20/detail/select_str.hpp>
#include <yk/json20/json.hpp>
#include <yk/json20/visitor.hpp>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <cassert>

namespace yk::json20 {

template<class charT>
class basic_json_parser {
public:
  // Every parser only recognizes: on success `match` is the span of the
  // input it consumed and `rest` what follows; on failure `match` is empty
  // and `rest` is the untouched input. Both always point into the caller's
  // input, which therefore must outlive any use of a result.
  struct parse_result {
    std::optional<std::basic_string_view<charT>> match;
    std::basic_string_view<charT> rest;

    constexpr explicit operator bool() const noexcept { return match.has_value() && rest.empty(); }
  };

private:
  // The part of `str` in front of `rest`, which must be a suffix of `str`.
  // Computed from sizes because iterators of two different string_views must
  // not be mixed: MSVC's debug iterators reject it, even in constant evaluation.
  static constexpr std::basic_string_view<charT> consumed(std::basic_string_view<charT> str, std::basic_string_view<charT> rest) noexcept
  {
    return str.substr(0, str.size() - rest.size());
  }

  // holds the literal by value so a parser object never refers to the
  // (often function-local) literal it was built from
  template<std::size_t N>
  struct lit_parser {
    detail::basic_fixed_string<charT, N> literal;

    constexpr auto operator()(std::basic_string_view<charT> str) const noexcept -> parse_result
    {
      if (str.starts_with(std::basic_string_view<charT>(literal))) {
        return {
            str.substr(0, N),
            str.substr(N),
        };
      } else {
        return {std::nullopt, str};
      }
    };
  };

  template<std::size_t N>
  static constexpr auto lit(detail::basic_fixed_string<charT, N> const& literal) noexcept
  {
    return lit_parser<N>{literal};
  }

  template<class Parser>
  static constexpr auto except(Parser const& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result {
      if (str.empty()) return {std::nullopt, str};
      if (auto res = p(str); res.match)
        return {std::nullopt, str};
      else
        return {str.substr(0, 1), str.substr(1)};
    };
  }

  template<class Parser1, class Parser2>
  struct alt_parser {
    [[no_unique_address]] Parser1 p1;
    [[no_unique_address]] Parser2 p2;

    constexpr auto operator()(std::basic_string_view<charT> str) const noexcept -> parse_result
    {
      if (auto res1 = p1(str); res1.match) return res1;
      if (auto res2 = p2(str); res2.match) return res2;
      return {std::nullopt, str};
    };
  };

  template<class Parser1, class Parser2>
  static constexpr auto alt(Parser1 const& p1, Parser2 const& p2) noexcept
  {
    return alt_parser{p1, p2};
  }

  template<class Parser1, class Parser2, class Parser3, class... Parsers>
  static constexpr auto alt(Parser1 const& p1, Parser2 const& p2, Parser3 const& p3, Parsers const&... parsers) noexcept
  {
    return alt(p1, alt(p2, p3, parsers...));
  }

  template<class Parser1, class Parser2>
  struct seq_parser {
    [[no_unique_address]] Parser1 p1;
    [[no_unique_address]] Parser2 p2;

    constexpr auto operator()(std::basic_string_view<charT> str) const noexcept -> parse_result
    {
      if (auto res1 = p1(str); res1.match) {
        if (auto res2 = p2(res1.rest); res2.match) {
          return {consumed(str, res2.rest), res2.rest};
        }
      }
      return {std::nullopt, str};
    };
  };

  template<class Parser1, class Parser2>
  static constexpr auto seq(Parser1 const& p1, Parser2 const& p2) noexcept
  {
    return seq_parser{p1, p2};
  }

  template<class Parser1, class Parser2, class Parser3, class... Parsers>
  static constexpr auto seq(Parser1 const& p1, Parser2 const& p2, Parser3 const& p3, Parsers const&... parsers) noexcept
  {
    return seq(p1, seq(p2, p3, parsers...));
  }

  template<class Parser>
  static constexpr auto opt(Parser const& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result {
      if (auto res = p(str); res.match) return res;
      return {str.substr(0, 0), str};
    };
  }

  // stops as soon as `p` fails or succeeds without consuming anything, so
  // many(opt(x)) and the like terminate
  template<class Parser>
  static constexpr auto many(Parser const& p) noexcept
  {
    return [p](std::basic_string_view<charT> str) -> parse_result {
      auto rest = str;
      for (auto res = p(rest); res.match && res.rest.size() < rest.size(); res = p(rest)) {
        rest = res.rest;
      }
      return {consumed(str, rest), rest};
    };
  }

  template<class Parser, class Delim>
  static constexpr auto sep_by(Parser const& p, Delim const& d) noexcept
  {
    return seq(p, many(seq(d, p)));
  }

  // A lambda with an explicit return type rather than std::bind_front: the
  // grammar is recursive (value -> array -> value), and bind_front's deduced
  // call operator is rejected there by clang 21 with libstdc++.
  template<class Visitor>
  static constexpr auto bind_visitor(parse_result (*parser)(Visitor&, std::basic_string_view<charT>) noexcept, Visitor& vis) noexcept
  {
    return [parser, &vis](std::basic_string_view<charT> str) noexcept -> parse_result { return parser(vis, str); };
  }

  static constexpr parse_result ws(std::basic_string_view<charT> str) noexcept
  {
    auto const space = YK_JSON20_WIDEN_STRING(charT, " ");
    auto const lf = YK_JSON20_WIDEN_STRING(charT, "\u000A");
    auto const cr = YK_JSON20_WIDEN_STRING(charT, "\u000D");
    auto const tab = YK_JSON20_WIDEN_STRING(charT, "\u0009");

    return many(alt(lit(space), lit(lf), lit(cr), lit(tab)))(str);
  }

  static constexpr parse_result cntrl(std::basic_string_view<charT> str) noexcept
  {
    auto const control_characters_parser =
        alt(lit(YK_JSON20_WIDEN_STRING(charT, "\u0000")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0001")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0002")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u0003")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0004")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0005")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u0006")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0007")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0008")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u0009")), lit(YK_JSON20_WIDEN_STRING(charT, "\u000A")), lit(YK_JSON20_WIDEN_STRING(charT, "\u000B")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u000C")), lit(YK_JSON20_WIDEN_STRING(charT, "\u000D")), lit(YK_JSON20_WIDEN_STRING(charT, "\u000E")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u000F")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0010")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0011")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u0012")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0013")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0014")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u0015")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0016")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0017")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u0018")), lit(YK_JSON20_WIDEN_STRING(charT, "\u0019")), lit(YK_JSON20_WIDEN_STRING(charT, "\u001A")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u001B")), lit(YK_JSON20_WIDEN_STRING(charT, "\u001C")), lit(YK_JSON20_WIDEN_STRING(charT, "\u001D")),
            lit(YK_JSON20_WIDEN_STRING(charT, "\u001E")), lit(YK_JSON20_WIDEN_STRING(charT, "\u001F")));

    return control_characters_parser(str);
  }

#define YK_JSON20_INVOKE_VISITOR_ARG2(vis, member)   \
  if constexpr (requires { vis.member##_raw(); }) {  \
    vis.member##_raw();                              \
  } else if constexpr (requires { vis.member(); }) { \
    vis.member();                                    \
  }

#define YK_JSON20_INVOKE_VISITOR_ARG3(vis, member, raw_arg) \
  if constexpr (requires { vis.member##_raw(raw_arg); }) {  \
    vis.member##_raw(raw_arg);                              \
  } else if constexpr (requires { vis.member(); }) {        \
    vis.member();                                           \
  }

#define YK_JSON20_INVOKE_VISITOR_ARG4(vis, member, normal_arg, raw_arg) \
  if constexpr (requires { vis.member##_raw(normal_arg, raw_arg); }) {  \
    vis.member##_raw(normal_arg, raw_arg);                              \
  } else if constexpr (requires { vis.member(normal_arg); }) {          \
    vis.member(normal_arg);                                             \
  }

  template<class Visitor>
  static constexpr parse_result parse_null(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    constexpr auto null_string = YK_JSON20_WIDEN_STRING(charT, "null");

    auto res = lit(null_string)(str);
    if (res.match) {
      YK_JSON20_INVOKE_VISITOR_ARG4(vis, on_null, *res.match, *res.match)
    }
    return res;
  }

  template<class Visitor>
  static constexpr parse_result parse_boolean(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    constexpr auto true_string = YK_JSON20_WIDEN_STRING(charT, "true");
    constexpr auto false_string = YK_JSON20_WIDEN_STRING(charT, "false");

    auto res = alt(lit(true_string), lit(false_string))(str);
    if (res.match) {
      YK_JSON20_INVOKE_VISITOR_ARG4(vis, on_boolean, *res.match, *res.match)
    }
    return res;
  }

  template<class Visitor>
  static constexpr parse_result parse_number(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const parse0 = lit(YK_JSON20_WIDEN_STRING(charT, "0"));
    auto const parse1to9 =
        alt(lit(YK_JSON20_WIDEN_STRING(charT, "1")), lit(YK_JSON20_WIDEN_STRING(charT, "2")), lit(YK_JSON20_WIDEN_STRING(charT, "3")),
            lit(YK_JSON20_WIDEN_STRING(charT, "4")), lit(YK_JSON20_WIDEN_STRING(charT, "5")), lit(YK_JSON20_WIDEN_STRING(charT, "6")),
            lit(YK_JSON20_WIDEN_STRING(charT, "7")), lit(YK_JSON20_WIDEN_STRING(charT, "8")), lit(YK_JSON20_WIDEN_STRING(charT, "9")));
    auto const parse0to9 = alt(parse0, parse1to9);

    auto const parse_minus = lit(YK_JSON20_WIDEN_STRING(charT, "-"));
    auto const parse_plus = lit(YK_JSON20_WIDEN_STRING(charT, "+"));
    auto const parse_sign = alt(parse_minus, parse_plus);

    auto const parse_dot = lit(YK_JSON20_WIDEN_STRING(charT, "."));
    auto const parse_e = alt(lit(YK_JSON20_WIDEN_STRING(charT, "e")), lit(YK_JSON20_WIDEN_STRING(charT, "E")));

    auto const parse_digits = seq(parse0to9, many(parse0to9));

    auto const parse_frac = seq(parse_dot, parse_digits);
    auto const parse_exp = seq(parse_e, opt(parse_sign), parse_digits);

    auto const res1 = parse_minus(str);
    bool const has_minus_sign = bool(res1.match);
    auto const int_frac_exp = res1.rest;

    auto const parser = alt(parse0, seq(parse1to9, many(parse0to9)));

    auto res2 = parser(int_frac_exp);
    if (res2.match) {
      auto const frac_exp = res2.rest;

      auto res3 = parse_frac(frac_exp);
      bool const has_frac = bool(res3.match);
      auto const exp = res3.rest;

      auto res4 = parse_exp(exp);
      bool const has_exp = bool(res4.match);
      auto const rest = res4.rest;

      std::basic_string_view<charT> match = consumed(str, rest);

      if (has_frac || has_exp) {
        YK_JSON20_INVOKE_VISITOR_ARG4(vis, on_number_floating_point, match, match);
      } else if (has_minus_sign) {
        YK_JSON20_INVOKE_VISITOR_ARG4(vis, on_number_signed_integer, match, match);
      } else {
        YK_JSON20_INVOKE_VISITOR_ARG4(vis, on_number_unsigned_integer, match, match);
      }

      return {match, rest};
    } else {
      return {std::nullopt, str};
    }
  }

  template<class Visitor>
  static constexpr parse_result parse_array(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const open_bracket = YK_JSON20_WIDEN_STRING(charT, "[");
    auto const close_bracket = YK_JSON20_WIDEN_STRING(charT, "]");
    auto const comma = YK_JSON20_WIDEN_STRING(charT, ",");

    auto const parser = seq(lit(open_bracket), alt(sep_by(bind_visitor(parse_value<Visitor>, vis), lit(comma)), ws), lit(close_bracket));

    YK_JSON20_INVOKE_VISITOR_ARG2(vis, on_array_start)
    auto res = parser(str);
    if (res.match) {
      std::basic_string_view<charT> match = consumed(str, res.rest);
      YK_JSON20_INVOKE_VISITOR_ARG3(vis, on_array_finalize, match)
      return {match, res.rest};
    }
    YK_JSON20_INVOKE_VISITOR_ARG2(vis, on_array_abort)
    return {std::nullopt, str};
  }

  template<class Visitor>
  static constexpr parse_result parse_string(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const quote = YK_JSON20_WIDEN_STRING(charT, "\"");
    auto const backslash = YK_JSON20_WIDEN_STRING(charT, "\\");

    auto const escape_sequence = [&](std::basic_string_view<charT> s) -> parse_result {
      if (!s.starts_with(std::basic_string_view<charT>(backslash))) return {std::nullopt, s};
      auto const n = backslash.size() + unescape(s.substr(backslash.size()), nullptr);
      if (n == backslash.size()) return {std::nullopt, s};
      return {s.substr(0, n), s.substr(n)};
    };

    auto const body_parser = many(alt(except(alt(lit(quote), lit(backslash), cntrl)), escape_sequence));

    auto const res1 = lit(quote)(str);
    if (!res1.match) return {std::nullopt, str};
    auto const res2 = body_parser(res1.rest);
    if (!res2.match) return {std::nullopt, str};
    auto const res3 = lit(quote)(res2.rest);
    if (!res3.match) return {std::nullopt, str};

    std::basic_string_view<charT> const match = consumed(str, res3.rest);
    std::basic_string_view<charT> const body = *res2.match;

    // decode only when there is something to decode; otherwise hand out the input itself
    std::basic_string<charT> decoded;
    std::basic_string_view<charT> content = body;
    if (body.find(std::basic_string_view<charT>(backslash)) != std::basic_string_view<charT>::npos) {
      decoded = decode_string(body);
      content = decoded;
    }
    YK_JSON20_INVOKE_VISITOR_ARG4(vis, on_string, content, match)
    return {match, res3.rest};
  }

  // Recognizes one escape sequence (the part after the backslash) at the head
  // of `s`. Returns the number of code units it spans, or 0 if there is none;
  // on a match, also appends the decoded character to `out` when given.
  // This is the only table of JSON escapes: recognition and decoding share it.
  // TODO: implement unicode escape sequence
  static constexpr std::size_t unescape(std::basic_string_view<charT> s, std::basic_string<charT>* out)
  {
    auto const one = [&](auto const& escaped, auto const& decoded) -> std::size_t {
      if (!s.starts_with(std::basic_string_view<charT>(escaped))) return 0;
      if (out) out->append(std::basic_string_view<charT>(decoded));
      return escaped.size();
    };

    auto const quote = YK_JSON20_WIDEN_STRING(charT, "\"");
    auto const backslash = YK_JSON20_WIDEN_STRING(charT, "\\");
    auto const slash = YK_JSON20_WIDEN_STRING(charT, "/");

    if (auto n = one(quote, quote)) return n;
    if (auto n = one(backslash, backslash)) return n;
    if (auto n = one(slash, slash)) return n;
    if (auto n = one(YK_JSON20_WIDEN_STRING(charT, "b"), YK_JSON20_WIDEN_STRING(charT, "\u0008"))) return n;
    if (auto n = one(YK_JSON20_WIDEN_STRING(charT, "f"), YK_JSON20_WIDEN_STRING(charT, "\u000C"))) return n;
    if (auto n = one(YK_JSON20_WIDEN_STRING(charT, "n"), YK_JSON20_WIDEN_STRING(charT, "\u000A"))) return n;
    if (auto n = one(YK_JSON20_WIDEN_STRING(charT, "r"), YK_JSON20_WIDEN_STRING(charT, "\u000D"))) return n;
    if (auto n = one(YK_JSON20_WIDEN_STRING(charT, "t"), YK_JSON20_WIDEN_STRING(charT, "\u0009"))) return n;
    return 0;
  }

  // `body` must already be recognized by the string grammar, so every
  // backslash in it starts a valid escape sequence.
  static constexpr std::basic_string<charT> decode_string(std::basic_string_view<charT> body)
  {
    auto const backslash = YK_JSON20_WIDEN_STRING(charT, "\\");
    std::basic_string<charT> result;
    while (true) {
      auto const pos = body.find(std::basic_string_view<charT>(backslash));
      result.append(body.substr(0, pos));
      if (pos == std::basic_string_view<charT>::npos) return result;
      body.remove_prefix(pos + backslash.size());
      auto const n = unescape(body, &result);
      assert(n != 0);
      body.remove_prefix(n);
    }
  }

  template<class Visitor>
  static constexpr parse_result parse_object(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const open_brace = YK_JSON20_WIDEN_STRING(charT, "{");
    auto const close_brace = YK_JSON20_WIDEN_STRING(charT, "}");
    auto const colon = YK_JSON20_WIDEN_STRING(charT, ":");
    auto const comma = YK_JSON20_WIDEN_STRING(charT, ",");

    auto const key_value_parser = seq(ws, bind_visitor(parse_string<Visitor>, vis), ws, lit(colon), bind_visitor(parse_value<Visitor>, vis));
    auto const parser = seq(lit(open_brace), alt(sep_by(key_value_parser, lit(comma)), ws), lit(close_brace));

    YK_JSON20_INVOKE_VISITOR_ARG2(vis, on_object_start)
    auto res = parser(str);
    if (res.match) {
      std::basic_string_view<charT> match = consumed(str, res.rest);
      YK_JSON20_INVOKE_VISITOR_ARG3(vis, on_object_finalize, match)
      return {match, res.rest};
    }
    YK_JSON20_INVOKE_VISITOR_ARG2(vis, on_object_abort)
    return {std::nullopt, str};
  }

  template<class Visitor>
  static constexpr parse_result parse_value(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const parser =
        seq(ws,
            alt(bind_visitor(parse_null<Visitor>, vis), bind_visitor(parse_boolean<Visitor>, vis), bind_visitor(parse_number<Visitor>, vis),
                bind_visitor(parse_string<Visitor>, vis), bind_visitor(parse_array<Visitor>, vis), bind_visitor(parse_object<Visitor>, vis)),
            ws);

    auto res = parser(str);
    if (res.match) {
      return {consumed(str, res.rest), res.rest};
    }
    return {std::nullopt, str};
  }

public:
  static constexpr basic_json<charT> parse(std::basic_string_view<charT> str, parse_options options = {})
  {
    basic_json_visitor<charT> vis(options);
    if (auto res = parse_value(vis, str)) {
      if (vis.has_rejected_duplicate_key()) throw std::invalid_argument("duplicate key in JSON object");
      return vis.get();
    }
    throw std::invalid_argument("invalid JSON");
  }

  static constexpr std::optional<basic_json<charT>> try_parse(std::basic_string_view<charT> str, parse_options options = {}) noexcept
  {
    basic_json_visitor<charT> vis(options);
    if (auto res = parse_value(vis, str); res && !vis.has_rejected_duplicate_key()) return vis.get();
    return std::nullopt;
  }

  template<class Visitor>
  static constexpr void parse(Visitor& vis, std::basic_string_view<charT> str)
  {
    if (auto res = parse_value(vis, str)) return;
    throw std::invalid_argument("invalid JSON");
  }

  template<class Visitor>
  static constexpr bool try_parse(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    if (auto res = parse_value(vis, str)) return true;
    return false;
  }
};

using json_parser = basic_json_parser<char>;
using wjson_parser = basic_json_parser<wchar_t>;
using u8json_parser = basic_json_parser<char8_t>;
using u16json_parser = basic_json_parser<char16_t>;
using u32json_parser = basic_json_parser<char32_t>;

}  // namespace yk::json20

#endif  // YK_ZZ_JSON20_PARSER_HPP
