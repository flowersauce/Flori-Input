cmake_minimum_required(VERSION 3.30)

set(FLORI_INPUT_VERSION 1.4.0)
project(Flori-Input VERSION ${FLORI_INPUT_VERSION} LANGUAGES CXX RC)

find_package(Slint 1.17.1 EXACT CONFIG REQUIRED)
find_package(glaze 8.3.0 EXACT CONFIG REQUIRED)

set(FLORI_INPUT_RESOURCE_DIR "${PROJECT_SOURCE_DIR}/resources")
file(READ "${FLORI_INPUT_RESOURCE_DIR}/themes/dark.jsonc" UI_DARK)
file(READ "${FLORI_INPUT_RESOURCE_DIR}/themes/light.jsonc" UI_LIGHT)
file(READ "${FLORI_INPUT_RESOURCE_DIR}/languages/zh-CN.jsonc" UI_ZH)
file(READ "${FLORI_INPUT_RESOURCE_DIR}/languages/en-US.jsonc" UI_EN)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${FLORI_INPUT_RESOURCE_DIR}/themes/dark.jsonc"
        "${FLORI_INPUT_RESOURCE_DIR}/themes/light.jsonc"
        "${FLORI_INPUT_RESOURCE_DIR}/languages/zh-CN.jsonc"
        "${FLORI_INPUT_RESOURCE_DIR}/languages/en-US.jsonc"
)
configure_file(src/app/ui_resource_defaults.h.in ui_resource_defaults.h @ONLY)

add_executable(FloriInput WIN32
        src/app/application.cpp
        src/app/app_config.cpp
        src/app/clicker_runtime.cpp
        src/app/main.cpp
        src/app/ui_resources.cpp
        src/core/clicker_controller.cpp
        src/core/event_script.cpp
        src/core/input_program.cpp
        src/core/input_router.cpp
        src/core/key_catalog.cpp
        src/core/program_executor.cpp
        src/core/task_scheduler.cpp
        src/platform/event_script_file.cpp
        src/platform/keyboard_hook.cpp
        src/platform/paste_input.cpp
        src/platform/windows_input_backend.cpp
        src/platform/windows_platform.cpp
        src/platform/windows_trigger_executor.cpp
        resources/slint_app.rc
)
slint_target_sources(FloriInput ui/main.slint)
target_compile_features(FloriInput PRIVATE cxx_std_23)
set_target_properties(FloriInput PROPERTIES CXX_EXTENSIONS OFF SLINT_EMBED_RESOURCES embed-files OUTPUT_NAME "Flori-Input")
target_include_directories(FloriInput PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${FLORI_INPUT_RESOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_definitions(FloriInput PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        FLORI_INPUT_APP_VERSION="${PROJECT_VERSION}" FLORI_INPUT_AUTHOR_NAME="Flowersauce")
target_link_libraries(FloriInput PRIVATE
        Slint::Slint
        glaze::glaze
        user32
        advapi32
        gdi32
        comctl32
        comdlg32
        shell32
        winmm
)
if (MSVC)
    target_compile_options(FloriInput PRIVATE /utf-8 /Zc:preprocessor)
    target_link_options(FloriInput PRIVATE /ENTRY:mainCRTStartup)
endif ()

add_custom_command(TARGET FloriInput POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:FloriInput> $<TARGET_FILE_DIR:FloriInput>
        COMMAND_EXPAND_LISTS
        VERBATIM
)
