// Minimal pass scheduler with named dependencies. Enough structure to make pass order explicit.
//
// This is intentionally not a full transient-resource render graph. Its value in this project
// is that it makes pass dependency intent visible and easy to extend before adding a heavier
// graph system later.
#pragma once

#include <functional>
#include <string>
#include <vector>

class RenderGraph
{
public:
    using ExecuteCallback = std::function<void()>;

    // Remove all registered passes before building the next frame graph.
    void Clear();
    // Adds a pass plus the names of passes that must execute before it.
    void AddPass(std::string name, std::vector<std::string> dependencies, ExecuteCallback callback);
    // Executes passes once all declared dependencies have completed.
    void Execute() const;

private:
    struct PassNode
    {
        std::string name;
        std::vector<std::string> dependencies;
        ExecuteCallback callback;
    };

    std::vector<PassNode> m_Passes;
};
