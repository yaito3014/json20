#include <yk/json20.hpp>

#define BOOST_TEST_MODULE yk_json20
#include <boost/test/unit_test.hpp>

#include <print>

BOOST_AUTO_TEST_SUITE(debug)

BOOST_AUTO_TEST_CASE(visit)
{
  struct print_visitor {
    void on_null(std::string_view str) noexcept { std::println("on_null: {:?}", str); }
    void on_boolean(std::string_view str) noexcept { std::println("on_boolean: {:?}", str); }
    void on_number_unsigned_integer(std::string_view str) noexcept { std::println("on_number_unsigned_integer: {:?}", str); }
    void on_number_signed_integer(std::string_view str) noexcept { std::println("on_number_signed_integer: {:?}", str); }
    void on_number_floating_point(std::string_view str) noexcept { std::println("on_number_floating_point: {:?}", str); }
    void on_string(std::string_view str) noexcept { std::println("on_string: {:?}", str); }

    void on_array_start() { std::println("on_array_start"); }
    void on_array_finalize() { std::println("on_array_finalize"); }
    void on_array_abort() { std::println("on_array_abort"); }

    void on_object_start() { std::println("on_object_start"); }
    void on_object_finalize() { std::println("on_object_finalize"); }
    void on_object_abort() { std::println("on_object_abort"); }
  };

  std::println("==begin==");
  print_visitor vis;
  yk::json20::basic_json_parser<char>::parse(vis, "[  ]");
  std::println("==end==");

  {
    yk::json20::basic_json_visitor<char> json_visitor;
    json_visitor.on_object_start();                  // prepare map
    json_visitor.on_string("foo");                   // parse key
    json_visitor.on_number_unsigned_integer("123");  // parse uint
    json_visitor.on_object_finalize();               // insert into map, construct json
    auto const json = json_visitor.get();
  }
  {
    yk::json20::basic_json_visitor<char> json_visitor;
    json_visitor.on_object_start();                  // prepare map
    json_visitor.on_object_abort();                  // abort map
    json_visitor.on_number_unsigned_integer("123");  // assign value
    auto const json = json_visitor.get();
  }
}

BOOST_AUTO_TEST_SUITE_END()
