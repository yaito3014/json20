add_executable(yk_json20_test main.cpp util.cpp parse.cpp)

target_link_libraries(yk_json20_test PRIVATE json20)

find_package(Boost CONFIG REQUIRED COMPONENTS unit_test_framework)
target_link_libraries(yk_json20_test PRIVATE Boost::unit_test_framework)
