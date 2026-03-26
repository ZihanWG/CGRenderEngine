#pragma once

enum class DebugViewMode
{
    Final = 0,
    SceneColor = 1,
    BrightColor = 2,
    Albedo = 3,
    Normal = 4,
    Material = 5,
    Depth = 6,
    Shadow = 7
};

struct RenderSettings
{
    bool enableShadows = true;
    bool enableBloom = true;
    bool enableIBL = true;
    bool enableReferenceComparison = true;
    float exposure = 1.05f;
    float environmentIntensity = 0.85f;
    float splitPosition = 0.5f;
    int bloomPasses = 6;
    DebugViewMode debugView = DebugViewMode::Final;
};
