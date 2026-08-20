#pragma once

#include <cmath>
#include <exception>
#include <iostream>
#include <string>

class TestContext
{
public:
    void Expect(bool condition, const char* expression, const char* file, int line)
    {
        if (condition)
        {
            return;
        }

        ++m_Failures;
        std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    }

    template <typename Function>
    void ExpectThrows(Function&& function, const char* expression, const char* file, int line)
    {
        try
        {
            function();
        }
        catch (const std::exception&)
        {
            return;
        }

        ++m_Failures;
        std::cerr << file << ':' << line << ": expected exception: " << expression << '\n';
    }

    int Finish(const std::string& suiteName) const
    {
        if (m_Failures == 0)
        {
            std::cout << suiteName << ": all checks passed\n";
            return 0;
        }

        std::cerr << suiteName << ": " << m_Failures << " check(s) failed\n";
        return 1;
    }

private:
    int m_Failures = 0;
};

#define EXPECT(context, expression) (context).Expect((expression), #expression, __FILE__, __LINE__)
#define EXPECT_THROWS(context, expression) \
    (context).ExpectThrows([&]() { expression; }, #expression, __FILE__, __LINE__)

inline bool NearlyEqual(float left, float right, float epsilon = 1e-5f)
{
    return std::abs(left - right) <= epsilon;
}

