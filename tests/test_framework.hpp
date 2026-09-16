#pragma once

#include <sstream>
#include <string>
#include <vector>

// A deliberately tiny test harness.
//
// The project has no third-party dependencies -- that is one of the reasons it
// builds unchanged under MSVC, GCC and Clang -- so the tests do not depend on
// one either.  Register a test with MDVIEW_TEST(suite, name) and use CHECK /
// CHECK_EQ for expectations, REQUIRE to stop a test early.

namespace mdview::test {

struct TestCase {
    std::string suite;
    std::string name;
    void (*function)();
};

std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* suite, const char* name, void (*function)());
};

/// Thrown by REQUIRE() to abandon the current test.
struct AbortTest {
    std::string message;
};

struct TestContext {
    bool failed = false;
    std::vector<std::string> failures;
};

TestContext& context();

void reportSoftFailure(const std::string& message);
[[noreturn]] void reportHardFailure(const std::string& message);

template <typename T>
std::string describe(const T& value) {
    std::ostringstream stream;
    if constexpr (requires { stream << value; }) {
        stream << value;
    } else {
        stream << "<value>";
    }
    return stream.str();
}

/// Runs every test whose suite or name contains `filter` (empty matches all).
int runAll(const std::string& filter);

}  // namespace mdview::test

#define MDVIEW_TEST(suite, name)                                                              \
    static void mdview_test_##suite##_##name();                                               \
    static const ::mdview::test::Registrar mdview_registrar_##suite##_##name(                 \
        #suite, #name, &mdview_test_##suite##_##name);                                        \
    static void mdview_test_##suite##_##name()

#define CHECK(condition)                                                                      \
    do {                                                                                      \
        if (!(condition)) {                                                                   \
            ::mdview::test::reportSoftFailure(std::string("CHECK failed: ") + #condition);    \
        }                                                                                     \
    } while (false)

#define CHECK_EQ(actual, expected)                                                            \
    do {                                                                                      \
        const auto& mdview_actual = (actual);                                                 \
        const auto& mdview_expected = (expected);                                             \
        if (!(mdview_actual == mdview_expected)) {                                            \
            ::mdview::test::reportSoftFailure(                                                \
                std::string("CHECK_EQ failed: ") + #actual + " == " + #expected +             \
                "\n      actual:   " + ::mdview::test::describe(mdview_actual) +              \
                "\n      expected: " + ::mdview::test::describe(mdview_expected));            \
        }                                                                                     \
    } while (false)

#define CHECK_CONTAINS(haystack, needle)                                                      \
    do {                                                                                      \
        const std::string mdview_haystack = (haystack);                                       \
        const std::string mdview_needle = (needle);                                           \
        if (mdview_haystack.find(mdview_needle) == std::string::npos) {                       \
            ::mdview::test::reportSoftFailure(std::string("CHECK_CONTAINS failed: ") +        \
                                              #needle + " not found in " + #haystack +        \
                                              "\n      haystack: " + mdview_haystack);        \
        }                                                                                     \
    } while (false)

#define REQUIRE(condition)                                                                    \
    do {                                                                                      \
        if (!(condition)) {                                                                   \
            ::mdview::test::reportHardFailure(std::string("REQUIRE failed: ") + #condition);  \
        }                                                                                     \
    } while (false)
