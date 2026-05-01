cmake_minimum_required(VERSION 3.20)
project(ilibcxx VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB ILIBC_SOURCES CONFIGURE_DEPENDS src/*.cpp)

set(ILIBCXX_EXTRAS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extras/include)
set(ILIBCXX_EXTRAS_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extras/lib)
set(ILIBCXX_FREETYPE_ARCHIVE ${ILIBCXX_EXTRAS_LIB_DIR}/libfreetype.a)

add_library(ilibcxx STATIC ${ILIBC_SOURCES})
add_library(ilibcxx::ilibcxx ALIAS ilibcxx)

target_include_directories(ilibcxx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${ILIBCXX_EXTRAS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(ilibcxx
    PUBLIC
        ${ILIBCXX_FREETYPE_ARCHIVE}
)
target_compile_options(ilibcxx PRIVATE ${USER_CXX_FLAGS} ${SIZE_CXX_FLAGS})

install(TARGETS ilibcxx
    EXPORT ilibcxxTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY extras/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${ILIBCXX_FREETYPE_ARCHIVE} DESTINATION ${CMAKE_INSTALL_LIBDIR})

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ilibcxxConfigVersion.cmake"
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ilibcxxConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ilibcxxConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ilibcxx"
)

install(EXPORT ilibcxxTargets
    FILE ilibcxxTargets.cmake
    NAMESPACE ilibcxx::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ilibcxx
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ilibcxxConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ilibcxxConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ilibcxx"
)
