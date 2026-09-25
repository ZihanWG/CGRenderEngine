// OpenGL debug output. The project's glad loader is generated for core 3.3 with no
// extensions, so the KHR_debug / ARB_debug_output entry points and enums are resolved here
// instead. Both extensions share the same enum values and function signatures.
#include "Engine/Platform/GLDebug.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace
{
    // Enum values from the Khronos registry (identical for KHR_debug and ARB_debug_output).
    constexpr std::uint32_t kDebugOutput = 0x92E0;
    constexpr std::uint32_t kDebugOutputSynchronous = 0x8242;
    constexpr std::uint32_t kContextFlagDebugBit = 0x00000002;

    constexpr std::uint32_t kSourceApi = 0x8246;
    constexpr std::uint32_t kSourceWindowSystem = 0x8247;
    constexpr std::uint32_t kSourceShaderCompiler = 0x8248;
    constexpr std::uint32_t kSourceThirdParty = 0x8249;
    constexpr std::uint32_t kSourceApplication = 0x824A;
    constexpr std::uint32_t kSourceOther = 0x824B;

    constexpr std::uint32_t kTypeError = 0x824C;
    constexpr std::uint32_t kTypeDeprecatedBehavior = 0x824D;
    constexpr std::uint32_t kTypeUndefinedBehavior = 0x824E;
    constexpr std::uint32_t kTypePortability = 0x824F;
    constexpr std::uint32_t kTypePerformance = 0x8250;
    constexpr std::uint32_t kTypeOther = 0x8251;
    constexpr std::uint32_t kTypeMarker = 0x8268;
    constexpr std::uint32_t kTypePushGroup = 0x8269;
    constexpr std::uint32_t kTypePopGroup = 0x826A;

    constexpr std::uint32_t kSeverityHigh = 0x9146;
    constexpr std::uint32_t kSeverityMedium = 0x9147;
    constexpr std::uint32_t kSeverityLow = 0x9148;
    constexpr std::uint32_t kSeverityNotification = 0x826B;

    using DebugProc = void(APIENTRY*)(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam
    );
    using DebugMessageCallbackProc = void(APIENTRY*)(DebugProc callback, const void* userParam);

    // The callback is synchronous, so it normally runs on the thread that made the failing
    // GL call. The lock covers drivers that deliver messages from their own threads anyway.
    std::mutex g_LogMutex;
    CGEngine::Platform::GLDebugMessageLog g_Log;

    void APIENTRY OnDebugMessage(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void*
    )
    {
        const std::string_view text = length >= 0 && message
            ? std::string_view(message, static_cast<std::size_t>(length))
            : std::string_view(message ? message : "");

        std::optional<std::string> line;
        {
            const std::lock_guard<std::mutex> lock(g_LogMutex);
            line = g_Log.Accept(source, type, id, severity, text);
        }

        if (line)
        {
            // A breakpoint here stops inside the offending GL call's stack.
            std::cerr << *line << '\n';
        }
    }
}

namespace CGEngine::Platform
{
    GLDebugMessageLog::GLDebugMessageLog(std::size_t maxReportsPerMessage)
        : m_MaxReportsPerMessage(std::max<std::size_t>(maxReportsPerMessage, 1))
    {
    }

    std::optional<std::string> GLDebugMessageLog::Accept(
        std::uint32_t source,
        std::uint32_t type,
        std::uint32_t id,
        std::uint32_t severity,
        std::string_view text
    )
    {
        // Notifications are informational (NVIDIA reports every buffer placement at this
        // level), and group markers are the application's own annotations.
        if (severity == kSeverityNotification || type == kTypeMarker || type == kTypePushGroup ||
            type == kTypePopGroup)
        {
            return std::nullopt;
        }

        ++m_ReportedCount;
        if (IsError(type, severity))
        {
            ++m_ErrorCount;
        }

        // A per-frame error would otherwise print once per frame forever.
        const std::size_t timesSeen = ++m_TimesSeen[{source, type, id}];
        if (timesSeen > m_MaxReportsPerMessage)
        {
            return std::nullopt;
        }

        std::string line = "[GL ";
        line += DescribeSeverity(severity);
        line += "] ";
        line += DescribeSource(source);
        line += ' ';
        line += DescribeType(type);
        line += " #";
        line += std::to_string(id);
        line += ": ";
        line += text;
        // Drivers usually end messages with a newline; the caller adds its own.
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        {
            line.pop_back();
        }
        if (timesSeen == m_MaxReportsPerMessage)
        {
            line += " (further repeats suppressed)";
        }

        return line;
    }

