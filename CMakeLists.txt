cmake_minimum_required(VERSION 3.23)

if(
    CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
    AND NOT DEFINED CMAKE_PROJECT_TOP_LEVEL_INCLUDES
)
    set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES
        "${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake"
    )
endif()

project(json20 VERSION 0.0.1 LANGUAGES CXX)

add_library(json20 INTERFACE)

target_compile_features(json20 INTERFACE cxx_std_20)

# single_include/ holds the same library as one generated file
# (tools/amalgamate.py); the option builds and installs that instead
option(
    JSON20_SINGLE_INCLUDE
    "use the generated single header in single_include/ instead of include/"
    OFF
)
if(JSON20_SINGLE_INCLUDE)
    set(YK_JSON20_INCLUDE_DIR single_include)
else()
    set(YK_JSON20_INCLUDE_DIR include)
endif()

file(
    GLOB_RECURSE YK_JSON20_HEADERS
    CONFIGURE_DEPENDS
    ${YK_JSON20_INCLUDE_DIR}/*.hpp
)

target_sources(
    json20
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS ${YK_JSON20_INCLUDE_DIR}
        FILES ${YK_JSON20_HEADERS}
)

if(PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory(tests)
endif()

add_library(yk::json20 ALIAS json20)

option(
    JSON20_INSTALL
    "generate install rules for json20"
    ${PROJECT_IS_TOP_LEVEL}
)

if(JSON20_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS json20 EXPORT json20_targets FILE_SET HEADERS)
    install(
        EXPORT json20_targets
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/json20
        FILE json20-targets.cmake
        NAMESPACE yk::
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/json20-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/json20-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/json20
    )
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/json20-config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/json20
    )
endif()
