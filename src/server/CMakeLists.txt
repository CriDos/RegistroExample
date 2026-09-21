add_library(RegistroCore STATIC
    database.cpp
    api_server.cpp
    config.cpp
    password.cpp
    log_categories.cpp
)
target_include_directories(RegistroCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RegistroCore PUBLIC
    RegistroCommon
    Qt6::Core
    Qt6::Network
    Qt6::HttpServer
    Qt6::Sql
)

set(registro_server_sources main.cpp)
if(WIN32)
    set(REGISTRO_SERVER_ICON_PATH "${PROJECT_SOURCE_DIR}/assets/registro-server.ico")
    configure_file(registro_server.rc.in ${CMAKE_CURRENT_BINARY_DIR}/registro_server.rc @ONLY)
    list(APPEND registro_server_sources ${CMAKE_CURRENT_BINARY_DIR}/registro_server.rc)
endif()
add_executable(RegistroServer ${registro_server_sources})
target_link_libraries(RegistroServer PRIVATE RegistroCore)
target_compile_definitions(RegistroServer PRIVATE REGISTRO_VERSION="${PROJECT_VERSION}")
registro_add_qt_deploy(RegistroServer)
