// Executes render passes once their named dependencies have been satisfied.
#include "Engine/Renderer/RenderGraph.h"

#include <functional>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace
{
    std::unordered_map<std::string, std::size_t> BuildPassIndexByName(
        const std::vector<RenderPassDesc>& passes
    )
    {
        std::unordered_map<std::string, std::size_t> passIndexByName;
        passIndexByName.reserve(passes.size());

        for (std::size_t index = 0; index < passes.size(); ++index)
        {
            const std::string& passName = passes[index].name;
            if (passName.empty())
            {
                throw std::runtime_error("RenderGraph pass name cannot be empty.");
            }

            const auto [_, inserted] = passIndexByName.emplace(passName, index);
            if (!inserted)
            {
                throw std::runtime_error("RenderGraph contains duplicate pass name '" + passName + "'.");
            }
        }

        return passIndexByName;
    }

    bool DependsOnPass(
        const std::vector<RenderPassDesc>& passes,
        const std::unordered_map<std::string, std::size_t>& passIndexByName,
        std::size_t passIndex,
        const std::string& dependencyName,
        std::unordered_set<std::size_t>& visited
    )
    {
        if (!visited.insert(passIndex).second)
        {
            return false;
        }

        const RenderPassDesc& pass = passes[passIndex];
        for (const std::string& dependency : pass.dependencies)
        {
            if (dependency == dependencyName)
            {
                return true;
            }

            const auto dependencyIt = passIndexByName.find(dependency);
            if (dependencyIt != passIndexByName.end() &&
                DependsOnPass(passes, passIndexByName, dependencyIt->second, dependencyName, visited))
            {
                return true;
            }
        }

        return false;
    }

    bool HasDependencyPath(
        const std::vector<RenderPassDesc>& passes,
        const std::unordered_map<std::string, std::size_t>& passIndexByName,
        std::size_t passIndex,
        const std::string& dependencyName
    )
    {
        std::unordered_set<std::size_t> visited;
        return DependsOnPass(passes, passIndexByName, passIndex, dependencyName, visited);
    }

    void ValidatePassDependencies(
        const std::vector<RenderPassDesc>& passes,
        const std::unordered_map<std::string, std::size_t>& passIndexByName
    )
    {
        for (const RenderPassDesc& pass : passes)
        {
            for (const std::string& dependency : pass.dependencies)
            {
                if (passIndexByName.find(dependency) == passIndexByName.end())
                {
                    throw std::runtime_error(
                        "RenderGraph pass '" + pass.name +
                        "' depends on unknown pass '" + dependency + "'."
                    );
                }
            }
        }
    }

    void ValidatePassResources(
        const std::vector<RenderPassDesc>& passes,
        const std::unordered_map<std::string, std::size_t>& passIndexByName
    )
    {
        std::unordered_map<std::string, std::string> lastWriterByResource;

        for (std::size_t passIndex = 0; passIndex < passes.size(); ++passIndex)
        {
            const RenderPassDesc& pass = passes[passIndex];

            if (!pass.writes.empty() && pass.target.empty())
            {
                throw std::runtime_error(
                    "RenderGraph pass '" + pass.name + "' writes resources but has no target."
                );
            }

            for (const std::string& resource : pass.reads)
            {
                const auto writerIt = lastWriterByResource.find(resource);
                if (writerIt == lastWriterByResource.end())
                {
                    continue;
                }

                if (!HasDependencyPath(passes, passIndexByName, passIndex, writerIt->second))
                {
                    throw std::runtime_error(
                        "RenderGraph pass '" + pass.name + "' reads resource '" + resource +
                        "' without depending on writer pass '" + writerIt->second + "'."
                    );
                }
            }

            for (const std::string& resource : pass.writes)
            {
                const auto writerIt = lastWriterByResource.find(resource);
                if (writerIt != lastWriterByResource.end() &&
                    !HasDependencyPath(passes, passIndexByName, passIndex, writerIt->second))
                {
                    throw std::runtime_error(
                        "RenderGraph pass '" + pass.name + "' writes resource '" + resource +
                        "' without depending on previous writer pass '" + writerIt->second + "'."
                    );
                }

                lastWriterByResource.insert_or_assign(resource, pass.name);
            }
        }
    }
}

void RenderGraph::Clear()
{
    m_Passes.clear();
}

void RenderGraph::AddPass(RenderPassDesc pass)
{
    m_Passes.push_back(std::move(pass));
}

void RenderGraph::Execute() const
{
    const std::unordered_map<std::string, std::size_t> passIndexByName = BuildPassIndexByName(m_Passes);
    ValidatePassDependencies(m_Passes, passIndexByName);
    ValidatePassResources(m_Passes, passIndexByName);

    // A simple incremental scheduler is enough here because the graph is tiny and rebuilt every frame.
    std::unordered_set<std::string> executed;
    std::size_t executedCount = 0;

    while (executedCount < m_Passes.size())
    {
        bool progressed = false;

        for (const RenderPassDesc& pass : m_Passes)
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
