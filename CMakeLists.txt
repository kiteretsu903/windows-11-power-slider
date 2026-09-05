cmake_minimum_required(VERSION 3.24)
project(PowerModeNative VERSION 1.0.4 LANGUAGES CXX RC)

add_executable(PowerModeNative WIN32
    src/main.cpp
    src/power_controller.cpp
    src/power_controller.h
    src/tray_registration.h
    src/launch_options.h
    src/version.h
    src/renderer.cpp
    src/renderer.h
    src/PowerModeNative.ico
    src/resources.rc
    src/PowerModeNative.manifest
)

target_compile_features(PowerModeNative PRIVATE cxx_std_20)
target_compile_definitions(PowerModeNative PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

if(MSVC)
    target_compile_options(PowerModeNative PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_link_options(PowerModeNative PRIVATE /MANIFEST:NO)
else()
    target_compile_options(PowerModeNative PRIVATE -Wall -Wextra -Wpedantic -municode)
    target_link_options(PowerModeNative PRIVATE -municode -static-libgcc -static-libstdc++)
endif()

target_link_libraries(PowerModeNative PRIVATE
    advapi32
    d2d1
    d3d11
    dxgi
    dcomp
    windowscodecs
    dwrite
    dwmapi
    ole32
    gdi32
    shell32
    user32
)

install(TARGETS PowerModeNative RUNTIME DESTINATION .)
include(CTest)
if(BUILD_TESTING)
    add_executable(test-tray-registration tools/test-tray-registration.cpp)
    target_include_directories(test-tray-registration PRIVATE src)
    target_compile_features(test-tray-registration PRIVATE cxx_std_20)
    target_compile_definitions(test-tray-registration PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(test-tray-registration PRIVATE /UNDEBUG /W4)
    else()
        target_compile_options(test-tray-registration PRIVATE -UNDEBUG -Wall -Wextra)
        target_link_options(test-tray-registration PRIVATE -static)
    endif()
    target_link_libraries(test-tray-registration PRIVATE user32 gdi32)
    add_test(NAME tray-registration COMMAND test-tray-registration)
endif()
install(FILES assets/fluent/LICENSE.txt DESTINATION . RENAME Fluent-Icons-LICENSE.txt)
add_custom_command(TARGET PowerModeNative POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fluent/LICENSE.txt"
    "$<TARGET_FILE_DIR:PowerModeNative>/Fluent-Icons-LICENSE.txt")
