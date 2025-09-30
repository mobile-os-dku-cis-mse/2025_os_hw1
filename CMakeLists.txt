cmake_minimum_required(VERSION 3.10)

project(SiSH)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -O2)

include_directories(include)

set(SOURCES
        src/main.c
        src/helpers.c
        src/parsing.c
        src/exec.c
)

add_executable(sish ${SOURCES})
