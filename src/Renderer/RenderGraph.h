#pragma once

#include <functional>
#include <string>
#include <vector>

class RenderGraph
{
public:
    using ExecuteCallback = std::function<void()>;

    void Clear();
    void AddPass(std::string name, std::vector<std::string> dependencies, ExecuteCallback callback);
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
