cmake_minimum_required(VERSION 3.16)
project(xpresspp_core CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(xpresspp INTERFACE)
add_library(xpresspp::xpresspp ALIAS xpresspp)

target_compile_features(xpresspp INTERFACE cxx_std_20)

target_include_directories(xpresspp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Drogon REQUIRED)
find_package(Jsoncpp REQUIRED)
find_package(OpenSSL REQUIRED)

target_link_libraries(xpresspp INTERFACE Drogon::Drogon)
target_link_libraries(xpresspp INTERFACE OpenSSL::Crypto)

if(TARGET Jsoncpp_lib)
    target_link_libraries(xpresspp INTERFACE Jsoncpp_lib)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(xpresspp INTERFACE JsonCpp::JsonCpp)
else()
    target_include_directories(xpresspp INTERFACE /usr/include/jsoncpp)
    target_link_libraries(xpresspp INTERFACE jsoncpp)
endif()

install(TARGETS xpresspp EXPORT xpressppTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN "CMakeLists.txt" EXCLUDE)
install(EXPORT xpressppTargets
    FILE xpressppTargets.cmake
    NAMESPACE xpresspp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xpresspp)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/xpressppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/xpressppConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xpresspp)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/xpressppConfigVersion.cmake
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/xpressppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/xpressppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xpresspp)
