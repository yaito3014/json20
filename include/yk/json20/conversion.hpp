#ifndef YK_JSON20_CONVERSION_HPP
#define YK_JSON20_CONVERSION_HPP

#include <yk/json20/detail/fixed_string.hpp>
#include <yk/json20/detail/number/format_float.hpp>
#include <yk/json20/detail/number/ieee754.hpp>
#include <yk/json20/detail/number/parse_float.hpp>

#include <charconv>
#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
// std::begin / std::end come from <string>: <iterator> does not compile in
// C++26 mode with libstdc++ 14 under an EBCDIC execution character set
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace yk {

namespace json20 {

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

template<class charT>
struct deserializer<bool, charT> {
  static constexpr std::basic_string_view<charT> true_ = YK_JSON20_WIDEN_STRING(charT, "true");
  static constexpr std::basic_string_view<charT> false_ = YK_JSON20_WIDEN_STRING(charT, "false");
  static constexpr auto deserialize(std::basic_string_view<charT> str)
  {
    if (str == true_) return make_deserialize_result<charT>(str.begin() + true_.size(), true);
    if (str == false_) return make_deserialize_result<charT>(str.begin() + false_.size(), false);
    throw std::invalid_argument("argument is not boolean");
  }
};

// Digits are decoded as offsets from the translated literal '0', which the
// standard guarantees to be followed by '1'..'9' in every encoding, so this
// works for any charT and execution character set (EBCDIC included), unlike
// the platform's from_chars.
template<class T, class charT>
  requires std::integral<T> && (!std::same_as<T, bool>)
struct deserializer<T, charT> {
  static constexpr auto deserialize(std::basic_string_view<charT> str)
  {
    constexpr auto zero = YK_JSON20_WIDEN_STRING(charT, "0");
    constexpr auto minus = YK_JSON20_WIDEN_STRING(charT, "-");
    using U = std::make_unsigned_t<T>;

    std::size_t i = 0;
    bool negative = false;
    if constexpr (std::is_signed_v<T>) {
      if (!str.empty() && str[0] == minus.data[0]) {
        negative = true;
        i = 1;
      }
    }
    // the magnitude of the most negative value is one more than the maximum
    U const limit = static_cast<U>(std::numeric_limits<T>::max()) + (negative ? 1 : 0);

    std::size_t const first_digit = i;
    U value = 0;
    for (; i < str.size(); ++i) {
      auto const offset = static_cast<long long>(str[i]) - static_cast<long long>(zero.data[0]);
      if (offset < 0 || offset > 9) break;
      U const digit = static_cast<U>(offset);
      if (value > (limit - digit) / 10) throw std::invalid_argument("integer out of range");
      value = static_cast<U>(value * 10 + digit);
    }
    if (i == first_digit) throw std::invalid_argument("no digits");

    T const result = negative ? static_cast<T>(U(0) - value) : static_cast<T>(value);
    return make_deserialize_result<charT>(str.begin() + i, result);
  }
};

template<class T, class charT>
  requires detail::number::self_converted<T>
struct deserializer<T, charT> {
  static constexpr auto deserialize(std::basic_string_view<charT> str)
  {
    auto const [value, consumed] = detail::number::parse_float<T>(str);
    return make_deserialize_result<charT>(str.begin() + consumed, value);
  }
};

// not converted by json20 itself: the platform's from_chars, char only
template<class T>
  requires std::floating_point<T> && (!detail::number::self_converted<T>)
struct deserializer<T, char> {
  static constexpr auto deserialize(std::string_view str)
  {
    T value{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc{}) throw std::invalid_argument("from_chars error");
    return make_deserialize_result<char>(str.begin() + (ptr - str.data()), std::move(value));
  }
};

namespace detail {

// JSON has no representation for infinity or NaN (RFC 8259), so serializers
// reject them; callers pick their own encoding, such as null or a string.
// Not std::isfinite: it is not constexpr before C++23, and including <cmath>
// for it alone costs about 0.16 s per translation unit (GCC 14).
template<std::floating_point T>
constexpr bool is_finite(T x) noexcept
{
  return -std::numeric_limits<T>::max() <= x && x <= std::numeric_limits<T>::max();
}

}  // namespace detail

template<class T, class charT = char>
struct serializer {
  static constexpr std::basic_string<charT> serialize(T const& x) = delete;
};

template<class charT>
struct serializer<bool, charT> {
  static constexpr std::basic_string_view<charT> true_ = YK_JSON20_WIDEN_STRING(charT, "true");
  static constexpr std::basic_string_view<charT> false_ = YK_JSON20_WIDEN_STRING(charT, "false");
  static constexpr std::basic_string<charT> serialize(bool const& x)
  {
    auto const s = x ? true_ : false_;
    // a pointer range, not the string_view constructor: clang (18) cannot
    // constant-evaluate libstdc++'s for char8_t
    return std::basic_string<charT>(s.data(), s.data() + s.size());
  }
};

template<class T, class charT>
  requires std::integral<T> && (!std::same_as<T, bool>)
struct serializer<T, charT> {
  static constexpr std::basic_string<charT> serialize(T const& x)
  {
    constexpr auto minus = YK_JSON20_WIDEN_STRING(charT, "-");
    using U = std::make_unsigned_t<T>;

    bool negative = false;
    U value = static_cast<U>(x);
    if constexpr (std::is_signed_v<T>) {
      if (x < 0) {
        negative = true;
        value = static_cast<U>(U(0) - value);
      }
    }

    // digits10 rounds down, so +2 covers every digit plus the sign
    charT buf[std::numeric_limits<U>::digits10 + 2]{};
    charT* const last = std::end(buf);
    charT* first = detail::number::write_digits(last, value);
    if (negative) *--first = minus.data[0];
    return std::basic_string<charT>(first, last);
  }
};

template<class T, class charT>
  requires detail::number::self_converted<T>
struct serializer<T, charT> {
  static constexpr std::basic_string<charT> serialize(T const& x)
  {
    if (!detail::is_finite(x)) throw std::invalid_argument("JSON cannot represent infinity or NaN");
    return detail::number::format_float<charT>(x);
  }
};

// not converted by json20 itself: the platform's to_chars, char only
template<class T>
  requires std::floating_point<T> && (!detail::number::self_converted<T>)
struct serializer<T, char> {
  static std::string serialize(T const& x)
  {
    if (!detail::is_finite(x)) throw std::invalid_argument("JSON cannot represent infinity or NaN");
    // shortest form needs max_digits10 digits, sign, dot, and "e+XXXX"
    char buf[std::numeric_limits<T>::max_digits10 + 8];
    auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf), x);
    if (ec != std::errc{}) throw std::runtime_error("to_chars error");
    return std::string(buf, ptr);
  }
};

}  // namespace json20

}  // namespace yk

#endif  // YK_JSON20_CONVERSION_HPP
