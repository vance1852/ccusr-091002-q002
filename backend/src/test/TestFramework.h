#pragma once

/**
 * 轻量级测试框架 - 无外部依赖
 * 提供断言宏和测试运行器
 */

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <sstream>

namespace test {

    struct TestCase {
        std::string name;
        std::function<void()> func;
    };

    class TestRunner {
    public:
        static TestRunner& instance() {
            static TestRunner inst;
            return inst;
        }

        void add(const std::string& name, std::function<void()> func) {
            cases_.push_back({name, func});
        }

        int run() {
            int passed = 0, failed = 0;
            std::cout << "\n" << std::string(60, '=') << std::endl;
            std::cout << "  Running " << cases_.size() << " test cases..." << std::endl;
            std::cout << std::string(60, '=') << "\n" << std::endl;

            for (auto& tc : cases_) {
                try {
                    tc.func();
                    std::cout << "  [PASS] " << tc.name << std::endl;
                    passed++;
                } catch (const std::exception& e) {
                    std::cout << "  [FAIL] " << tc.name << std::endl;
                    std::cout << "         " << e.what() << std::endl;
                    failed++;
                }
            }

            std::cout << "\n" << std::string(60, '-') << std::endl;
            std::cout << "  Results: " << passed << " passed, " << failed << " failed, "
                      << cases_.size() << " total" << std::endl;
            std::cout << std::string(60, '=') << "\n" << std::endl;

            return failed;
        }

    private:
        std::vector<TestCase> cases_;
    };

    class AssertionError : public std::runtime_error {
    public:
        explicit AssertionError(const std::string& msg) : std::runtime_error(msg) {}
    };

    inline void fail(const std::string& file, int line, const std::string& msg) {
        std::ostringstream oss;
        oss << file << ":" << line << " - " << msg;
        throw AssertionError(oss.str());
    }
}

#define TEST(name) \
    static void test_##name(); \
    namespace { struct Register_##name { \
        Register_##name() { test::TestRunner::instance().add(#name, test_##name); } \
    } reg_##name; } \
    static void test_##name()

#define ASSERT_TRUE(expr) \
    if (!(expr)) test::fail(__FILE__, __LINE__, "ASSERT_TRUE failed: " #expr)

#define ASSERT_FALSE(expr) \
    if ((expr)) test::fail(__FILE__, __LINE__, "ASSERT_FALSE failed: " #expr)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::ostringstream _oss; _oss << "ASSERT_EQ failed: " << (a) << " != " << (b); \
        test::fail(__FILE__, __LINE__, _oss.str()); \
    }

#define ASSERT_NE(a, b) \
    if ((a) == (b)) { \
        std::ostringstream _oss; _oss << "ASSERT_NE failed: " << (a) << " == " << (b); \
        test::fail(__FILE__, __LINE__, _oss.str()); \
    }

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { \
        std::ostringstream _oss; _oss << "ASSERT_GT failed: " << (a) << " <= " << (b); \
        test::fail(__FILE__, __LINE__, _oss.str()); \
    }

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) { \
        std::ostringstream _oss; _oss << "ASSERT_GE failed: " << (a) << " < " << (b); \
        test::fail(__FILE__, __LINE__, _oss.str()); \
    }

#define ASSERT_FLOAT_EQ(a, b) \
    if (std::fabs((a) - (b)) > 0.01f) { \
        std::ostringstream _oss; _oss << "ASSERT_FLOAT_EQ failed: " << (a) << " != " << (b); \
        test::fail(__FILE__, __LINE__, _oss.str()); \
    }

#define ASSERT_STR_EQ(a, b) \
    if (std::string(a) != std::string(b)) { \
        std::ostringstream _oss; _oss << "ASSERT_STR_EQ failed: \"" << (a) << "\" != \"" << (b) << "\""; \
        test::fail(__FILE__, __LINE__, _oss.str()); \
    }

#define ASSERT_THROWS(expr, ExType) \
    { bool _caught = false; \
      try { expr; } catch (const ExType&) { _caught = true; } \
      if (!_caught) test::fail(__FILE__, __LINE__, "ASSERT_THROWS failed: no " #ExType " thrown"); }
