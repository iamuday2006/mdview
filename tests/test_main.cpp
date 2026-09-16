#include "test_framework.hpp"

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>

namespace mdview::test {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

Registrar::Registrar(const char* suite, const char* name, void (*function)()) {
    registry().push_back(TestCase{suite, name, function});
}

TestContext& context() {
    static TestContext ctx;
    return ctx;
}

void reportSoftFailure(const std::string& message) {
    context().failed = true;
    context().failures.push_back(message);
}

void reportHardFailure(const std::string& message) {
    context().failed = true;
    context().failures.push_back(message);
    throw AbortTest{message};
}

int runAll(const std::string& filter) {
    int total = 0;
    int failed = 0;
    std::string currentSuite;

    for (const TestCase& test : registry()) {
        if (!filter.empty() && test.suite.find(filter) == std::string::npos &&
            test.name.find(filter) == std::string::npos) {
            continue;
        }
        if (test.suite != currentSuite) {
            if (!currentSuite.empty()) std::printf("\n");
            std::printf("== %s ==\n", test.suite.c_str());
            currentSuite = test.suite;
        }

        context().failed = false;
        context().failures.clear();

        try {
            test.function();
        } catch (const AbortTest&) {
            // Already recorded by reportHardFailure().
        } catch (const std::exception& error) {
            reportSoftFailure(std::string("unexpected exception: ") + error.what());
        } catch (...) {
            reportSoftFailure("unexpected unknown exception");
        }

        ++total;
        if (context().failed) {
            ++failed;
            std::printf("FAIL %s.%s\n", test.suite.c_str(), test.name.c_str());
            for (const std::string& failure : context().failures) {
                std::printf("       %s\n", failure.c_str());
            }
        } else {
            std::printf("ok   %s.%s\n", test.suite.c_str(), test.name.c_str());
        }
    }

    std::printf("\n%d test(s) run, %d failed\n", total, failed);
    if (total == 0) {
        std::printf("no tests matched\n");
        return 1;
    }
    return failed == 0 ? 0 : 1;
}

}  // namespace mdview::test

int main(int argc, char** argv) {
    std::string filter;
    bool list = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i] != nullptr ? argv[i] : "";
        if (argument == "--list") {
            list = true;
        } else if (argument == "--filter" && i + 1 < argc) {
            filter = argv[++i];
        } else if (!argument.empty() && argument.front() != '-') {
            filter = std::string(argument);
        }
    }

    if (list) {
        for (const mdview::test::TestCase& test : mdview::test::registry()) {
            std::printf("%s.%s\n", test.suite.c_str(), test.name.c_str());
        }
        return 0;
    }

    return mdview::test::runAll(filter);
}
