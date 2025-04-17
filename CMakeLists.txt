cmake_minimum_required(VERSION 3.30)
project(computer_plays_factorio LANGUAGES CXX)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/factorioAPI.hpp
    src/factorioAPI.cpp
    src/thread.hpp
    src/thread.cpp
    src/serializer.hpp
    src/serializer.cpp
)

set_property(TARGET computer_plays_factorio PROPERTY CXX_STANDARD 20)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/lua/ lua)