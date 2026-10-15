#ifndef YK_JSON20_HPP
#define YK_JSON20_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <exception>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <cassert>

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

template<class charT, class Tuple>
struct deserialize_result {
  typename std::basic_string_view<charT>::iterator it;
  Tuple args;
};

template<class charT, class... Args>
constexpr auto make_deserialize_result(typename std::basic_string_view<charT>::iterator it, Args&&... args) noexcept
{
  return deserialize_result<charT, std::tuple<std::decay_t<Args>...>>{it, std::forward_as_tuple(std::forward<Args>(args)...)};
}

template<class T, class charT = char>
struct deserializer {
  static constexpr auto deserialize(std::basic_string_view<charT> str) = delete;
};

template<>
struct deserializer<bool, char> {
  static constexpr std::string_view true_ = "true";
  static constexpr std::string_view false_ = "false";
  static constexpr auto deserialize(std::string_view str)
  {
    if (str == true_) return make_deserialize_result<char>(str.begin() + true_.size(), true);
    if (str == false_) return make_deserialize_result<char>(str.begin() + false_.size(), false);
    throw std::invalid_argument("argument is not boolean");
  }
};

template<class T>
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

template<class T, class charT = char>
struct serializer {
  static constexpr std::basic_string<charT> serialize(T const& x) = delete;
};

template<>
struct serializer<bool, char> {
  static constexpr std::string_view true_ = "true";
  static constexpr std::string_view false_ = "false";
  static constexpr std::string serialize(bool const& x) { return std::string(x ? true_ : false_); }
};

template<class T>
  requires std::integral<T> || std::floating_point<T>
struct serializer<T, char> {
  static constexpr std::string serialize(T const& x)
  {
    // digits10 rounds down, so +2 covers all digits plus the sign;
    // shortest float form needs max_digits10 digits, sign, dot, and "e+XXXX"
    constexpr std::size_t buf_size = [] {
      if constexpr (std::integral<T>)
        return std::numeric_limits<T>::digits10 + 2;
      else
        return std::numeric_limits<T>::max_digits10 + 8;
    }();
    char buf[buf_size];
    auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf), x);
    if (ec != std::errc{}) throw std::runtime_error("to_chars error");
    return std::string(buf, ptr);
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

namespace detail {

// maps a semantic kind onto its storage alternative: every scalar kind keeps
// its lexeme in the string at index 0, only the containers get their own slot
template<json_value_kind K>
inline constexpr std::size_t storage_index_of =
    K == json_value_kind::array ? 1 : K == json_value_kind::object ? 2 : 0;

}  // namespace detail

template<class charT>
class basic_json {
private:
  template<json_value_kind K>
  struct kind_construct_t {};

  // the only places where a kind is paired with a storage alternative;
  // every constructor, assignment and the visitor funnel through these
  template<json_value_kind K, class... Args>
  constexpr basic_json(kind_construct_t<K>, Args&&... args)
      : kind_(K), data_(std::in_place_index<detail::storage_index_of<K>>, std::forward<Args>(args)...)
  {
  }

  template<json_value_kind K, class... Args>
  static constexpr basic_json make(Args&&... args)
  {
    return basic_json(kind_construct_t<K>{}, std::forward<Args>(args)...);
  }

  template<json_value_kind K, class... Args>
  constexpr void emplace_kind(Args&&... args)
  {
    data_.template emplace<detail::storage_index_of<K>>(std::forward<Args>(args)...);
    kind_ = K;
  }

public:
  constexpr bool as_boolean_unchecked() const { return std::make_from_tuple<bool>(deserializer<bool, charT>::deserialize(std::get<0>(data_)).args); }

