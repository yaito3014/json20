#ifndef YK_JSON20_HPP
#define YK_JSON20_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <exception>
#include <functional>
#include <initializer_list>
#include <limits>
#include <locale>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <cassert>

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
  return deserialize_result<charT, std::tuple<std::decay_t<Args>...>>{it, std::forward_as_tuple(std::forward<Args>(args)...)};
}

template <class T, class charT = char>
struct deserializer {
  static constexpr auto deserialize(std::basic_string_view<charT> str) = delete;
};

template <class T>
  requires std::integral<T> || std::floating_point<T>
struct deserializer<T, char> {
  static constexpr auto deserialize(std::string_view str)
  {
    T value{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc{}) throw std::invalid_argument("from_chars error");
    return make_deserialize_result<char>(str.begin() + (ptr - str.data()), std::move(value));
  }
};

template <class T, class charT = char>
struct serializer {
  static constexpr std::basic_string<charT> serialize(const T& x) = delete;
};

template <class T>
  requires std::integral<T> || std::floating_point<T>
struct serializer<T, char> {
  static constexpr std::string serialize(const T& x)
  {
    char buf[std::numeric_limits<T>::digits10 + 1];
    auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf), x);
    if (ec != std::errc{}) throw std::runtime_error("to_chars error");
    return std::string(buf);
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

class bad_json_access : public std::exception {};

template <class charT>
class basic_json {
private:
  struct private_construct_t {};
  static inline constexpr private_construct_t private_construct{};

public:
  template <class T>
  constexpr T as_unsigned_integer_unchecked() const noexcept
  {
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr T as_signed_integer_unchecked() const noexcept
  {
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr T as_floating_point_unchecked() const noexcept
  {
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  constexpr const std::basic_string<charT>& as_string_unchecked() const noexcept { return std::get<0>(data_); }

  constexpr const std::vector<basic_json<charT>>& as_array_unchecked() const noexcept { return std::get<1>(data_); }

  constexpr const basic_json& at_unchecked(std::size_t index) const noexcept { return std::get<1>(data_)[index]; }

  constexpr const std::vector<std::pair<std::basic_string<charT>, basic_json>>& as_object_unchecked() const noexcept { return std::get<2>(data_); }

  constexpr const basic_json& at_unchecked(std::basic_string_view<charT> key) const noexcept
  {
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    return iter->second;
  }

  template <class T>
  constexpr T as_unsigned_integer() const
  {
    if (get_kind() != json_value_kind::number_unsigned_integer) throw bad_json_access{};
    return as_unsigned_integer_unchecked<T>();
  }

  template <class T>
  constexpr T as_signed_integer() const
  {
    if (get_kind() != json_value_kind::number_signed_integer) throw bad_json_access{};
    return as_signed_integer_unchecked<T>();
  }

  template <class T>
  constexpr T as_floating_point() const
  {
    if (get_kind() != json_value_kind::number_floating_point) throw bad_json_access{};
    return as_floating_point_unchecked<T>();
  }

  constexpr const std::basic_string<charT>& as_string() const
  {
    if (get_kind() != json_value_kind::string) throw bad_json_access{};
    return as_string_unchecked();
  }

  constexpr const std::vector<basic_json<charT>>& as_array() const
  {
    if (get_kind() != json_value_kind::array) throw bad_json_access{};
    return as_array_unchecked();
  }

  constexpr const std::vector<std::pair<std::basic_string<charT>, basic_json>>& as_object() const
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    return as_object_unchecked();
  }

  constexpr const basic_json& at(std::size_t index) const
  {
    if (get_kind() != json_value_kind::array) throw bad_json_access{};
    return std::get<1>(data_).at(index);
  }

  constexpr const basic_json& at(std::basic_string_view<charT> key) const
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter == vec.end() || iter->first != key) throw std::out_of_range{"json has not such key"};
    return iter->second;
  }

