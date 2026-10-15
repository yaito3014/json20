#include <yk/json20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <utility>

using json_parser = yk::json20::basic_json_parser<char>;

TEST_CASE("null", "[parse]")
{
  {
    const auto x = json_parser::parse("null");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::null));
  }
}

TEST_CASE("boolean", "[parse]")
{
  {
    const auto x = json_parser::parse("true");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
  {
    const auto x = json_parser::parse("false");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::boolean));
  }
}

TEST_CASE("number_unsigned_integer", "[parse]")
{
  {
    const auto x = json_parser::parse("0");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
    REQUIRE(x.as_unsigned_integer<unsigned>() == 0);
  }
  {
    const auto x = json_parser::parse("1234");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_unsigned_integer));
    REQUIRE(x.as_unsigned_integer<unsigned>() == 1234);
  }
}

TEST_CASE("number_signed_integer", "[parse]")
{
  {
    const auto x = json_parser::parse("-1234");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_signed_integer));
    REQUIRE(x.as_signed_integer<int>() == -1234);
  }
}

TEST_CASE("number_floating_point", "[parse]")
{
  {
    const auto x = json_parser::parse("1234e5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 1234e5);
  }
  {
    const auto x = json_parser::parse("1234e+5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 1234e+5);
  }
  {
    const auto x = json_parser::parse("1234e-5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 1234e-5);
  }
  {
    const auto x = json_parser::parse("12.34");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34);
  }
  {
    const auto x = json_parser::parse("12.34e5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34e5);
  }
  {
    const auto x = json_parser::parse("12.34e+5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34e+5);
  }
  {
    const auto x = json_parser::parse("12.34e-5");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::number_floating_point));
    REQUIRE(x.as_floating_point<double>() == 12.34e-5);
  }
}

namespace {

// same path as checked_string: constant evaluation with a no-op visitor
constexpr bool accepts(std::string_view str)
{
  yk::json20::basic_noop_visitor<char> vis;
  return json_parser::try_parse(vis, str);
}

}  // namespace

TEST_CASE("number grammar", "[parse]")
{
  // RFC 8259: frac = decimal-point 1*DIGIT, exp = e [ minus / plus ] 1*DIGIT
  static_assert(!accepts("1."));
  static_assert(!accepts("1.e5"));
  static_assert(!accepts("1e"));
  static_assert(!accepts("1e+"));
  static_assert(!accepts("1e-"));
  static_assert(!accepts("1.5e"));
  static_assert(!accepts("[1.]"));
  static_assert(!accepts("{\"a\":1e}"));
  static_assert(!accepts("-"));
  static_assert(!accepts(".5"));
  static_assert(!accepts("01"));
  static_assert(!accepts("+1"));

  static_assert(accepts("0"));
  static_assert(accepts("-0"));
  static_assert(accepts("0.5"));
  static_assert(accepts("1.0"));
  static_assert(accepts("1e5"));
  static_assert(accepts("1E+5"));
  static_assert(accepts("1e-05"));
  static_assert(accepts("-0.0e-0"));
  static_assert(accepts("[1.5,2e3]"));

  CHECK_FALSE(json_parser::try_parse("1.").has_value());
  CHECK_FALSE(json_parser::try_parse("1e").has_value());
  CHECK_FALSE(json_parser::try_parse("1e+").has_value());
}

TEST_CASE("truncated input", "[parse]")
{
  // inputs ending inside a string must be rejected, not crash
  static_assert(!accepts("\""));
  static_assert(!accepts("\"abc"));
  static_assert(!accepts("[\"abc"));
  static_assert(!accepts("{\"a"));
  static_assert(!accepts("{\"a\":\"b"));

  CHECK_FALSE(json_parser::try_parse("\"").has_value());
  CHECK_FALSE(json_parser::try_parse("\"abc").has_value());
  CHECK_FALSE(json_parser::try_parse("[\"abc").has_value());
  CHECK_FALSE(json_parser::try_parse("{\"a").has_value());
  CHECK_THROWS_AS(json_parser::parse("\"abc"), std::invalid_argument);
}

TEST_CASE("array", "[parse]")
{
  {
    const auto x = json_parser::parse("[]");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::array));
    REQUIRE(x.as_array().empty());
  }
  {
    const auto x = json_parser::parse("[ ]");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::array));
    REQUIRE(x.as_array().empty());
  }
  {
    const auto x = json_parser::parse("[12]");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::array));
    REQUIRE(x.at(0).as_unsigned_integer<unsigned>() == 12);
  }
}

