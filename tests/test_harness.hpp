#pragma once
#ifndef PRISM_TEST_HARNESS_HPP
#define PRISM_TEST_HARNESS_HPP

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace prism::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner r;
        return r;
    }

    void register_test(const std::string& name, std::function<void()> func) {
        tests_.push_back({name, std::move(func)});
    }

    int run_all() {
        std::cout << "========================================================\n";
        std::cout << "  PRISM-V PRODUCTION SUITE: TEST-DRIVEN FALSIFICATION   \n";
        std::cout << "  Verifying 7 Non-Negotiable Engineering Invariants     \n";
        std::cout << "========================================================\n\n";

        size_t passed = 0;
        size_t failed = 0;

        for (const auto& t : tests_) {
            std::cout << "[ RUN      ] " << t.name << "\n";
            auto start = std::chrono::high_resolution_clock::now();
            try {
                t.func();
                auto end = std::chrono::high_resolution_clock::now();
                double ms = std::chrono::duration<double, std::milli>(end - start).count();
                std::cout << "[       OK ] " << t.name << " (" << ms << " ms)\n";
                passed++;
            } catch (const std::exception& ex) {
                std::cout << "[  FAILED  ] " << t.name << "\n";
                std::cout << "             Error: " << ex.what() << "\n";
                failed++;
            } catch (...) {
                std::cout << "[  FAILED  ] " << t.name << " (Unknown exception)\n";
                failed++;
            }
        }

        std::cout << "\n--------------------------------------------------------\n";
        std::cout << "Total Tests: " << tests_.size() << " | Passed: " << passed
                  << " | Failed: " << failed << "\n";
        std::cout << "--------------------------------------------------------\n";

        return (failed == 0) ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

#define PRISM_TEST(name) \
    static void name(); \
    namespace { \
        struct name##_Reg { \
            name##_Reg() { \
                ::prism::test::TestRunner::instance().register_test(#name, name); \
            } \
        } name##_instance; \
    } \
    static void name()

#define PRISM_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #cond + \
                                     " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define PRISM_ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #a + " == " + #b + \
                                     " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

} // namespace prism::test

#endif // PRISM_TEST_HARNESS_HPP
