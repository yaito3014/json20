#ifndef YK_ZZ_JSON20_JSON_HPP
#define YK_ZZ_JSON20_JSON_HPP

#include <yk/json20/conversion.hpp>
#include <yk/json20/detail/select_str.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace yk {

namespace json20 {

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

// What building a DOM does when an object repeats a key. The parser itself
// passes every member to the visitor; the policy only decides which one
// basic_json keeps, so every basic_json has unique keys whatever was chosen.
enum class duplicate_key_policy {
  reject,
  first_wins,
  last_wins,
};

struct parse_options {
  duplicate_key_policy duplicate_keys = duplicate_key_policy::reject;
};

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

private:
  using object_type = std::vector<std::pair<std::basic_string<charT>, basic_json>>;

  // Sorts members by key and keeps one member per key: the first or the last
  // in input order. Returns whether any key occurred more than once.
  // Input order breaks ties explicitly because std::stable_sort is not
  // constexpr before C++26.
  static constexpr bool normalize_object(object_type& members, bool keep_last)
  {
    std::vector<std::size_t> order(members.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
      auto const c = members[a].first.compare(members[b].first);
      return c < 0 || (c == 0 && a < b);
    });

    bool duplicated = false;
    object_type result;
    result.reserve(members.size());
    for (std::size_t i = 0; i < order.size();) {
      std::size_t j = i + 1;
      while (j < order.size() && members[order[j]].first == members[order[i]].first) ++j;
      duplicated = duplicated || j - i > 1;
      result.push_back(std::move(members[keep_last ? order[j - 1] : order[i]]));
      i = j;
    }
    members = std::move(result);
    return duplicated;
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

  // Not a string: before C++23, nullptr converts to basic_string_view through
  // a null pointer. Null is basic_json().
  basic_json(std::nullptr_t) = delete;

  constexpr basic_json(std::initializer_list<basic_json> il) : basic_json(kind_construct_t<json_value_kind::array>{}, il) {}

  constexpr basic_json(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il)
      : basic_json(kind_construct_t<json_value_kind::object>{}, il)
  {
    if (normalize_object(std::get<2>(data_), false)) throw std::invalid_argument("duplicate key in object");
  }

  // bool satisfies std::unsigned_integral; this exact match keeps it a boolean
  constexpr basic_json& operator=(bool x)
  {
    emplace_kind<json_value_kind::boolean>(serializer<bool, charT>::serialize(x));
    return *this;
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

  basic_json& operator=(std::nullptr_t) = delete;

  constexpr basic_json& operator=(std::initializer_list<basic_json> il)
  {
    emplace_kind<json_value_kind::array>(il);
    return *this;
  }

  constexpr basic_json& operator=(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il)
  {
    object_type members(il);
    if (normalize_object(members, false)) throw std::invalid_argument("duplicate key in object");
    emplace_kind<json_value_kind::object>(std::move(members));
    return *this;
  }

  static constexpr basic_json array() { return make<json_value_kind::array>(); }

  static constexpr basic_json array(std::initializer_list<basic_json> il) { return basic_json(std::move(il)); }

  static constexpr basic_json object() { return make<json_value_kind::object>(); }

  static constexpr basic_json object(std::initializer_list<std::pair<std::basic_string<charT>, basic_json>> il) { return basic_json(std::move(il)); }

private:
  json_value_kind kind_;
  std::variant<std::basic_string<charT>, std::vector<basic_json>, object_type> data_;
};

using json = basic_json<char>;
using wjson = basic_json<wchar_t>;
using u8json = basic_json<char8_t>;
using u16json = basic_json<char16_t>;
using u32json = basic_json<char32_t>;

}  // namespace json20

}  // namespace yk

#endif  // YK_ZZ_JSON20_JSON_HPP
