#include "TestFramework.h"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int run = 0;
    int failedTests = 0;
    for (const auto& test : testfw::registry()) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) continue;
        ++run;
        const int before = testfw::failureCount();
        try {
            test.body();
        } catch (const std::exception& e) {
            testfw::reportFailure(__FILE__, __LINE__, "Unexpected exception: " + std::string(e.what()));
        } catch (...) {
            testfw::reportFailure(__FILE__, __LINE__, "Unexpected non-standard exception");
        }
        const bool ok = testfw::failureCount() == before;
        if (!ok) ++failedTests;
        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << test.name << "\n";
    }
    std::cout << "\n" << run - failedTests << "/" << run << " tests passed, " << testfw::failureCount()
              << " failed check(s).\n";
    if (run == 0) {
        std::cout << "No tests matched filter '" << filter << "'.\n";
        return 1;
    }
    return failedTests == 0 ? 0 : 1;
}