  template <class T>
  constexpr std::optional<T> try_as_unsigned_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_unsigned_integer) return std::nullopt;
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr std::optional<T> try_as_signed_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_signed_integer) return std::nullopt;
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template <class T>
  constexpr std::optional<T> try_as_floating_point() const noexcept
  {
    if (get_kind() != json_value_kind::number_floating_point) return std::nullopt;
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  constexpr std::optional<std::basic_string<charT>> try_as_string() const noexcept
  {
    if (get_kind() != json_value_kind::string) return std::nullopt;
    return std::get<0>(data_);
  }

  constexpr std::optional<std::vector<basic_json<charT>>> try_as_array() const noexcept
  {
    if (get_kind() != json_value_kind::array) return std::nullopt;
    return std::get<1>(data_);
  }

  constexpr std::optional<std::vector<std::pair<std::basic_string<charT>, basic_json>>> try_as_object() const noexcept
  {
    if (get_kind() != json_value_kind::object) return std::nullopt;
    return std::get<2>(data_);
  }

  constexpr std::optional<basic_json> try_at(std::size_t index) const noexcept
  {
    if (get_kind() != json_value_kind::array) return std::nullopt;
    auto& vec = std::get<1>(data_);
    if (index >= vec.size()) return std::nullopt;
    return vec[index];
  }

  constexpr std::optional<basic_json> try_at(std::basic_string_view<charT> key) const noexcept
  {
    if (get_kind() != json_value_kind::object) return std::nullopt;
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter == vec.end() || iter->first != key) return std::nullopt;
    return iter->second;
  }

  constexpr basic_json& operator[](std::size_t idx)
  {
    if (get_kind() != json_value_kind::array) throw bad_json_access{};
    auto& vec = std::get<1>(data_);
    if (idx >= vec.size()) vec.resize(idx + 1);
    return vec[idx];
  }

  constexpr basic_json& operator[](std::basic_string_view<charT> key)
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter != vec.end() && iter->first == key) return iter->second;
    return vec.emplace(iter, key, basic_json{})->second;
  }

  constexpr json_value_kind get_kind() const noexcept { return kind_; }

  constexpr bool insert(std::basic_string_view<charT> key, const basic_json& json)
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter != vec.end() && iter->first == key) return false;
    vec.emplace(iter, key, json);
    return true;
  }

  template <class... Args>
  constexpr bool emplace(std::basic_string_view<charT> key, Args&&... args)
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter != vec.end() && iter->first == key) return false;
    vec.emplace(iter, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<Args>(args)...));
    return true;
  }

  template <class... Args>
  constexpr void erase(std::basic_string_view<charT> key)
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter != vec.end() && iter->first == key) return;
    vec.erase(iter);
  }

public:
  template <class charT2>
  friend class basic_json_visitor;

  constexpr basic_json() : kind_(json_value_kind::object), data_(std::in_place_index<2>) {}

  template <std::unsigned_integral UInt>
  constexpr basic_json(UInt x) : kind_(json_value_kind::number_unsigned_integer), data_(std::in_place_index<0>, serializer<UInt, charT>::serialize(x))
  {
  }

  template <std::signed_integral Int>
  constexpr basic_json(Int x) : kind_(json_value_kind::number_signed_integer), data_(std::in_place_index<0>, serializer<Int, charT>::serialize(x))
  {
  }

  template <std::floating_point Float>
  constexpr basic_json(Float x) : kind_(json_value_kind::number_floating_point), data_(std::in_place_index<0>, serializer<Float, charT>::serialize(x))
  {
  }

  template <class S>
    requires std::is_convertible_v<const S&, std::basic_string_view<charT>>
  constexpr basic_json(const S& str) : kind_(json_value_kind::string), data_(std::in_place_index<0>, str)
  {
  }

  constexpr basic_json(std::initializer_list<basic_json> il) : kind_(json_value_kind::array), data_(std::in_place_index<1>, il) {}

  constexpr basic_json(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il)
      : kind_(json_value_kind::object), data_(std::in_place_index<2>, il)
  {
    std::ranges::sort(std::get<2>(data_), {}, &std::pair<std::basic_string<charT>, basic_json>::first);
  }

  template <std::unsigned_integral UInt>
  constexpr basic_json& operator=(UInt x)
  {
    kind_ = json_value_kind::number_unsigned_integer;
    data_.template emplace<0>(serializer<UInt, charT>::serialize(x));
    return *this;
  }

  template <std::signed_integral Int>
  constexpr basic_json& operator=(Int x)
  {
    kind_ = json_value_kind::number_signed_integer;
    data_.template emplace<0>(serializer<Int, charT>::serialize(x));
    return *this;
  }

  template <std::floating_point Float>
  constexpr basic_json& operator=(Float x)
  {
    kind_ = json_value_kind::number_floating_point;
    data_.template emplace<0>(serializer<Float, charT>::serialize(x));
    return *this;
  }

  template <class S>
    requires std::is_convertible_v<const S&, std::basic_string_view<charT>>
  constexpr basic_json& operator=(const S& str)
  {
    kind_ = json_value_kind::string;
    data_.template emplace<0>(str);
    return *this;
  }

  constexpr basic_json& operator=(std::initializer_list<basic_json> il)
  {
    kind_ = json_value_kind::array;
    data_.template emplace<1>(il);
    return *this;
  }

  constexpr basic_json& operator=(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il)
  {
    kind_ = json_value_kind::object;
    data_.template emplace<2>(il);
    std::ranges::sort(std::get<2>(data_), {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    return *this;
  }

  template <class... Args>
  constexpr basic_json(private_construct_t, json_value_kind kind, Args&&... args) : kind_(kind), data_(std::forward<Args>(args)...)
  {
  }

  static constexpr basic_json array(std::initializer_list<basic_json> il) { return basic_json(std::move(il)); }

  static constexpr basic_json object(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il) { return basic_json(std::move(il)); }

private:
  json_value_kind kind_;
  std::variant<
      std::basic_string<charT>,                                     //
      std::vector<basic_json>,                                      //
      std::vector<std::pair<std::basic_string<charT>, basic_json>>  //
      >
      data_;
};

using json = basic_json<char>;
using wjson = basic_json<wchar_t>;
using u8json = basic_json<char8_t>;
using u16json = basic_json<char16_t>;
using u32json = basic_json<char32_t>;

template <class charT>
class basic_json_visitor {
public:
  constexpr void on_null(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::null, std::in_place_index<0>, str.begin(), str.end());
  }
  constexpr void on_boolean(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::boolean, std::in_place_index<0>, str.begin(), str.end());
  }
  constexpr void on_number_unsigned_integer(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(
        std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::number_unsigned_integer, std::in_place_index<0>, str.begin(), str.end()
    );
  }
  constexpr void on_number_signed_integer(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(
        std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::number_signed_integer, std::in_place_index<0>, str.begin(), str.end()
    );
  }
  constexpr void on_number_floating_point(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(
        std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::number_floating_point, std::in_place_index<0>, str.begin(), str.end()
    );
  }
  constexpr void on_string(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::string, std::in_place_index<0>, str.begin(), str.end());
  }

  constexpr void on_array_start() { stack_.emplace_back(start_tag{}); }
  constexpr void on_array_finalize()
  {
    auto i = std::ranges::find_if(stack_ | std::views::reverse, [](const auto& var) { return std::holds_alternative<start_tag>(var); }).base();
    std::vector<basic_json<charT>> vec;
    for (auto j = i; j != stack_.end(); ++j) {
      vec.emplace_back(std::get<basic_json<charT>>(std::move(*j)));
    }
    stack_.erase(i - 1, stack_.end());
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::array, std::in_place_index<1>, std::move(vec));
  }
  constexpr void on_array_abort()
  {
    auto i = std::ranges::find_if(stack_ | std::views::reverse, [](const auto& var) { return std::holds_alternative<start_tag>(var); }).base();
    stack_.erase(i - 1, stack_.end());
  }

  constexpr void on_object_start() { stack_.emplace_back(start_tag{}); }
  constexpr void on_object_finalize()
  {
    auto i = std::ranges::find_if(stack_ | std::views::reverse, [](const auto& var) { return std::holds_alternative<start_tag>(var); }).base();
    std::vector<std::pair<std::basic_string<charT>, basic_json<charT>>> vec;
    for (auto j = i; j != stack_.end(); j += 2) {
      vec.emplace_back(std::get<basic_json<charT>>(*j).as_string(), std::get<basic_json<charT>>(*(j + 1)));
    }
    std::ranges::sort(vec, {}, &std::pair<std::basic_string<charT>, basic_json<charT>>::first);
    stack_.erase(i - 1, stack_.end());
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::private_construct, json_value_kind::object, std::in_place_index<2>, std::move(vec));
  }
  constexpr void on_object_abort()
  {
    auto i = std::ranges::find_if(stack_ | std::views::reverse, [](const auto& var) { return std::holds_alternative<start_tag>(var); }).base();
    stack_.erase(i - 1, stack_.end());
  }

  constexpr basic_json<charT> get() const
  {
    assert(stack_.size() == 1);
    return std::get<basic_json<charT>>(stack_.back());
  }

