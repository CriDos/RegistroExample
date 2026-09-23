add_executable(test_database test_database.cpp)
target_link_libraries(test_database PRIVATE RegistroCore Qt6::Test)
add_test(NAME database COMMAND test_database
         -o "${CMAKE_CURRENT_BINARY_DIR}/database-result.txt,txt")
set_tests_properties(database PROPERTIES LABELS "unit")

add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE RegistroCore Qt6::Test)
add_test(NAME config COMMAND test_config -o "${CMAKE_CURRENT_BINARY_DIR}/config-result.txt,txt")
set_tests_properties(config PROPERTIES LABELS "unit")

add_executable(test_api test_api.cpp)
target_link_libraries(test_api PRIVATE RegistroCore Qt6::Test Qt6::Network)
add_test(NAME api COMMAND test_api -o "${CMAKE_CURRENT_BINARY_DIR}/api-result.txt,txt")
set_tests_properties(api PROPERTIES LABELS "integration")

add_executable(test_log test_log.cpp)
target_link_libraries(test_log PRIVATE RegistroCommon Qt6::Test)
add_test(NAME log COMMAND test_log -o "${CMAKE_CURRENT_BINARY_DIR}/log-result.txt,txt")
set_tests_properties(log PROPERTIES LABELS "unit")

add_executable(test_config_store test_config_store.cpp)
target_link_libraries(test_config_store PRIVATE RegistroCommon Qt6::Test)
add_test(NAME config_store COMMAND test_config_store
         -o "${CMAKE_CURRENT_BINARY_DIR}/config-store-result.txt,txt")
set_tests_properties(config_store PROPERTIES LABELS "unit")

add_executable(test_api_client test_api_client.cpp)
target_link_libraries(test_api_client PRIVATE RegistroClientCore RegistroCore Qt6::Test)
add_test(NAME api_client COMMAND test_api_client
         -o "${CMAKE_CURRENT_BINARY_DIR}/api-client-result.txt,txt")
set_tests_properties(api_client PROPERTIES LABELS "integration")

add_executable(test_client_model test_client_model.cpp)
target_link_libraries(test_client_model PRIVATE RegistroClientCore Qt6::Test)
add_test(NAME client_model COMMAND test_client_model
         -o "${CMAKE_CURRENT_BINARY_DIR}/client-model-result.txt,txt")
set_tests_properties(client_model PROPERTIES LABELS "unit")

add_executable(test_client_config test_client_config.cpp)
target_link_libraries(test_client_config PRIVATE RegistroClientCore Qt6::Test)
add_test(NAME client_config COMMAND test_client_config
         -o "${CMAKE_CURRENT_BINARY_DIR}/client-config-result.txt,txt")
set_tests_properties(client_config PROPERTIES LABELS "unit")

foreach(registro_test IN ITEMS
         test_database test_config test_api test_log test_config_store
         test_api_client test_client_model test_client_config)
    registro_add_qt_deploy(${registro_test})
endforeach()

set_property(TEST database config api log config_store api_client client_model client_config
             APPEND PROPERTY ENVIRONMENT "REGISTRO_PBKDF2_ITERATIONS=1000")

add_test(NAME qmllint
         COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
                 --target RegistroClient_qmllint)
set_tests_properties(qmllint PROPERTIES LABELS "lint")
