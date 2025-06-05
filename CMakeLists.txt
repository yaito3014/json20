cmake_minimum_required(VERSION 3.23)

project(json20 VERSION 0.0.1 LANGUAGES CXX)

option(BUILD_TESTING "Build tests" PROJECT_IS_TOP_LEVEL)

add_library(json20 INTERFACE)

target_compile_features(json20 INTERFACE cxx_std_20)

file(GLOB_RECURSE YK_JSON20_HEADERS CONFIGURE_DEPENDS include/*.hpp)

target_sources(
    json20
    INTERFACE FILE_SET HEADERS BASE_DIRS include FILES ${YK_JSON20_HEADERS}
)

include(CTest)
add_subdirectory(tests EXCLUDE_FROM_ALL)

include(GNUInstallDirs)

add_library(yk::json20 ALIAS json20)

install(TARGETS json20 EXPORT json20_targets FILE_SET HEADERS)
install(
    EXPORT json20_targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/json20
    FILE json20-targets.cmake
    NAMESPACE yk
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/json20-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/json20-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/json20
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/json20-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/json20
)