private:
  struct start_tag {};
  std::vector<std::variant<start_tag, basic_json<charT>>> stack_;
};

using json_visitor = basic_json_visitor<char>;
using wjson_visitor = basic_json_visitor<wchar_t>;
using u8json_visitor = basic_json_visitor<char8_t>;
using u16json_visitor = basic_json_visitor<char16_t>;
using u32json_visitor = basic_json_visitor<char32_t>;

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
    const auto control_characters_parser = alt(
        lit(YK_JSON20_WIDEN_STRING(charT, "\x00")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x01")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x02")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x03")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x04")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x05")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x06")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x07")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x08")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x09")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x0A")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x0B")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x0C")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x0D")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x0E")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x0F")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x00")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x11")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x12")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x13")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x14")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x15")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x16")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x17")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x18")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x19")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x1A")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x1B")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x1C")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x1D")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x1E")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\x1F"))   //
    );

    return control_characters_parser(str);
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

    // const auto hex = alt(
    //     lit(YK_JSON20_WIDEN_STRING(charT, "0")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "1")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "2")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "3")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "4")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "5")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "6")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "7")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "8")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "9")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "A")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "B")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "C")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "D")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "E")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "F")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "a")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "b")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "c")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "d")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "e")),  //
    //     lit(YK_JSON20_WIDEN_STRING(charT, "f"))   //
    // );

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

  static constexpr std::optional<basic_json<charT>> try_parse(std::basic_string_view<charT> str) noexcept
  {
    basic_json_visitor<charT> vis;
    if (auto res = parse_value(vis, str)) return vis.get();
    return std::nullopt;
  }

  template <class Visitor>
  static constexpr void parse(Visitor& vis, std::basic_string_view<charT> str)
  {
    if (auto res = parse_value(vis, str)) return;
    throw std::invalid_argument("invalid JSON");
  }

  template <class Visitor>
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

template <class charT>
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

template <class charT>
class basic_checked_string {
public:
  template <class S>
    requires std::convertible_to<S, std::basic_string_view<charT>>
  consteval basic_checked_string(const S& str) : str_(str)
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

#endif  // YK_JSON20_HPP
