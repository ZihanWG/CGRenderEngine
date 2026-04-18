#pragma once

#include <filesystem>
#include <string>

namespace CGEngine::Core
{
std::filesystem::path GetProjectRootPath();
std::filesystem::path ResolveProjectPath(const std::filesystem::path& path);
std::string ResolveProjectPathString(const std::string& path);
}
