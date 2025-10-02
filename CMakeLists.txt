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
    src/projects/test.hpp
    src/projects/test.cpp

    src/main.cpp
    src/app.hpp
    src/app.cpp
    src/bot/bot.hpp
    src/bot/bot.cpp
    src/bot/task.hpp
    src/bot/task.cpp
    src/bot/map-data.hpp
    src/bot/map-data.cpp
    src/factorio-API/prototypes.hpp
    src/factorio-API/prototypes.cpp
    src/factorio-API/factorio-API.hpp
    src/factorio-API/factorio-API.cpp
    src/factorio-API/types.hpp
    src/factorio-API/types.cpp
    src/algorithms/path-finder.hpp
    src/algorithms/path-finder.cpp
    src/algorithms/TSP.hpp
    src/utils/thread.hpp
    src/utils/thread.cpp
    src/utils/logging.hpp
    src/utils/logging.cpp
    src/utils/utils.hpp
    src/utils/utils.cpp
    src/utils/inicpp.h
    src/utils/base64.h
    src/utils/base64.cpp
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX /Zc:preprocessor)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/PROFILE")
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(FetchContent)
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG        v1.0.2 # <HASH or TAG>
)
FetchContent_MakeAvailable(cpptrace)

FetchContent_Declare(
    json
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    zlib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(zlib)

target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace nlohmann_json::nlohmann_json zlib)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(TO_NATIVE_PATH "${OUTPUT_DATA_DIR}" OUTPUT_DATA_DIR_WIN)
    file(TO_NATIVE_PATH "${SOURCE_DATA_DIR}" SOURCE_DATA_DIR_WIN)
    execute_process(COMMAND cmd.exe /c mklink /J "${OUTPUT_DATA_DIR_WIN}" "${SOURCE_DATA_DIR_WIN}")
else()
    file(COPY ${SOURCE_DATA_DIR} DESTINATION ${OUTPUT_DATA_DIR})
endif()