    bool GLDebugMessageLog::IsError(std::uint32_t type, std::uint32_t severity)
    {
        return type == kTypeError || severity == kSeverityHigh;
    }

    const char* GLDebugMessageLog::DescribeSource(std::uint32_t source)
    {
        switch (source)
        {
        case kSourceApi:
            return "api";
        case kSourceWindowSystem:
            return "window-system";
        case kSourceShaderCompiler:
            return "shader-compiler";
        case kSourceThirdParty:
            return "third-party";
        case kSourceApplication:
            return "application";
        case kSourceOther:
            return "other";
        default:
            return "unknown-source";
        }
    }

    const char* GLDebugMessageLog::DescribeType(std::uint32_t type)
    {
        switch (type)
        {
        case kTypeError:
            return "error";
        case kTypeDeprecatedBehavior:
            return "deprecated";
        case kTypeUndefinedBehavior:
            return "undefined-behavior";
        case kTypePortability:
            return "portability";
        case kTypePerformance:
            return "performance";
        case kTypeOther:
            return "other";
        case kTypeMarker:
            return "marker";
        case kTypePushGroup:
            return "push-group";
        case kTypePopGroup:
            return "pop-group";
        default:
            return "unknown-type";
        }
    }

    const char* GLDebugMessageLog::DescribeSeverity(std::uint32_t severity)
    {
        switch (severity)
        {
        case kSeverityHigh:
            return "high";
        case kSeverityMedium:
            return "medium";
        case kSeverityLow:
            return "low";
        case kSeverityNotification:
            return "notification";
        default:
            return "unknown";
        }
    }

    std::optional<bool> ParseGLDebugOverride(const char* value)
    {
        if (!value)
        {
            return std::nullopt;
        }

        std::string lowered(value);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (lowered == "1" || lowered == "on" || lowered == "true")
        {
            return true;
        }
        if (lowered == "0" || lowered == "off" || lowered == "false")
        {
            return false;
        }

        return std::nullopt;
    }

    bool IsGLDebugOutputRequested()
    {
        if (const std::optional<bool> overrideValue = ParseGLDebugOverride(std::getenv("CGENGINE_GL_DEBUG")))
        {
            return *overrideValue;
        }

#ifdef NDEBUG
        return false;
#else
        return true;
#endif
    }

    std::string InstallGLDebugOutput()
    {
        const char* extension = nullptr;
        DebugMessageCallbackProc debugMessageCallback = nullptr;
        if (glfwExtensionSupported("GL_KHR_debug"))
        {
            extension = "GL_KHR_debug";
            debugMessageCallback =
                reinterpret_cast<DebugMessageCallbackProc>(glfwGetProcAddress("glDebugMessageCallback"));
        }
        else if (glfwExtensionSupported("GL_ARB_debug_output"))
        {
            extension = "GL_ARB_debug_output";
            debugMessageCallback =
                reinterpret_cast<DebugMessageCallbackProc>(glfwGetProcAddress("glDebugMessageCallbackARB"));
        }

        if (!debugMessageCallback)
        {
            return "GL debug output: unavailable (driver offers neither GL_KHR_debug nor GL_ARB_debug_output)";
        }

        GLint contextFlags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &contextFlags);
        const bool debugContext = (static_cast<std::uint32_t>(contextFlags) & kContextFlagDebugBit) != 0;

        // KHR_debug has a global enable; ARB_debug_output is on for debug contexts and
        // does not define GL_DEBUG_OUTPUT, so only touch it where it exists.
        if (std::string_view(extension) == "GL_KHR_debug")
        {
            glEnable(kDebugOutput);
        }
        // Synchronous delivery: the callback runs inside the GL call that raised the
        // message, so the call stack points at the culprit.
        glEnable(kDebugOutputSynchronous);
        debugMessageCallback(OnDebugMessage, nullptr);

        std::string status = "GL debug output: enabled via ";
        status += extension;
        status += debugContext ? " (debug context, synchronous)" : " (non-debug context: drivers may report little)";
        return status;
    }

    std::size_t GetGLDebugReportedCount()
    {
        const std::lock_guard<std::mutex> lock(g_LogMutex);
        return g_Log.GetReportedCount();
    }

    std::size_t GetGLDebugErrorCount()
    {
        const std::lock_guard<std::mutex> lock(g_LogMutex);
        return g_Log.GetErrorCount();
    }
}
