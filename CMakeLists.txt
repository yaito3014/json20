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

# The option builds and installs the library as one file, generated from
# include/ by tools/amalgamate.py into the build tree, instead of the parts
option(
    JSON20_SINGLE_INCLUDE
    "use json20 as a single header generated from include/ (needs Python 3)"
    OFF
)
if(JSON20_SINGLE_INCLUDE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(YK_JSON20_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/single_include)
    file(
        GLOB_RECURSE YK_JSON20_PARTS
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    )
    # regenerated whenever a part or the script changes, through a reconfigure
    set_property(
        DIRECTORY
        APPEND
        PROPERTY
            CMAKE_CONFIGURE_DEPENDS
                ${YK_JSON20_PARTS}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate.py
    )
    execute_process(
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/amalgamate.py
            ${YK_JSON20_INCLUDE_DIR}/yk/json20.hpp
        COMMAND_ERROR_IS_FATAL ANY
    )
else()
    set(YK_JSON20_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
