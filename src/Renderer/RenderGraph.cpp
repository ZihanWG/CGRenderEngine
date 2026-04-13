// Executes render passes once their named dependencies have been satisfied.
#include "Renderer/RenderGraph.h"

#include <stdexcept>
#include <unordered_set>

void RenderGraph::Clear()
{
    m_Passes.clear();
}

void RenderGraph::AddPass(
    std::string name,
    std::vector<std::string> dependencies,
    ExecuteCallback callback
)
{
    m_Passes.push_back(PassNode{
        std::move(name),
        std::move(dependencies),
        std::move(callback)
    });
}

void RenderGraph::Execute() const
{
    // A simple incremental scheduler is enough here because the graph is tiny and rebuilt every frame.
    std::unordered_set<std::string> executed;
    std::size_t executedCount = 0;

    while (executedCount < m_Passes.size())
    {
        bool progressed = false;

        for (const PassNode& pass : m_Passes)
        {
            if (executed.find(pass.name) != executed.end())
            {
                continue;
            }

            bool dependenciesReady = true;
            for (const std::string& dependency : pass.dependencies)
            {
                if (executed.find(dependency) == executed.end())
                {
                    dependenciesReady = false;
                    break;
                }
            }

            if (!dependenciesReady)
            {
                continue;
            }

            if (pass.callback)
            {
                pass.callback();
            }

            executed.insert(pass.name);
            ++executedCount;
            progressed = true;
        }

        if (!progressed)
        {
            // Cycles or missing dependencies should fail loudly instead of silently skipping passes.
            throw std::runtime_error("RenderGraph contains an unresolved pass dependency cycle.");
        }
    }
}
