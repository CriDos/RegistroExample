add_library(RegistroCommon STATIC
    client.cpp
    auth.cpp
    audit.cpp
    log.cpp
    config_store.cpp
)
target_include_directories(RegistroCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(RegistroCommon PUBLIC cxx_std_17)
target_compile_definitions(RegistroCommon PUBLIC QT_MESSAGELOGCONTEXT)
target_link_libraries(RegistroCommon PUBLIC Qt6::Core)