  template<class T>
  constexpr T as_unsigned_integer_unchecked() const
  {
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template<class T>
  constexpr T as_signed_integer_unchecked() const
  {
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  template<class T>
  constexpr T as_floating_point_unchecked() const
  {
    return std::make_from_tuple<T>(deserializer<T, charT>::deserialize(std::get<0>(data_)).args);
  }

  constexpr std::basic_string<charT> const& as_string_unchecked() const noexcept { return std::get<0>(data_); }

  constexpr std::vector<basic_json<charT>> const& as_array_unchecked() const noexcept { return std::get<1>(data_); }

  constexpr basic_json const& at_unchecked(std::size_t index) const noexcept { return std::get<1>(data_)[index]; }

  constexpr std::vector<std::pair<std::basic_string<charT>, basic_json>> const& as_object_unchecked() const noexcept { return std::get<2>(data_); }

  constexpr basic_json const& at_unchecked(std::basic_string_view<charT> key) const noexcept
  {
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    return iter->second;
  }

  constexpr bool as_boolean() const
  {
    if (get_kind() != json_value_kind::boolean) throw bad_json_access{};
    return as_boolean_unchecked();
  }

  template<class T>
  constexpr T as_unsigned_integer() const
  {
    if (get_kind() != json_value_kind::number_unsigned_integer) throw bad_json_access{};
    return as_unsigned_integer_unchecked<T>();
  }

  template<class T>
  constexpr T as_signed_integer() const
  {
    if (get_kind() != json_value_kind::number_signed_integer) throw bad_json_access{};
    return as_signed_integer_unchecked<T>();
  }

  template<class T>
  constexpr T as_floating_point() const
  {
    if (get_kind() != json_value_kind::number_floating_point) throw bad_json_access{};
    return as_floating_point_unchecked<T>();
  }

  constexpr std::basic_string<charT> const& as_string() const
  {
    if (get_kind() != json_value_kind::string) throw bad_json_access{};
    return as_string_unchecked();
  }

  constexpr std::vector<basic_json<charT>> const& as_array() const
  {
    if (get_kind() != json_value_kind::array) throw bad_json_access{};
    return as_array_unchecked();
  }

  constexpr std::vector<std::pair<std::basic_string<charT>, basic_json>> const& as_object() const
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    return as_object_unchecked();
  }

  constexpr std::optional<bool> try_as_boolean() const noexcept
  {
    if (get_kind() != json_value_kind::boolean) return std::nullopt;
    return as_boolean_unchecked();
  }

  template<class T>
  constexpr std::optional<T> try_as_unsigned_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_unsigned_integer) return std::nullopt;
    try {
      return as_unsigned_integer_unchecked<T>();
    } catch (...) {
      return std::nullopt;
    }
  }

  template<class T>
  constexpr std::optional<T> try_as_signed_integer() const noexcept
  {
    if (get_kind() != json_value_kind::number_signed_integer) return std::nullopt;
    try {
      return as_signed_integer_unchecked<T>();
    } catch (...) {
      return std::nullopt;
    }
  }

  template<class T>
  constexpr std::optional<T> try_as_floating_point() const noexcept
  {
    if (get_kind() != json_value_kind::number_floating_point) return std::nullopt;
    try {
      return as_floating_point_unchecked<T>();
    } catch (...) {
      return std::nullopt;
    }
  }

  constexpr std::basic_string<charT> const* try_as_string() const noexcept
  {
    if (get_kind() != json_value_kind::string) return nullptr;
    return &as_string_unchecked();
  }

  constexpr std::vector<basic_json<charT>> const* try_as_array() const noexcept
  {
    if (get_kind() != json_value_kind::array) return nullptr;
    return &as_array_unchecked();
  }

  constexpr std::vector<std::pair<std::basic_string<charT>, basic_json>> const* try_as_object() const noexcept
  {
    if (get_kind() != json_value_kind::object) return nullptr;
    return &as_object_unchecked();
  }

  constexpr basic_json const& at(std::size_t index) const
  {
    if (get_kind() != json_value_kind::array) throw bad_json_access{};
    return std::get<1>(data_).at(index);
  }

  constexpr basic_json const& at(std::basic_string_view<charT> key) const
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter == vec.end() || iter->first != key) throw std::out_of_range{"json has not such key"};
    return iter->second;
  }

  constexpr basic_json const* try_at(std::size_t index) const noexcept
  {
    if (get_kind() != json_value_kind::array) return nullptr;
    auto& vec = std::get<1>(data_);
    if (index >= vec.size()) return nullptr;
    return &vec[index];
  }

  constexpr basic_json const* try_at(std::basic_string_view<charT> key) const noexcept
  {
    if (get_kind() != json_value_kind::object) return nullptr;
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter == vec.end() || iter->first != key) return nullptr;
    return &iter->second;
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

  constexpr bool insert(std::basic_string_view<charT> key, basic_json const& json)
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter != vec.end() && iter->first == key) return false;
    vec.emplace(iter, key, json);
    return true;
  }

  template<class... Args>
  constexpr bool emplace(std::basic_string_view<charT> key, Args&&... args)
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter != vec.end() && iter->first == key) return false;
    vec.emplace(iter, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<Args>(args)...));
    return true;
  }

  constexpr bool erase(std::basic_string_view<charT> key)
  {
    if (get_kind() != json_value_kind::object) throw bad_json_access{};
    auto& vec = std::get<2>(data_);
    auto iter = std::ranges::lower_bound(vec, key, {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    if (iter == vec.end() || iter->first != key) return false;
    vec.erase(iter);
    return true;
  }

public:
  template<class charT2>
  friend class basic_json_visitor;

  constexpr basic_json()
      : basic_json(kind_construct_t<json_value_kind::null>{}, std::basic_string_view<charT>(YK_JSON20_WIDEN_STRING(charT, "null")))
  {
  }

  constexpr basic_json(bool x) : basic_json(kind_construct_t<json_value_kind::boolean>{}, serializer<bool, charT>::serialize(x)) {}

  template<std::unsigned_integral UInt>
  constexpr basic_json(UInt x) : basic_json(kind_construct_t<json_value_kind::number_unsigned_integer>{}, serializer<UInt, charT>::serialize(x))
  {
  }

  template<std::signed_integral Int>
  constexpr basic_json(Int x) : basic_json(kind_construct_t<json_value_kind::number_signed_integer>{}, serializer<Int, charT>::serialize(x))
  {
  }

  template<std::floating_point Float>
  constexpr basic_json(Float x) : basic_json(kind_construct_t<json_value_kind::number_floating_point>{}, serializer<Float, charT>::serialize(x))
  {
  }

  template<class S>
    requires std::is_convertible_v<S const&, std::basic_string_view<charT>>
  constexpr basic_json(S const& str) : basic_json(kind_construct_t<json_value_kind::string>{}, str)
  {
  }

  constexpr basic_json(std::initializer_list<basic_json> il) : basic_json(kind_construct_t<json_value_kind::array>{}, il) {}

  constexpr basic_json(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il)
      : basic_json(kind_construct_t<json_value_kind::object>{}, il)
  {
    std::ranges::sort(std::get<2>(data_), {}, &std::pair<std::basic_string<charT>, basic_json>::first);
  }

  template<std::unsigned_integral UInt>
  constexpr basic_json& operator=(UInt x)
  {
    emplace_kind<json_value_kind::number_unsigned_integer>(serializer<UInt, charT>::serialize(x));
    return *this;
  }

  template<std::signed_integral Int>
  constexpr basic_json& operator=(Int x)
  {
    emplace_kind<json_value_kind::number_signed_integer>(serializer<Int, charT>::serialize(x));
    return *this;
  }

  template<std::floating_point Float>
  constexpr basic_json& operator=(Float x)
  {
    emplace_kind<json_value_kind::number_floating_point>(serializer<Float, charT>::serialize(x));
    return *this;
  }

  template<class S>
    requires std::is_convertible_v<S const&, std::basic_string_view<charT>>
  constexpr basic_json& operator=(S const& str)
  {
    emplace_kind<json_value_kind::string>(str);
    return *this;
  }

  constexpr basic_json& operator=(std::initializer_list<basic_json> il)
  {
    emplace_kind<json_value_kind::array>(il);
    return *this;
  }

  constexpr basic_json& operator=(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il)
  {
    emplace_kind<json_value_kind::object>(il);
    std::ranges::sort(std::get<2>(data_), {}, &std::pair<std::basic_string<charT>, basic_json>::first);
    return *this;
  }

  static constexpr basic_json array() { return make<json_value_kind::array>(); }

  static constexpr basic_json array(std::initializer_list<basic_json> il) { return basic_json(std::move(il)); }

  static constexpr basic_json object() { return make<json_value_kind::object>(); }

  static constexpr basic_json object(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il) { return basic_json(std::move(il)); }

private:
  json_value_kind kind_;
  std::variant<std::basic_string<charT>, std::vector<basic_json>, std::vector<std::pair<std::basic_string<charT>, basic_json>>> data_;
};

using json = basic_json<char>;
using wjson = basic_json<wchar_t>;
using u8json = basic_json<char8_t>;
using u16json = basic_json<char16_t>;
using u32json = basic_json<char32_t>;

template<class charT>
class basic_json_visitor {
public:
  constexpr void on_null(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::null>(str.begin(), str.end()));
  }
  constexpr void on_boolean(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::boolean>(str.begin(), str.end()));
  }
  constexpr void on_number_unsigned_integer(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::number_unsigned_integer>(str.begin(), str.end()));
  }
  constexpr void on_number_signed_integer(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::number_signed_integer>(str.begin(), str.end()));
  }
  constexpr void on_number_floating_point(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::number_floating_point>(str.begin(), str.end()));
  }
  constexpr void on_string(std::basic_string_view<charT> str)
  {
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::string>(str.begin(), str.end()));
  }

  constexpr void on_array_start() { stack_.emplace_back(start_tag{}); }
  constexpr void on_array_finalize()
  {
    auto i = std::find_if(stack_.rbegin(), stack_.rend(), [](auto const& var) { return std::holds_alternative<start_tag>(var); }).base();
    std::vector<basic_json<charT>> vec;
    for (auto j = i; j != stack_.end(); ++j) {
      vec.emplace_back(std::get<basic_json<charT>>(std::move(*j)));
    }
    stack_.erase(i - 1, stack_.end());
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::array>(std::move(vec)));
  }
  constexpr void on_array_abort()
  {
    auto i = std::find_if(stack_.rbegin(), stack_.rend(), [](auto const& var) { return std::holds_alternative<start_tag>(var); }).base();
    stack_.erase(i - 1, stack_.end());
  }

  constexpr void on_object_start() { stack_.emplace_back(start_tag{}); }
  constexpr void on_object_finalize()
  {
    auto i = std::find_if(stack_.rbegin(), stack_.rend(), [](auto const& var) { return std::holds_alternative<start_tag>(var); }).base();
    std::vector<std::pair<std::basic_string<charT>, basic_json<charT>>> vec;
    for (auto j = i; j != stack_.end(); j += 2) {
      vec.emplace_back(std::get<basic_json<charT>>(*j).as_string(), std::get<basic_json<charT>>(*(j + 1)));
    }
    std::ranges::sort(vec, {}, &std::pair<std::basic_string<charT>, basic_json<charT>>::first);
    stack_.erase(i - 1, stack_.end());
    stack_.emplace_back(std::in_place_index<1>, basic_json<charT>::template make<json_value_kind::object>(std::move(vec)));
  }
  constexpr void on_object_abort()
  {
    auto i = std::find_if(stack_.rbegin(), stack_.rend(), [](auto const& var) { return std::holds_alternative<start_tag>(var); }).base();
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
    auto const control_characters_parser = alt(
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0000")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0001")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0002")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0003")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0004")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0005")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0006")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0007")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0008")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0009")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u000A")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u000B")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u000C")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u000D")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u000E")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u000F")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0010")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0011")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0012")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0013")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0014")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0015")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0016")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0017")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0018")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u0019")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u001A")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u001B")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u001C")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u001D")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u001E")),  //
        lit(YK_JSON20_WIDEN_STRING(charT, "\u001F"))   //
    );

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

