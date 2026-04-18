// Minimal resource-aware render graph. Passes declare reads, writes, targets, and dependencies.
//
// This is still intentionally lighter than a transient resource allocator, but it now models
// what each pass consumes and produces so the submission pipeline is not just "call pass A/B/C".
#pragma once

#include <functional>
#include <string>
#include <vector>

struct RenderPassDesc
{
    std::string name;
    std::vector<std::string> reads;
    std::vector<std::string> writes;
    std::string target;
    std::vector<std::string> dependencies;
    std::function<void()> callback;
};

class RenderGraph
{
public:
    using ExecuteCallback = std::function<void()>;

    // Remove all registered passes before building the next frame graph.
    void Clear();
    // Adds a pass plus the resources it reads/writes and the passes it depends on.
    void AddPass(RenderPassDesc pass);
    // Executes passes once all declared dependencies have completed.
    void Execute() const;

private:
    std::vector<RenderPassDesc> m_Passes;
};
