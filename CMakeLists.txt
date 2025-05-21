cmake_minimum_required(VERSION 3.30)
project(computer_plays_factorio LANGUAGES CXX)

set(dataSrc ${CMAKE_SOURCE_DIR}/data)
set(dataDst ${CMAKE_BINARY_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
    src/main.cpp
    src/gui/gui.hpp
    src/gui/gui.cpp
    src/bot/bot.hpp
    src/bot/bot.cpp
    src/factorioAPI/factorioAPI.hpp
    src/factorioAPI/factorioAPI.cpp
    src/factorioAPI/types.hpp
    src/utils/thread.hpp
    src/utils/thread.cpp
    src/utils/utils.hpp
    src/utils/utils.cpp
    src/utils/serializer.hpp
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

set_property(TARGET computer_plays_factorio PROPERTY CXX_STANDARD 20)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX /Zc:preprocessor)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(UNIX)
        add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink ${dataSrc} ${dataDst}/data
        )
    else()
        file(TO_NATIVE_PATH "${dataDst}/data" _dstDir)
        file(TO_NATIVE_PATH "${dataSrc}" _srcDir)
        execute_process(COMMAND cmd.exe /c mklink /J "${_dstDir}" "${_srcDir}")
    endif()
else()
    file(COPY ${dataSrc} DESTINATION ${dataDst})
endif()