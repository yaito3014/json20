#ifndef YK_ZZ_JSON20_VISITOR_HPP
#define YK_ZZ_JSON20_VISITOR_HPP

#include <yk/json20/json.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <cassert>

namespace yk::json20 {

template<class charT>
class basic_json_visitor {
public:
  constexpr basic_json_visitor() = default;
  constexpr explicit basic_json_visitor(parse_options options) noexcept : options_(options) {}

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
    bool const duplicated = basic_json<charT>::normalize_object(vec, options_.duplicate_keys == duplicate_key_policy::last_wins);
    if (duplicated && options_.duplicate_keys == duplicate_key_policy::reject) has_rejected_duplicate_key_ = true;
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

  // Set when an object repeated a key under duplicate_key_policy::reject.
  // The DOM is still built (keeping the first member) so get() stays valid,
  // but parse() and try_parse() treat the input as rejected.
  constexpr bool has_rejected_duplicate_key() const noexcept { return has_rejected_duplicate_key_; }

private:
  struct start_tag {};
  std::vector<std::variant<start_tag, basic_json<charT>>> stack_;
  parse_options options_{};
  bool has_rejected_duplicate_key_ = false;
};

using json_visitor = basic_json_visitor<char>;
using wjson_visitor = basic_json_visitor<wchar_t>;
using u8json_visitor = basic_json_visitor<char8_t>;
using u16json_visitor = basic_json_visitor<char16_t>;
using u32json_visitor = basic_json_visitor<char32_t>;

}  // namespace yk::json20

#endif  // YK_ZZ_JSON20_VISITOR_HPP
