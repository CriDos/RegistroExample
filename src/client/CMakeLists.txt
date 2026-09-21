qt_add_library(RegistroClientCore STATIC
    api_client.cpp
    client_limits.cpp
    client_model.cpp
    log_categories.cpp
    config.cpp
)
target_include_directories(RegistroClientCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RegistroClientCore PUBLIC
    RegistroCommon
    Qt6::Core
    Qt6::Network
)

set(registro_client_sources main.cpp)
if(WIN32)
    set(REGISTRO_CLIENT_ICON_PATH "${PROJECT_SOURCE_DIR}/assets/registro-client.ico")
    configure_file(registro_client.rc.in ${CMAKE_CURRENT_BINARY_DIR}/registro_client.rc @ONLY)
    list(APPEND registro_client_sources ${CMAKE_CURRENT_BINARY_DIR}/registro_client.rc)
endif()
add_executable(RegistroClient ${registro_client_sources})
target_link_libraries(RegistroClient PRIVATE
    RegistroClientCore
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
)
target_compile_definitions(RegistroClient PRIVATE REGISTRO_VERSION="${PROJECT_VERSION}")

set(registro_qml_files
    qml/Main.qml
    qml/Theme.qml
    qml/pages/LoginPage.qml
    qml/pages/ClientsPage.qml
    qml/pages/ClientCardPage.qml
qml/components/FormField.qml
    qml/components/AboutDialog.qml
    qml/components/StatusBadge.qml
    qml/components/ErrorBanner.qml
    qml/components/MaskedTextField.qml
    qml/modules/format.js
)
foreach(f IN LISTS registro_qml_files)
    get_filename_component(qml_alias ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(registro_client_icon "${PROJECT_SOURCE_DIR}/assets/registro-client.svg")
set_source_files_properties(${registro_client_icon} PROPERTIES QT_RESOURCE_ALIAS "registro-client.svg")

qt_add_qml_module(RegistroClient
    URI Registro.Client
    VERSION 1.0
    QML_FILES
        ${registro_qml_files}
    RESOURCES
        ${registro_client_icon}
    SOURCES
        qml_singletons.h
)
registro_add_qt_deploy(RegistroClient
    DEPLOY_TOOL_OPTIONS --qmldir=${CMAKE_CURRENT_SOURCE_DIR}/qml
)

