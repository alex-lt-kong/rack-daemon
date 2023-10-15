cmake_minimum_required(VERSION 3.18.4)

project("Rack Daemon")
add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")

# Always generate compile_commands.json for clangd, etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 17)


set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_ASAN "Build with AddressSanitizer to detect memory error" OFF)
option(BUILD_UBSAN "Build with UndefinedBehaviorSanitizer to detect undefined behavior" OFF)

set(COUNTER 0)
set(ALL_OPTIONS BUILD_ASAN;BUILD_MSAN)
foreach(option IN LISTS ALL_OPTIONS)
    if(${option})
        math(EXPR COUNTER "${COUNTER}+1")
    endif()
endforeach()

if(${COUNTER} GREATER 1)
    message(FATAL_ERROR "Can't enable AddressSanitizer(BUILD_ASAN=ON), "
        "UndefinedBehaviorSanitizer(BUILD_UBSAN=ON) concurrently")
endif()


set(SANITIZER_NAME "None")
if(BUILD_ASAN)
    message("-- AddressSanitizer WILL be compiled in as BUILD_ASAN=ON")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
    set(SANITIZER_NAME "AddressSanitizer")
    add_definitions(-DSANITIZER_NAME="${SANITIZER_NAME}")    
else()
    message("-- AddressSanitizer will NOT be compiled in as BUILD_ASAN=OFF")
endif()
if(BUILD_UBSAN)
    message("-- UndefinedBehaviorSanitizer WILL be compiled in as BUILD_UBSAN=ON")
    add_compile_options(-fsanitize=undefined -g)
    add_link_options(-fsanitize=undefined)
    set(SANITIZER_NAME "UndefinedBehaviorSanitizer")
    add_definitions(-DSANITIZER_NAME="${SANITIZER_NAME}")
else()
    message("-- UndefinedBehaviorSanitizer will NOT be compiled in as BUILD_UBSAN=OFF")
endif()


add_compile_options(-Wall)
add_compile_options(-Wextra)
add_compile_options(-pedantic)
add_compile_options(-O3)


find_library(SQLITE3_LIB sqlite3)
if(NOT SQLITE3_LIB)
  message(FATAL_ERROR "sqlite3 library not found, install it with 'apt install libsqlite3-dev'")
endif()
find_package(cJSON REQUIRED)

add_subdirectory(src/back)
