add_executable(yk_json20_test main.cpp util.cpp parse.cpp json.cpp)

target_compile_features(yk_json20_test PRIVATE cxx_std_23)

target_link_libraries(yk_json20_test PRIVATE json20)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(yk_json20_test PRIVATE Catch2::Catch2WithMain)
