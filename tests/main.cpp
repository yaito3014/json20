#include <yk/json20.hpp>

#include <catch2/catch_test_macros.hpp>

#include <iostream>

TEST_CASE("visit", "[debug]")
{
  struct print_visitor {
    void on_null(std::string_view str) noexcept { std::cout << "on_null: " << str << '\n'; }
    void on_boolean(std::string_view str) noexcept { std::cout << "on_boolean: " << str << '\n'; }
    void on_number_unsigned_integer(std::string_view str) noexcept { std::cout << "on_number_unsigned_integer: " << str << '\n'; }
    void on_number_signed_integer(std::string_view str) noexcept { std::cout << "on_number_signed_integer: " << str << '\n'; }
    void on_number_floating_point(std::string_view str) noexcept { std::cout << "on_number_floating_point: " << str << '\n'; }
    void on_string(std::string_view str) noexcept { std::cout << "on_string: " << str << '\n'; }

    void on_array_start() { std::cout << "on_array_start\n"; }
    void on_array_finalize() { std::cout << "on_array_finalize\n"; }
    void on_array_abort() { std::cout << "on_array_abort\n"; }

    void on_object_start() { std::cout << "on_object_start\n"; }
    void on_object_finalize() { std::cout << "on_object_finalize\n"; }
    void on_object_abort() { std::cout << "on_object_abort\n"; }
  };

  std::cout << "==begin==\n";
  print_visitor vis;
  yk::json20::basic_json_parser<char>::parse(vis, "[  ]");
  std::cout << "==end==\n";

  {
    yk::json20::basic_json_visitor<char> json_visitor;
    json_visitor.on_object_start();
    json_visitor.on_string("foo");
    json_visitor.on_number_unsigned_integer("123");
    json_visitor.on_object_finalize();
    auto const json = json_visitor.get();
  }
  {
    yk::json20::basic_json_visitor<char> json_visitor;
    json_visitor.on_object_start();
    json_visitor.on_object_abort();
    json_visitor.on_number_unsigned_integer("123");
    auto const json = json_visitor.get();
  }
}
