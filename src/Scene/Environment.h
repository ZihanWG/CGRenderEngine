#pragma once

#include <memory>
#include <string>

struct EnvironmentImage;

struct SceneEnvironment
{
    std::string hdrPath;
    float rotationDegrees = 0.0f;
    std::shared_ptr<EnvironmentImage> hdrImage;
};
