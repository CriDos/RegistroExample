cmake_minimum_required(VERSION 3.25)
project(RegistroExample VERSION 1.0.1 LANGUAGES CXX)

option(REGISTRO_ENABLE_ANALYZE "Enable MSVC static analysis (/analyze)" OFF)
if(REGISTRO_ENABLE_ANALYZE AND MSVC)
    add_compile_options(/analyze)
endif()

option(REGISTRO_ENABLE_ASAN "Enable MSVC AddressSanitizer (/fsanitize=address)" OFF)
if(REGISTRO_ENABLE_ASAN AND MSVC)
    add_compile_options(/fsanitize=address)
    add_link_options(/fsanitize=address)
endif()

if(MSVC)
    add_compile_options(/W4 /external:anglebrackets /external:W0 /utf-8)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network HttpServer Sql Test Quick Qml QuickControls2)

qt_policy(SET QTP0001 NEW)
qt_standard_project_setup()

# Under Ninja the separate post-build steps run in parallel and two windeployqt
# instances race on the same Qt DLLs ("Cannot remove existing file"). A
# one-slot job pool serializes the link+deploy steps of deployed targets.
if(CMAKE_GENERATOR MATCHES "Ninja")
    set(CMAKE_JOB_POOLS "${CMAKE_JOB_POOLS}deploy_pool=1")
endif()

# Deploy the Qt runtime next to every executable at build time (windeployqt
# underneath): all binaries then run straight from the build tree with zero
# environment - no Qt in PATH, F5 and double-click just work.
function(registro_add_qt_deploy target)
    if(NOT WIN32)
        return()
    endif()
    if(CMAKE_GENERATOR MATCHES "Ninja")
        set_property(TARGET ${target} PROPERTY JOB_POOL_LINK deploy_pool)
    endif()
    qt_generate_deploy_app_script(
        TARGET ${target}
        OUTPUT_SCRIPT registro_deploy_${target}_script
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME
        NO_UNSUPPORTED_PLATFORM_ERROR
        ${ARGN}
    )
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                "-DQT_DEPLOY_PREFIX=$<TARGET_FILE_DIR:${target}>"
                "-DQT_DEPLOY_BIN_DIR=."
                "-DQT_DEPLOY_LIB_DIR=."
                -P "${registro_deploy_${target}_script}"
        VERBATIM
        COMMENT "Deploying Qt runtime for ${target}"
    )
endfunction()

enable_testing()

add_subdirectory(src/common common)
add_subdirectory(src/server server)
add_subdirectory(src/client client)
add_subdirectory(tests)
