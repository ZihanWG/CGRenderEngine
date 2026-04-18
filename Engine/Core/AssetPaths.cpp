#include "Engine/Core/AssetPaths.h"

namespace CGEngine::Core
{
std::filesystem::path GetProjectRootPath()
{
    return std::filesystem::path(CGENGINE_PROJECT_ROOT);
}

std::filesystem::path ResolveProjectPath(const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return path;
    }

    return (GetProjectRootPath() / path).lexically_normal();
}

std::string ResolveProjectPathString(const std::string& path)
{
    return ResolveProjectPath(std::filesystem::path(path)).string();
}
}
