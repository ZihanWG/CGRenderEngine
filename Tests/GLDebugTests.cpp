#include "Tests/TestSupport.h"

#include <cstdint>
#include <optional>
#include <string>

#include "Engine/Platform/GLDebug.h"

using CGEngine::Platform::GLDebugMessageLog;
using CGEngine::Platform::ParseGLDebugOverride;

namespace
{
    // KHR_debug enum values, as the driver passes them to the callback.
    constexpr std::uint32_t kSourceApi = 0x8246;
    constexpr std::uint32_t kSourceShaderCompiler = 0x8248;
    constexpr std::uint32_t kTypeError = 0x824C;
    constexpr std::uint32_t kTypePerformance = 0x8250;
    constexpr std::uint32_t kTypePushGroup = 0x8269;
    constexpr std::uint32_t kSeverityHigh = 0x9146;
    constexpr std::uint32_t kSeverityMedium = 0x9147;
    constexpr std::uint32_t kSeverityNotification = 0x826B;

    bool Contains(const std::optional<std::string>& line, const char* fragment)
    {
        return line && line->find(fragment) != std::string::npos;
    }
}

int main()
{
    TestContext test;

    {
        GLDebugMessageLog log;
        const std::optional<std::string> line =
            log.Accept(kSourceApi, kTypeError, 1282, kSeverityHigh, "GL_INVALID_OPERATION in glDrawElements\n");
        EXPECT(test, Contains(line, "[GL high]"));
        EXPECT(test, Contains(line, "api error #1282"));
        EXPECT(test, Contains(line, "GL_INVALID_OPERATION in glDrawElements"));
        // The driver's trailing newline is stripped; the caller adds its own.
        EXPECT(test, line && line->back() != '\n');
        EXPECT(test, log.GetReportedCount() == 1);
        EXPECT(test, log.GetErrorCount() == 1);
    }

    {
        // Notifications and debug-group markers are dropped and not counted.
        GLDebugMessageLog log;
        EXPECT(test, !log.Accept(kSourceApi, kTypePerformance, 131185, kSeverityNotification, "buffer info"));
        EXPECT(test, !log.Accept(kSourceApi, kTypePushGroup, 0, kSeverityMedium, "ScenePass"));
        EXPECT(test, log.GetReportedCount() == 0);

        // A medium performance warning is reported but is not an error.
        EXPECT(test, Contains(log.Accept(kSourceApi, kTypePerformance, 7, kSeverityMedium, "stall"), "performance"));
        EXPECT(test, log.GetReportedCount() == 1);
        EXPECT(test, log.GetErrorCount() == 0);
    }

    {
        // A message repeated every frame is printed a bounded number of times, the last one
        // saying so, while the counters keep counting every occurrence.
        GLDebugMessageLog log(3);
        std::size_t printed = 0;
        std::optional<std::string> lastPrinted;
        for (int frame = 0; frame < 100; ++frame)
        {
            const std::optional<std::string> line =
                log.Accept(kSourceShaderCompiler, kTypeError, 42, kSeverityHigh, "bad sampler");
            if (line)
            {
                ++printed;
                lastPrinted = line;
            }
        }
        EXPECT(test, printed == 3);
        EXPECT(test, Contains(lastPrinted, "further repeats suppressed"));
        EXPECT(test, log.GetReportedCount() == 100);
        EXPECT(test, log.GetErrorCount() == 100);

        // A different message id is rate-limited separately.
        EXPECT(test, log.Accept(kSourceShaderCompiler, kTypeError, 43, kSeverityHigh, "other").has_value());
    }

    EXPECT(test, ParseGLDebugOverride(nullptr) == std::nullopt);
    EXPECT(test, ParseGLDebugOverride("1") == std::optional<bool>(true));
    EXPECT(test, ParseGLDebugOverride("ON") == std::optional<bool>(true));
    EXPECT(test, ParseGLDebugOverride("True") == std::optional<bool>(true));
    EXPECT(test, ParseGLDebugOverride("0") == std::optional<bool>(false));
    EXPECT(test, ParseGLDebugOverride("off") == std::optional<bool>(false));
    EXPECT(test, ParseGLDebugOverride("maybe") == std::nullopt);
    EXPECT(test, ParseGLDebugOverride("") == std::nullopt);

    return test.Finish("gl_debug");
}
