cmake_minimum_required(VERSION 3.17)
project(test)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-O1 -ggdb)

include(FetchContent)
include(FetchLagom.cmake)

add_executable(test
        sauce/parser.cpp
        sauce/lexer.cpp
        sauce/main.cpp
        sauce/ast.cpp
        )

target_link_libraries(test PUBLIC Lagom::Core)
