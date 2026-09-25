// OpenGL debug output: installs a KHR_debug / ARB_debug_output message callback when the
// driver offers one, and turns driver messages into filtered, rate-limited log lines.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace CGEngine::Platform
{
    // GL-free half of the debug output: classification, formatting, and flood control.
    // Kept separate from the callback so it can be tested without a GL context.
    class GLDebugMessageLog
    {
    public:
        explicit GLDebugMessageLog(std::size_t maxReportsPerMessage = 5);

        // Returns the line to log for this message, or nothing when it is filtered out
        // (notifications, debug-group markers) or has already been reported often enough.
        // The last report allowed for a message says that further repeats are suppressed.
        std::optional<std::string> Accept(
            std::uint32_t source,
            std::uint32_t type,
            std::uint32_t id,
            std::uint32_t severity,
            std::string_view text
        );

        // Messages that passed the filter, counting suppressed repeats.
        std::size_t GetReportedCount() const { return m_ReportedCount; }
        // Of those, messages of type ERROR or severity HIGH.
        std::size_t GetErrorCount() const { return m_ErrorCount; }

        static bool IsError(std::uint32_t type, std::uint32_t severity);
        static const char* DescribeSource(std::uint32_t source);
        static const char* DescribeType(std::uint32_t type);
        static const char* DescribeSeverity(std::uint32_t severity);

    private:
        std::size_t m_MaxReportsPerMessage;
        std::size_t m_ReportedCount = 0;
        std::size_t m_ErrorCount = 0;
        std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>, std::size_t> m_TimesSeen;
    };

    // Parses CGENGINE_GL_DEBUG-style values: "1"/"on"/"true" and "0"/"off"/"false"
    // (case-insensitive). Anything else, including an unset variable, gives no override.
    std::optional<bool> ParseGLDebugOverride(const char* value);

    // Whether to request a debug context and install the callback: on in builds without
    // NDEBUG, off otherwise, unless the CGENGINE_GL_DEBUG environment variable overrides it.
    bool IsGLDebugOutputRequested();

    // Installs the callback on the current context. Requires glad to be loaded. Returns a
    // short status line describing what was enabled, or why debug output is unavailable.
    std::string InstallGLDebugOutput();

    // Totals for the process, for a shutdown summary. Zero when output was never installed.
    std::size_t GetGLDebugReportedCount();
    std::size_t GetGLDebugErrorCount();
}
