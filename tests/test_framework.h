#pragma once

#include <exception>
#include <string>
#include <vector>

namespace tinyrhi_tests {

using TestFunction = void (*)();

struct TestCase {
    const char* name;
    TestFunction function;
};

class TestFailure final : public std::exception {
public:
    TestFailure(const char* expression, const char* file, int line);

    const char* what() const noexcept override;

private:
    std::string m_message;
};

class TestRegistrar {
public:
    TestRegistrar(const char* name, TestFunction function);
};

std::vector<TestCase>& registry();
void require(bool condition, const char* expression, const char* file, int line);

} // namespace tinyrhi_tests

#define TINYRHI_TEST_CONCAT_IMPL(lhs, rhs) lhs##rhs
#define TINYRHI_TEST_CONCAT(lhs, rhs) TINYRHI_TEST_CONCAT_IMPL(lhs, rhs)

#define TINYRHI_TEST_CASE(name)                                                                                       \
    static void TINYRHI_TEST_CONCAT(tinyrhi_test_, __LINE__)();                                                       \
    static ::tinyrhi_tests::TestRegistrar TINYRHI_TEST_CONCAT(tinyrhi_registrar_, __LINE__)(                          \
        name,                                                                                                         \
        &TINYRHI_TEST_CONCAT(tinyrhi_test_, __LINE__));                                                               \
    static void TINYRHI_TEST_CONCAT(tinyrhi_test_, __LINE__)()

#define TINYRHI_REQUIRE(expression) ::tinyrhi_tests::require((expression), #expression, __FILE__, __LINE__)
#define TINYRHI_CHECK(expression) TINYRHI_REQUIRE(expression)
