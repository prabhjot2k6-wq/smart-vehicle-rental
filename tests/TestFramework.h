#pragma once
// A tiny dependency-free test framework.
//
//   TEST_CASE("name") { CHECK(cond); CHECK_EQ(actual, expected); CHECK_THROWS_AS(expr, Type); }
//
// Each failure prints file:line, the expression and both values. Run the
// binary with a substring argument to run only matching tests:
//   ./rental_tests pricing

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

inline void reportFailure(const char* file, int line, const std::string& message) {
    ++failureCount();
    std::cout << "    FAILED " << file << ":" << line << "\n      " << message << "\n";
}

template <typename A, typename B>
void checkEqual(const A& actual, const B& expected, const char* actualExpr, const char* expectedExpr, const char* file,
                int line) {
    if (!(actual == expected)) {
        std::ostringstream os;
        os << "CHECK_EQ(" << actualExpr << ", " << expectedExpr << ")\n        actual:   " << actual
           << "\n        expected: " << expected;
        reportFailure(file, line, os.str());
    }
}

} // namespace testfw

#define TF_CONCAT2(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT2(a, b)

#define TEST_CASE(name)                                                                                       \
    static void TF_CONCAT(tf_test_, __LINE__)();                                                              \
    static const testfw::Registrar TF_CONCAT(tf_reg_, __LINE__)(name, &TF_CONCAT(tf_test_, __LINE__));         \
    static void TF_CONCAT(tf_test_, __LINE__)()

#define CHECK(cond)                                                                                           \
    do {                                                                                                      \
        if (!(cond)) testfw::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ")");                           \
    } while (0)

#define CHECK_EQ(actual, expected) testfw::checkEqual((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define CHECK_THROWS_AS(expr, ExceptionType)                                                                  \
    do {                                                                                                      \
        bool tf_caught = false;                                                                               \
        try {                                                                                                 \
            (void)(expr);                                                                                     \
        } catch (const ExceptionType&) {                                                                      \
            tf_caught = true;                                                                                 \
        } catch (const std::exception& tf_e) {                                                                \
            testfw::reportFailure(__FILE__, __LINE__,                                                         \
                                  "CHECK_THROWS_AS(" #expr ", " #ExceptionType ") threw a different exception: " + \
                                      std::string(tf_e.what()));                                              \
            tf_caught = true;                                                                                 \
        }                                                                                                     \
        if (!tf_caught)                                                                                       \
            testfw::reportFailure(__FILE__, __LINE__, "CHECK_THROWS_AS(" #expr ", " #ExceptionType ") did not throw"); \
    } while (0)