#define YK_JSON20_INVOKE_VISITOR_SELECTER(_1, _2, _3, _4, _5, ...) _5
#define YK_JSON20_INVOKE_VISITOR(...) \
  YK_JSON20_INVOKE_VISITOR_SELECTER(__VA_ARGS__, YK_JSON20_INVOKE_VISITOR_ARG4, YK_JSON20_INVOKE_VISITOR_ARG3, YK_JSON20_INVOKE_VISITOR_ARG2)(__VA_ARGS__)

  template<class Visitor>
  static constexpr parse_result parse_null(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    constexpr auto null_string = YK_JSON20_WIDEN_STRING(charT, "null");

    auto res = lit(null_string)(str);
    if (res.match) {
      YK_JSON20_INVOKE_VISITOR(vis, on_null, *res.match, *res.match)
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
      YK_JSON20_INVOKE_VISITOR(vis, on_boolean, *res.match, *res.match)
    }
    return res;
  }

  template<class Visitor>
  static constexpr parse_result parse_number(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const parse0 = lit(YK_JSON20_WIDEN_STRING(charT, "0"));
    auto const parse1to9 =
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
        YK_JSON20_INVOKE_VISITOR(vis, on_number_floating_point, match, match);
      } else if (has_minus_sign) {
        YK_JSON20_INVOKE_VISITOR(vis, on_number_signed_integer, match, match);
      } else {
        YK_JSON20_INVOKE_VISITOR(vis, on_number_unsigned_integer, match, match);
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

    YK_JSON20_INVOKE_VISITOR(vis, on_array_start)
    auto res = parser(str);
    if (res.match) {
      std::basic_string_view<charT> match = consumed(str, res.rest);
      YK_JSON20_INVOKE_VISITOR(vis, on_array_finalize, match)
      return {match, res.rest};
    }
    YK_JSON20_INVOKE_VISITOR(vis, on_array_abort)
    return {std::nullopt, str};
  }

  template<class Visitor>
  static constexpr parse_result parse_string(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const quote = YK_JSON20_WIDEN_STRING(charT, "\"");
    auto const backslash = YK_JSON20_WIDEN_STRING(charT, "\\");

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
    YK_JSON20_INVOKE_VISITOR(vis, on_string, content, match)
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

    auto const key_value_parser =
        seq(ws, bind_visitor(parse_string<Visitor>, vis), ws, lit(colon), bind_visitor(parse_value<Visitor>, vis));
    auto const parser = seq(lit(open_brace), alt(sep_by(key_value_parser, lit(comma)), ws), lit(close_brace));

    YK_JSON20_INVOKE_VISITOR(vis, on_object_start)
    auto res = parser(str);
    if (res.match) {
      std::basic_string_view<charT> match = consumed(str, res.rest);
      YK_JSON20_INVOKE_VISITOR(vis, on_object_finalize, match)
      return {match, res.rest};
    }
    YK_JSON20_INVOKE_VISITOR(vis, on_object_abort)
    return {std::nullopt, str};
  }

  template<class Visitor>
  static constexpr parse_result parse_value(Visitor& vis, std::basic_string_view<charT> str) noexcept
  {
    auto const parser =
        seq(ws,
            alt(
                bind_visitor(parse_null<Visitor>, vis),     //
                bind_visitor(parse_boolean<Visitor>, vis),  //
                bind_visitor(parse_number<Visitor>, vis),   //
                bind_visitor(parse_string<Visitor>, vis),   //
                bind_visitor(parse_array<Visitor>, vis),    //
                bind_visitor(parse_object<Visitor>, vis)    //
            ),
            ws);

    auto res = parser(str);
    if (res.match) {
      return {consumed(str, res.rest), res.rest};
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

#endif  // YK_JSON20_HPP
