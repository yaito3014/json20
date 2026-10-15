# Prototype comparison of two ways to provide the power-of-five table (see
# docs/design-notes.md). verify.cpp checks both against exact values with
# static_asserts, so building it also shows whether each compiler can
# constant-evaluate variant B's generated table within its default limits.
add_executable(yk_json20_pow5_table_verify verify.cpp)
target_compile_features(yk_json20_pow5_table_verify PRIVATE cxx_std_20)
add_test(NAME yk_json20_pow5_table_verify COMMAND yk_json20_pow5_table_verify)
