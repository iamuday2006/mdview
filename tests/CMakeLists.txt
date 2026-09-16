add_executable(mdview_tests
    test_main.cpp
    utils_tests.cpp
    terminal_tests.cpp
    markdown_tests.cpp
    search_tests.cpp
    filesystem_tests.cpp
    app_tests.cpp
)

target_link_libraries(mdview_tests PRIVATE mdview_core)
mdview_enable_warnings(mdview_tests)

add_test(NAME mdview_tests COMMAND mdview_tests)
set_tests_properties(mdview_tests PROPERTIES TIMEOUT 180)

# A couple of end-to-end checks on the real executable.  They assert that the
# CLI contract the spec asks for keeps working.
add_test(NAME mdview_version
         COMMAND mdview --version)
set_tests_properties(mdview_version PROPERTIES PASS_REGULAR_EXPRESSION "mdview [0-9]+\\.[0-9]+\\.[0-9]+")

add_test(NAME mdview_help
         COMMAND mdview --help)
set_tests_properties(mdview_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")

add_test(NAME mdview_print
         COMMAND mdview --print --width 60 "${CMAKE_CURRENT_SOURCE_DIR}/../README.md")
set_tests_properties(mdview_print PROPERTIES PASS_REGULAR_EXPRESSION "mdview")
