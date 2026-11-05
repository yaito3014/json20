#ifndef YK_ZZ_JSON20_DETAIL_FIXED_STRING_HPP
#define YK_ZZ_JSON20_DETAIL_FIXED_STRING_HPP

#include <algorithm>
#include <array>
#include <string_view>

#include <cstddef>

namespace yk::json20::detail {

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

}  // namespace yk::json20::detail

#endif  // YK_ZZ_JSON20_DETAIL_FIXED_STRING_HPP
