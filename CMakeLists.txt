cmake_minimum_required(VERSION 3.30)
project(computer_plays_factorio LANGUAGES CXX)

set(SOURCE_DATA_DIR ${CMAKE_SOURCE_DIR}/data)
set(OUTPUT_DATA_DIR ${CMAKE_BINARY_DIR}/data)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_PROFILE "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_PROFILE "${CMAKE_BINARY_DIR}/bin")

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/bot/bot.hpp
    src/bot/bot.cpp
    src/bot/task.hpp
    src/bot/task.cpp
    src/bot/event.hpp
    src/bot/event.cpp
    src/bot/mapData.hpp
    src/bot/mapData.cpp
    src/factorioAPI/factorioAPI.hpp
    src/factorioAPI/factorioAPI.cpp
    src/factorioAPI/factorioAPI_linux.cpp
    src/factorioAPI/factorioAPI_windows.cpp
    src/factorioAPI/factorioData.hpp
    src/factorioAPI/types.hpp
    src/algorithms/pathFinder.hpp
    src/algorithms/pathFinder.cpp
    src/utils/luaUtils.hpp
    src/utils/thread.hpp
    src/utils/thread.cpp
    src/utils/logging.hpp
    src/utils/logging.cpp
    src/utils/utils.hpp
    src/utils/utils.cpp
    src/utils/typeTraits.hpp
    src/utils/includeReflectcpp.hpp
    src/utils/inicpp.h
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

if(MSVC)
    # target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX /Zc:preprocessor)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_subdirectory(external/lua-5.4.8)

include(FetchContent)
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG        v1.0.2 # <HASH or TAG>
)
FetchContent_MakeAvailable(cpptrace)

FetchContent_Declare(
    reflectcpp
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/getml/reflect-cpp/archive/refs/tags/v0.19.0.tar.gz
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(reflectcpp)

target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace lua_lib reflectcpp)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(UNIX)
        add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${SOURCE_DATA_DIR} ${OUTPUT_DATA_DIR}
        )
    else()
        file(TO_NATIVE_PATH "${OUTPUT_DATA_DIR}" OUTPUT_DATA_DIR_WIN)
        file(TO_NATIVE_PATH "${SOURCE_DATA_DIR}" SOURCE_DATA_DIR_WIN)
        execute_process(COMMAND cmd.exe /c mklink /J "${OUTPUT_DATA_DIR_WIN}" "${SOURCE_DATA_DIR_WIN}")
    endif()
else()
    file(COPY ${SOURCE_DATA_DIR} DESTINATION ${OUTPUT_DATA_DIR})
endif()