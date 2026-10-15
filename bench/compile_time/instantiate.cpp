#include <yk/json20.hpp>
// instantiates the full parser + DOM visitor for every character type
template<class C> auto f(std::basic_string_view<C> s) { return yk::json20::basic_json_parser<C>::try_parse(s); }
template auto f<char>(std::basic_string_view<char>);
template auto f<wchar_t>(std::basic_string_view<wchar_t>);
template auto f<char8_t>(std::basic_string_view<char8_t>);
template auto f<char16_t>(std::basic_string_view<char16_t>);
template auto f<char32_t>(std::basic_string_view<char32_t>);
