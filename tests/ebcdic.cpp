// Compile-time verification that the grammar tables are written in terms of
// Unicode code points, not raw code units: compiled with an EBCDIC execution
// character set (-fexec-charset=IBM1047), EBCDIC-encoded JSON must parse and
// the control-character rules must follow the code points' EBCDIC positions.
// Raw bytes are injected with hex escapes, which deliberately bypass the
// compiler's literal translation.

#include <yk/json20.hpp>

using parser = yk::json20::basic_json_parser<char>;

// EBCDIC-encoded JSON parses: every literal below is translated to IBM1047
static_assert(parser::try_parse("{ \"a\" : [ true, null, 12 ] }").has_value());

// the JSON escape \n decodes to U+000A = EBCDIC LF 0x25, not NL 0x15
static_assert(static_cast<unsigned char>(parser::parse("\"x\\ny\"").as_string_unchecked()[1]) == 0x25);

// a raw untranslated LF byte (0x25 = U+000A here) inside a string is rejected
static_assert(!parser::try_parse("\"a\x25" "b\"").has_value());

// but a raw NEL byte (0x15 = U+0085 here) is allowed: JSON bans only U+0000..U+001F
static_assert(parser::try_parse("\"a\x15" "b\"").has_value());

// integer access decodes EBCDIC digits instead of handing them to the
// platform's ASCII-assuming from_chars
static_assert(parser::parse("12").as_unsigned_integer<unsigned>() == 12);
static_assert(parser::parse("-12").as_signed_integer<int>() == -12);
static_assert(parser::parse("18446744073709551615").as_unsigned_integer<unsigned long long>() == 18446744073709551615ull);
static_assert(parser::parse("-9223372036854775808").as_signed_integer<long long>() == -9223372036854775807ll - 1);

// serialized integers are spelled in EBCDIC digits (0xF4 is '4')
static_assert(static_cast<unsigned char>(yk::json20::json(42u).as_string_unchecked()[0]) == 0xF4);
static_assert(yk::json20::json(-7).as_string_unchecked() == "-7");

int main() {}
