cmake_minimum_required(VERSION 3.30)
project(computer_plays_factorio LANGUAGES CXX)

set(luaSrc ${CMAKE_SOURCE_DIR}/lua)
set(luaDst ${CMAKE_BINARY_DIR}/lua)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
    src/main.cpp
    src/gui.hpp
    src/gui.cpp
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

if (UNIX)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${luaSrc} ${luaDst}
    )
else()
    file(TO_NATIVE_PATH "${luaDst}" _dstDir)
    file(TO_NATIVE_PATH "${luaSrc}" _srcDir)
    execute_process(COMMAND cmd.exe /c mklink /J "${_dstDir}" "${_srcDir}")
endif()