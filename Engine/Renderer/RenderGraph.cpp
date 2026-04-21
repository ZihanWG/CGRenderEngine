// Executes render passes once their named dependencies have been satisfied.
#include "Engine/Renderer/RenderGraph.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace
{
    void ValidatePassNames(const std::vector<RenderPassDesc>& passes)
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
    }

    const RenderGraphResourceDesc& GetResourceDesc(
        const std::vector<RenderGraphResourceDesc>& resources,
        RenderGraphResourceHandle handle,
        const std::string& context
    )
    {
        if (!handle.IsValid() || handle.id >= resources.size())
        {
            throw std::runtime_error("RenderGraph " + context + " references an invalid resource handle.");
        }

        return resources[handle.id];
    }

    const std::string& GetResourceName(
        const std::vector<RenderGraphResourceDesc>& resources,
        RenderGraphResourceHandle handle,
        const std::string& context
    )
    {
        return GetResourceDesc(resources, handle, context).name;
    }

    void AddDependency(
        std::vector<std::vector<std::size_t>>& dependencies,
        std::size_t passIndex,
        std::size_t dependencyIndex
    )
    {
        if (passIndex == dependencyIndex)
        {
            return;
        }

        std::vector<std::size_t>& passDependencies = dependencies[passIndex];
        if (std::find(passDependencies.begin(), passDependencies.end(), dependencyIndex) == passDependencies.end())
        {
            passDependencies.push_back(dependencyIndex);
        }
    }

    void AddResourceTransition(
        std::vector<RenderGraphResourceTransition>& transitions,
        RenderGraphResourceHandle resource,
        std::size_t sourcePassIndex,
        std::size_t destinationPassIndex,
        RenderGraphResourceAccess sourceAccess,
        RenderGraphResourceAccess destinationAccess
    )
    {
        if (sourcePassIndex == destinationPassIndex)
        {
            return;
        }

        for (const RenderGraphResourceTransition& transition : transitions)
        {
            if (transition.resource == resource &&
                transition.sourcePass.id == sourcePassIndex &&
                transition.destinationPass.id == destinationPassIndex &&
                transition.sourceAccess == sourceAccess &&
                transition.destinationAccess == destinationAccess)
            {
                return;
            }
        }

        transitions.push_back(RenderGraphResourceTransition{
            resource,
            RenderGraphPassHandle{static_cast<std::uint32_t>(sourcePassIndex)},
            RenderGraphPassHandle{static_cast<std::uint32_t>(destinationPassIndex)},
            sourceAccess,
            destinationAccess
        });
    }

    struct ResourceProducer
    {
        std::size_t passIndex = 0;
        RenderGraphResourceAccess access = RenderGraphResourceAccess::Write;
    };

    struct ResourceUseList
    {
        std::vector<ResourceProducer> producers;
        std::vector<std::size_t> readers;
    };

    void AddResourceProducer(
        ResourceUseList& useList,
        std::size_t passIndex,
        RenderGraphResourceAccess access
    )
    {
        for (ResourceProducer& producer : useList.producers)
        {
            if (producer.passIndex == passIndex)
            {
                if (access == RenderGraphResourceAccess::Target)
                {
                    producer.access = RenderGraphResourceAccess::Target;
                }
                return;
            }
        }

        useList.producers.push_back(ResourceProducer{
            passIndex,
            access
        });
    }

    void ValidatePassDependencies(
        const std::vector<RenderPassDesc>& passes
    )
    {
        for (std::size_t passIndex = 0; passIndex < passes.size(); ++passIndex)
        {
            const RenderPassDesc& pass = passes[passIndex];
            for (const RenderGraphPassHandle dependency : pass.dependencies)
            {
                if (!dependency.IsValid() || dependency.id >= passes.size())
                {
                    throw std::runtime_error(
                        "RenderGraph pass '" + pass.name + "' depends on an invalid pass handle."
                    );
                }
                if (dependency.id == passIndex)
                {
                    throw std::runtime_error("RenderGraph pass '" + pass.name + "' depends on itself.");
                }
            }
        }
    }

    void ValidatePassCallbacks(const std::vector<RenderPassDesc>& passes)
    {
        for (const RenderPassDesc& pass : passes)
        {
            if (!pass.callback)
            {
                throw std::runtime_error("RenderGraph pass '" + pass.name + "' has no execute callback.");
            }
        }
    }

    std::vector<std::vector<std::size_t>> BuildExplicitDependencies(
        const std::vector<RenderPassDesc>& passes
    )
    {
        std::vector<std::vector<std::size_t>> dependencies(passes.size());
        for (std::size_t passIndex = 0; passIndex < passes.size(); ++passIndex)
        {
            const RenderPassDesc& pass = passes[passIndex];
            for (const RenderGraphPassHandle dependency : pass.dependencies)
            {
                AddDependency(dependencies, passIndex, dependency.id);
            }
        }

        return dependencies;
    }

    std::vector<std::vector<RenderGraphPassHandle>> BuildExecutionLevels(
        const std::vector<RenderPassDesc>& passes,
        const std::vector<std::vector<std::size_t>>& dependencies
    )
    {
        std::vector<std::vector<RenderGraphPassHandle>> executionLevels;

        std::vector<bool> executed(passes.size(), false);
        std::size_t executedCount = 0;
        while (executedCount < passes.size())
        {
            std::vector<RenderGraphPassHandle> readyPasses;

            for (std::size_t passIndex = 0; passIndex < passes.size(); ++passIndex)
            {
                if (executed[passIndex])
                {
                    continue;
                }

                bool dependenciesReady = true;
                for (const std::size_t dependencyIndex : dependencies[passIndex])
                {
                    if (!executed[dependencyIndex])
                    {
                        dependenciesReady = false;
                        break;
                    }
                }

                if (!dependenciesReady)
                {
                    continue;
                }

                readyPasses.push_back(RenderGraphPassHandle{static_cast<std::uint32_t>(passIndex)});
            }

            if (readyPasses.empty())
            {
                throw std::runtime_error("RenderGraph contains an unresolved pass dependency cycle.");
            }

            for (const RenderGraphPassHandle passHandle : readyPasses)
            {
                executed[passHandle.id] = true;
                ++executedCount;
            }

            executionLevels.push_back(std::move(readyPasses));
        }

        return executionLevels;
    }

    std::vector<RenderGraphPassHandle> FlattenExecutionLevels(
        const std::vector<std::vector<RenderGraphPassHandle>>& executionLevels
    )
    {
        std::size_t passCount = 0;
        for (const std::vector<RenderGraphPassHandle>& level : executionLevels)
        {
            passCount += level.size();
        }

        std::vector<RenderGraphPassHandle> executionOrder;
        executionOrder.reserve(passCount);
        for (const std::vector<RenderGraphPassHandle>& level : executionLevels)
        {
            executionOrder.insert(executionOrder.end(), level.begin(), level.end());
        }

        return executionOrder;
    }

    std::vector<ResourceUseList> BuildResourceUseLists(
        const std::vector<RenderPassDesc>& passes,
        const std::vector<RenderGraphResourceDesc>& resources
    )
    {
        std::vector<ResourceUseList> resourceUses(resources.size());

        for (std::size_t passIndex = 0; passIndex < passes.size(); ++passIndex)
        {
            const RenderPassDesc& pass = passes[passIndex];

            if (!pass.writes.empty() && !pass.target.IsValid())
            {
                throw std::runtime_error(
                    "RenderGraph pass '" + pass.name + "' writes resources but has no target."
                );
            }
            if (pass.target.IsValid())
            {
                GetResourceDesc(resources, pass.target, "pass target");
                AddResourceProducer(
                    resourceUses[pass.target.id],
                    passIndex,
                    RenderGraphResourceAccess::Target
                );
            }

            for (const RenderGraphResourceHandle resource : pass.reads)
            {
                GetResourceDesc(resources, resource, "pass read");
                resourceUses[resource.id].readers.push_back(passIndex);
            }

            for (const RenderGraphResourceHandle resource : pass.writes)
            {
                GetResourceName(resources, resource, "pass write");
                AddResourceProducer(
                    resourceUses[resource.id],
                    passIndex,
                    RenderGraphResourceAccess::Write
                );
            }
        }

        return resourceUses;
    }

    void AddResourceDependencies(
        const std::vector<RenderPassDesc>& passes,
        const std::vector<RenderGraphResourceDesc>& resources,
        std::vector<std::vector<std::size_t>>& dependencies,
        std::vector<RenderGraphResourceTransition>& transitions
    )
    {
        const std::vector<ResourceUseList> resourceUses = BuildResourceUseLists(passes, resources);

        for (std::uint32_t resourceIndex = 0; resourceIndex < resourceUses.size(); ++resourceIndex)
        {
            const RenderGraphResourceDesc& resourceDesc = resources[resourceIndex];
            const ResourceUseList& useList = resourceUses[resourceIndex];
            const RenderGraphResourceHandle resource{resourceIndex};

            if (!resourceDesc.imported && !useList.readers.empty() && useList.producers.empty())
            {
                throw std::runtime_error(
                    "RenderGraph reads graph resource '" + resourceDesc.name + "' but no pass produces it."
                );
            }

            if (useList.producers.size() > 1)
            {
                throw std::runtime_error(
                    "RenderGraph resource '" + resourceDesc.name +
                    "' has multiple producer passes. Create explicit resource versions instead."
                );
            }

            if (useList.producers.empty())
            {
                continue;
            }

            const ResourceProducer& producer = useList.producers.front();
            for (const std::size_t readerPassIndex : useList.readers)
            {
                AddDependency(dependencies, readerPassIndex, producer.passIndex);
                AddResourceTransition(
                    transitions,
                    resource,
                    producer.passIndex,
                    readerPassIndex,
                    producer.access,
                    RenderGraphResourceAccess::Read
                );
            }
        }
    }

    void MarkResourceUse(
        RenderGraphResourceLifetime& lifetime,
        std::size_t executionIndex
    )
    {
        lifetime.firstUse = std::min(lifetime.firstUse, executionIndex);
        lifetime.lastUse = std::max(lifetime.lastUse, executionIndex);
    }

    std::vector<RenderGraphResourceLifetime> BuildResourceLifetimes(
        const std::vector<RenderPassDesc>& passes,
        const std::vector<RenderGraphResourceDesc>& resources,
        const std::vector<RenderGraphPassHandle>& executionOrder
    )
    {
        std::vector<RenderGraphResourceLifetime> lifetimes;
        lifetimes.reserve(resources.size());
        for (std::uint32_t resourceIndex = 0; resourceIndex < resources.size(); ++resourceIndex)
        {
            lifetimes.push_back(RenderGraphResourceLifetime{
                RenderGraphResourceHandle{resourceIndex}
            });
        }

        for (std::size_t executionIndex = 0; executionIndex < executionOrder.size(); ++executionIndex)
        {
            const RenderPassDesc& pass = passes[executionOrder[executionIndex].id];

            if (pass.target.IsValid())
            {
                RenderGraphResourceLifetime& lifetime = lifetimes[pass.target.id];
                MarkResourceUse(lifetime, executionIndex);
                ++lifetime.targetCount;
            }

            for (const RenderGraphResourceHandle resource : pass.reads)
            {
                RenderGraphResourceLifetime& lifetime = lifetimes[resource.id];
                MarkResourceUse(lifetime, executionIndex);
                ++lifetime.readCount;
            }

            for (const RenderGraphResourceHandle resource : pass.writes)
            {
                RenderGraphResourceLifetime& lifetime = lifetimes[resource.id];
                MarkResourceUse(lifetime, executionIndex);
                ++lifetime.writeCount;
            }
        }

        return lifetimes;
    }
}

