add_executable(test_test_evaluator
    test_test_evaluator.cpp
    ${CMAKE_SOURCE_DIR}/src/test/TestEvaluator.cpp
)

target_link_libraries(test_test_evaluator
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(test_test_evaluator
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/test
    ${CMAKE_SOURCE_DIR}/src/backend
    ${CMAKE_SOURCE_DIR}/src/algorithm
)

add_test(
    NAME test_test_evaluator
    COMMAND test_test_evaluator
)

add_executable(test_scenario_loader
    test_scenario_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/scenario/ScenarioLoader.cpp
)

target_link_libraries(test_scenario_loader
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(test_scenario_loader
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/test
    ${CMAKE_SOURCE_DIR}/src/scenario
    ${CMAKE_SOURCE_DIR}/src/backend
    ${CMAKE_SOURCE_DIR}/src/algorithm
)

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME test_scenario_loader
    COMMAND test_scenario_loader
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
    NAME headless_empty_scenario
    COMMAND
        bash
        ${CMAKE_CURRENT_SOURCE_DIR}/run_headless_smoke.sh
        $<TARGET_FILE:ADASim>
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/adasim_smoke.ini
        ${CMAKE_SOURCE_DIR}/scenarios/empty.json
)

set_tests_properties(headless_empty_scenario PROPERTIES
    TIMEOUT 120
)

add_executable(test_socket_server
    test_socket_server.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/Socket.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/Socket.h
)
target_include_directories(test_socket_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_socket_server PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Test
)
add_test(NAME test_socket_server COMMAND test_socket_server)
set_tests_properties(test_socket_server PROPERTIES TIMEOUT 30)  

add_executable(test_planner_link
    test_planner_link.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/PlannerLink.cpp
    ${CMAKE_SOURCE_DIR}/src/communication/PlannerLink.h
)
target_include_directories(test_planner_link PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_planner_link PRIVATE
    Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME test_planner_link COMMAND test_planner_link)
set_tests_properties(test_planner_link PROPERTIES TIMEOUT 30)