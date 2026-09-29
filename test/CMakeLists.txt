file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

add_executable(pyqint_gui_test ${TEST_SOURCES} ../resources.qrc)

target_link_libraries(pyqint_gui_test
    PRIVATE
        pyqint_gui_core
        Qt6::Test
)

target_compile_definitions(pyqint_gui_test
    PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

add_test(NAME pyqint_gui_tests COMMAND pyqint_gui_test)
set_tests_properties(pyqint_gui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