void RenderGraph::Clear()
{
    m_Passes.clear();
    m_Resources.clear();
    InvalidateCompiledData();
}

RenderGraph::PassBuilder::PassBuilder(RenderGraph& graph, RenderGraphPassHandle handle)
    : m_Graph(graph), m_Handle(handle)
{
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Read(RenderGraphResourceHandle resource)
{
    m_Graph.GetMutablePass(m_Handle).reads.push_back(resource);
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Type(RenderGraphPassType type)
{
    m_Graph.GetMutablePass(m_Handle).passType = type;
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Read(
    std::initializer_list<RenderGraphResourceHandle> resources
)
{
    RenderPassDesc& pass = m_Graph.GetMutablePass(m_Handle);
    pass.reads.insert(pass.reads.end(), resources.begin(), resources.end());
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Write(RenderGraphResourceHandle resource)
{
    m_Graph.GetMutablePass(m_Handle).writes.push_back(resource);
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Write(
    std::initializer_list<RenderGraphResourceHandle> resources
)
{
    RenderPassDesc& pass = m_Graph.GetMutablePass(m_Handle);
    pass.writes.insert(pass.writes.end(), resources.begin(), resources.end());
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Target(RenderGraphResourceHandle resource)
{
    m_Graph.GetMutablePass(m_Handle).target = resource;
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::DependsOn(RenderGraphPassHandle pass)
{
    m_Graph.GetMutablePass(m_Handle).dependencies.push_back(pass);
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::DependsOn(
    std::initializer_list<RenderGraphPassHandle> passes
)
{
    RenderPassDesc& pass = m_Graph.GetMutablePass(m_Handle);
    pass.dependencies.insert(pass.dependencies.end(), passes.begin(), passes.end());
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraph::PassBuilder& RenderGraph::PassBuilder::Execute(ExecuteCallback callback)
{
    m_Graph.GetMutablePass(m_Handle).callback = std::move(callback);
    m_Graph.InvalidateCompiledData();
    return *this;
}

RenderGraphResourceHandle RenderGraph::ImportResource(std::string name, RenderGraphResourceType type)
{
    return RegisterResource(std::move(name), type, true);
}

RenderGraphResourceHandle RenderGraph::CreateResource(std::string name, RenderGraphResourceType type)
{
    return RegisterResource(std::move(name), type, false);
}

RenderGraphResourceHandle RenderGraph::RegisterResource(
    std::string name,
    RenderGraphResourceType type,
    bool imported
)
{
    if (name.empty())
    {
        throw std::runtime_error("RenderGraph resource name cannot be empty.");
    }

    for (std::uint32_t index = 0; index < m_Resources.size(); ++index)
    {
        if (m_Resources[index].name == name)
        {
            throw std::runtime_error("RenderGraph contains duplicate resource name '" + name + "'.");
        }
    }

    const std::uint32_t id = static_cast<std::uint32_t>(m_Resources.size());
    m_Resources.push_back(RenderGraphResourceDesc{
        std::move(name),
        type,
        imported
    });
    InvalidateCompiledData();

    return RenderGraphResourceHandle{id};
}

RenderGraphPassHandle RenderGraph::AddPass(RenderPassDesc pass)
{
    const std::uint32_t id = static_cast<std::uint32_t>(m_Passes.size());
    m_Passes.push_back(std::move(pass));
    InvalidateCompiledData();

    return RenderGraphPassHandle{id};
}

RenderGraph::PassBuilder RenderGraph::AddPass(std::string name)
{
    RenderPassDesc pass;
    pass.name = std::move(name);
    return PassBuilder(*this, AddPass(std::move(pass)));
}

RenderPassDesc& RenderGraph::GetMutablePass(RenderGraphPassHandle handle)
{
    if (!handle.IsValid() || handle.id >= m_Passes.size())
    {
        throw std::runtime_error("RenderGraph received an invalid mutable pass handle.");
    }

    return m_Passes[handle.id];
}

void RenderGraph::InvalidateCompiledData()
{
    m_ExecutionOrder.clear();
    m_ExecutionLevels.clear();
    m_CompiledDependencies.clear();
    m_ResourceLifetimes.clear();
    m_ResourceTransitions.clear();
    m_Compiled = false;
}

const RenderPassDesc& RenderGraph::GetPass(RenderGraphPassHandle handle) const
{
    if (!handle.IsValid() || handle.id >= m_Passes.size())
    {
        throw std::runtime_error("RenderGraph::GetPass received an invalid pass handle.");
    }

    return m_Passes[handle.id];
}

const RenderGraphResourceDesc& RenderGraph::GetResource(RenderGraphResourceHandle handle) const
{
    if (!handle.IsValid() || handle.id >= m_Resources.size())
    {
        throw std::runtime_error("RenderGraph::GetResource received an invalid resource handle.");
    }

    return m_Resources[handle.id];
}

void RenderGraph::Compile()
{
    ValidatePassNames(m_Passes);
    ValidatePassDependencies(m_Passes);
    ValidatePassCallbacks(m_Passes);
    m_CompiledDependencies = BuildExplicitDependencies(m_Passes);
    m_ResourceTransitions.clear();
    AddResourceDependencies(m_Passes, m_Resources, m_CompiledDependencies, m_ResourceTransitions);
    m_ExecutionLevels = BuildExecutionLevels(m_Passes, m_CompiledDependencies);
    m_ExecutionOrder = FlattenExecutionLevels(m_ExecutionLevels);
    m_ResourceLifetimes = BuildResourceLifetimes(m_Passes, m_Resources, m_ExecutionOrder);
    m_Compiled = true;
}

void RenderGraph::Execute() const
{
    if (!m_Compiled)
    {
        throw std::runtime_error("RenderGraph::Execute requires Compile to run first.");
    }

    for (const RenderGraphPassHandle passHandle : m_ExecutionOrder)
    {
        const RenderPassDesc& pass = GetPass(passHandle);
        if (pass.callback)
        {
            pass.callback();
        }
    }
}
