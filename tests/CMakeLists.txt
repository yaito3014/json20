include(CTest)

option(JSON20_CI "for CI" OFF)

set(CMAKE_COLOR_DIAGNOSTICS ${JSON20_CI})

function(yk_json20_add_test test_name)
    set(target_name yk_json20_${test_name}_test)
    add_executable(${target_name} ${test_name}.cpp)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    target_link_libraries(${target_name} PRIVATE json20)
    find_package(Catch2 CONFIG REQUIRED)
    target_link_libraries(${target_name} PRIVATE Catch2::Catch2WithMain)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

yk_json20_add_test(json)
yk_json20_add_test(main)
yk_json20_add_test(number)
yk_json20_add_test(parse)
yk_json20_add_test(util)

# EBCDIC support check: all assertions in ebcdic.cpp are static_asserts, so
# successful compilation is the actual test. Only possible where the compiler
# can target an EBCDIC execution character set (GCC with iconv; clang cannot).
# The probe checks that literals are actually translated, not merely that the
# flag is accepted: MSVC ignores unknown options with a warning.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-fexec-charset=IBM1047")
check_cxx_source_compiles(
    [[static_assert(static_cast<unsigned char>('A') == 0xC1, "not EBCDIC"); int main() {}]]
    YK_JSON20_HAS_EXEC_CHARSET_IBM1047
)
unset(CMAKE_REQUIRED_FLAGS)
if(YK_JSON20_HAS_EXEC_CHARSET_IBM1047)
    add_executable(yk_json20_ebcdic_test ebcdic.cpp)
    target_compile_features(yk_json20_ebcdic_test PRIVATE cxx_std_23)
    target_compile_options(yk_json20_ebcdic_test PRIVATE -fexec-charset=IBM1047)
    target_link_libraries(yk_json20_ebcdic_test PRIVATE json20)
    add_test(NAME yk_json20_ebcdic_test COMMAND yk_json20_ebcdic_test)
endif()
