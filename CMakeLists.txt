
cmake_minimum_required(VERSION 3.20)

project(DeezerDesktop
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)
find_package(X11 REQUIRED)

# CEF SDK location
set(CEF_ROOT "" CACHE PATH "Path to the CEF binary distribution")

if(NOT CEF_ROOT)
    message(FATAL_ERROR
        "CEF SDK not found. Configure with -DCEF_ROOT=/path/to/cef"
    )
endif()

if(NOT EXISTS "${CEF_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "Invalid CEF_ROOT: ${CEF_ROOT}"
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

set(USE_SANDBOX OFF)

find_package(CEF REQUIRED)

add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper)

add_executable(deezer-desktop
    src/main.cpp
    src/mpriscontroller.cpp
    src/mpriscontroller.h
    src/settingsdialog.cpp
    src/settingsdialog.h
)

target_include_directories(deezer-desktop
    PRIVATE
        "${CEF_INCLUDE_PATH}"
)

target_link_libraries(deezer-desktop
    PRIVATE
        Qt6::Widgets
        Qt6::DBus
        X11::X11
        "${CEF_ROOT}/Release/libcef.so"
        libcef_dll_wrapper
        ${CEF_STANDARD_LIBS}
)

set_target_properties(deezer-desktop PROPERTIES
    BUILD_RPATH "${CEF_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN"
)

# CEF runtime files
set(CEF_RUNTIME_DIR "$<TARGET_FILE_DIR:deezer-desktop>")

foreach(CEF_FILE IN LISTS CEF_BINARY_FILES)
    if(NOT CEF_FILE STREQUAL "libcef.so"
       AND NOT CEF_FILE STREQUAL "chrome-sandbox")
        add_custom_command(TARGET deezer-desktop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_BINARY_DIR}/${CEF_FILE}"
                "${CEF_RUNTIME_DIR}/${CEF_FILE}"
            VERBATIM
        )
    endif()
endforeach()

# Copy CEF resources
foreach(CEF_FILE IN LISTS CEF_RESOURCE_FILES)
    if(IS_DIRECTORY "${CEF_RESOURCE_DIR}/${CEF_FILE}")
        add_custom_command(TARGET deezer-desktop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CEF_RESOURCE_DIR}/${CEF_FILE}"
                "${CEF_RUNTIME_DIR}/${CEF_FILE}"
            VERBATIM
        )
    else()
        add_custom_command(TARGET deezer-desktop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CEF_RESOURCE_DIR}/${CEF_FILE}"
                "${CEF_RUNTIME_DIR}/${CEF_FILE}"
            VERBATIM
        )
    endif()
endforeach()
