file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

add_executable(praxis_test ${TEST_SOURCES} ../resources.qrc)

target_link_libraries(praxis_test
    PRIVATE
        praxis_core
        Qt6::Test
)

target_compile_definitions(praxis_test
    PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

add_test(NAME praxis_tests COMMAND praxis_test)
set_tests_properties(praxis_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
