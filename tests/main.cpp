#include "test_framework.h"

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

namespace tinyrhi_tests {

TestFailure::TestFailure(const char* expression, const char* file, int line)
{
    m_message = std::string(file) + ":" + std::to_string(line) + ": check failed: " + expression;
}

const char* TestFailure::what() const noexcept
{
    return m_message.c_str();
}

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

TestRegistrar::TestRegistrar(const char* name, TestFunction function)
{
    registry().push_back(TestCase{.name = name, .function = function});
}

void require(bool condition, const char* expression, const char* file, int line)
{
    if (!condition) {
        throw TestFailure(expression, file, line);
    }
}

} // namespace tinyrhi_tests

int main()
{
    int failed = 0;
    const auto& tests = tinyrhi_tests::registry();

    for (const auto& test : tests) {
        try {
            test.function();
            std::printf("[PASS] %s\n", test.name);
        } catch (const std::exception& error) {
            ++failed;
            std::printf("[FAIL] %s\n       %s\n", test.name, error.what());
        } catch (...) {
            ++failed;
            std::printf("[FAIL] %s\n       unknown exception\n", test.name);
        }
    }

    std::printf("%zu tests, %d failed\n", tests.size(), failed);
    return failed == 0 ? 0 : 1;
}