TEST_CASE("string", "[parse]")
{
  {
    const auto x = json_parser::parse("\"foo\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "foo");
  }
  {
    const auto x = json_parser::parse("\"\\\\\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\\");
  }
  {
    const auto x = json_parser::parse("\"\\\"\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\"");
  }
  {
    const auto x = json_parser::parse("\"\\/\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "/");
  }
  {
    const auto x = json_parser::parse("\"\\/\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "/");
  }
  {
    const auto x = json_parser::parse("\"\\b\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\b");
  }
  {
    const auto x = json_parser::parse("\"\\f\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\f");
  }
  {
    const auto x = json_parser::parse("\"\\n\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\n");
  }
  {
    const auto x = json_parser::parse("\"\\r\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\r");
  }
  {
    const auto x = json_parser::parse("\"\\t\"");
    REQUIRE((x.get_kind() == yk::json20::json_value_kind::string));
    REQUIRE(x.as_string() == "\t");
  }
  {
    // escapes mixed with plain runs, including at both ends
    const auto x = json_parser::parse("\"\\\"x\\ty\\\\z\\/\\n\"");
    REQUIRE(x.as_string() == "\"x\ty\\z/\n");
  }
  {
    const auto x = json_parser::parse("\"\"");
    REQUIRE(x.as_string().empty());
  }
  static_assert(accepts("\"a\\nb\""));
  static_assert(!accepts("\"\\q\""));
  static_assert(!accepts("\"\\\""));
  CHECK_FALSE(json_parser::try_parse("\"\\q\"").has_value());
}

TEST_CASE("control characters", "[parse]")
{
  // RFC 8259: unescaped control characters (U+0000..U+001F) must not appear in strings
  for (int c = 0x00; c <= 0x1F; ++c) {
    char const buf[3] = {'"', static_cast<char>(c), '"'};
    CAPTURE(c);
    CHECK_FALSE(json_parser::try_parse(std::string_view(buf, 3)).has_value());
  }
}

TEST_CASE("object", "[parse]")
{
  {
    const auto x = json_parser::parse("{}");
    CHECK((x.get_kind() == yk::json20::json_value_kind::object));
  }
  {
    const auto x = json_parser::parse("{\"foo\":1234}");
    CHECK((x.get_kind() == yk::json20::json_value_kind::object));
    CHECK(x.at("foo").as_unsigned_integer<unsigned>() == 1234);
  }
}

TEST_CASE("duplicate keys", "[parse]")
{
  // rejected by default, at any depth
  CHECK_THROWS_AS(json_parser::parse("{\"a\":1,\"a\":2}"), std::invalid_argument);
  CHECK_FALSE(json_parser::try_parse("{\"a\":1,\"a\":2}").has_value());
  CHECK_FALSE(json_parser::try_parse("[{\"a\":1,\"a\":2}]").has_value());
  CHECK_FALSE(json_parser::try_parse("{\"x\":{\"a\":1,\"a\":2}}").has_value());

  // keys are compared after unescaping: "\/" is "/"
  CHECK_FALSE(json_parser::try_parse("{\"/\":1,\"\\/\":2}").has_value());

  // the same key in different objects is not a duplicate
  CHECK(json_parser::try_parse("[{\"a\":1},{\"a\":2}]").has_value());

  // checked_string validates syntax only, and duplicates are grammatical
  static_assert(accepts("{\"a\":1,\"a\":2}"));
}

TEST_CASE("duplicate key policies", "[parse]")
{
  using yk::json20::duplicate_key_policy;
  constexpr yk::json20::parse_options first{.duplicate_keys = duplicate_key_policy::first_wins};
  constexpr yk::json20::parse_options last{.duplicate_keys = duplicate_key_policy::last_wins};

  {
    auto const x = json_parser::parse("{\"a\":1,\"b\":0,\"a\":2}", first);
    CHECK(x.as_object().size() == 2);
    CHECK(x.at("a").as_unsigned_integer<unsigned>() == 1);
  }
  {
    auto const x = json_parser::parse("{\"a\":1,\"b\":0,\"a\":2}", last);
    CHECK(x.as_object().size() == 2);
    CHECK(x.at("a").as_unsigned_integer<unsigned>() == 2);
  }
  CHECK(json_parser::try_parse("{\"a\":1,\"a\":2}", last).has_value());

  // many copies of one key interleaved with others: the sort used to leave
  // their order unspecified, and at() picked a different one per library
  for (unsigned n : {4u, 17u, 40u, 100u}) {
    std::string s = "{";
    for (unsigned i = 0; i < n; ++i) {
      if (i) s += ",";
      s += "\"" + std::string(i % 2 ? "z" : "a") + std::to_string(i) + "\":0,\"k\":" + std::to_string(i);
    }
    s += "}";
    CAPTURE(n);
    auto const x = json_parser::parse(s, first);
    auto const y = json_parser::parse(s, last);
    CHECK(x.at("k").as_unsigned_integer<unsigned>() == 0);
    CHECK(y.at("k").as_unsigned_integer<unsigned>() == n - 1);
    CHECK(x.as_object().size() == n + 1);
    CHECK(std::ranges::is_sorted(x.as_object(), {}, &std::pair<std::string, yk::json20::json>::first));

    // one member per key, so erasing a key really removes it
    auto z = y;
    CHECK(z.erase("k"));
    CHECK(z.try_at("k") == nullptr);
  }

  // the DOM visitor takes the same options
  {
    yk::json20::json_visitor vis(last);
    json_parser::parse(vis, "{\"a\":1,\"a\":2}");
    CHECK_FALSE(vis.has_rejected_duplicate_key());
    CHECK(vis.get().at("a").as_unsigned_integer<unsigned>() == 2);
  }
  {
    yk::json20::json_visitor vis;
    json_parser::parse(vis, "{\"a\":1,\"a\":2}");
    CHECK(vis.has_rejected_duplicate_key());
  }
}

TEST_CASE("checked_string", "[parse]")
{
  const auto f = [](yk::json20::basic_checked_string<char>) {};
  f("null");
  f("true");
  f("false");
  f("1234");
  f("-1234");
  f("1234e5");
  f("1234e+5");
  f("1234e-5");
  f("12.34");
  f("-12.34");
  f("12.34e5");
  f("12.34e+5");
  f("12.34e-5");
  f("\"foo\"");
  f("{\"foo\":123}");
  f("[123,3.14]");
}
