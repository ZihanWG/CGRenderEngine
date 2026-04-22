// Minimal resource-aware render graph. Passes declare reads, writes, targets, and dependencies.
//
// This is still intentionally lighter than a transient resource allocator, but it now models
// what each pass consumes and produces so the submission pipeline is not just "call pass A/B/C".
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

enum class RenderGraphResourceType
{
    CPUData,
    Buffer,
    Texture,
    Framebuffer,
    Backbuffer
};

enum class RenderGraphPassType
{
    Graphics,
    Compute,
    CPU
};

struct RenderGraphResourceHandle
{
    static constexpr std::uint32_t kInvalidId = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t id = kInvalidId;

    bool IsValid() const { return id != kInvalidId; }
    bool operator==(const RenderGraphResourceHandle& other) const { return id == other.id; }
    bool operator!=(const RenderGraphResourceHandle& other) const { return !(*this == other); }
};

struct RenderGraphPassHandle
{
    static constexpr std::uint32_t kInvalidId = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t id = kInvalidId;

    bool IsValid() const { return id != kInvalidId; }
    bool operator==(const RenderGraphPassHandle& other) const { return id == other.id; }
    bool operator!=(const RenderGraphPassHandle& other) const { return !(*this == other); }
};

struct RenderGraphResourceDesc
{
    std::string name;
    RenderGraphResourceType type = RenderGraphResourceType::Texture;
    bool imported = false;
};

struct RenderGraphResourceLifetime
{
    static constexpr std::size_t kUnusedPass = std::numeric_limits<std::size_t>::max();

    RenderGraphResourceHandle resource;
    std::size_t firstUse = kUnusedPass;
    std::size_t lastUse = kUnusedPass;
    std::size_t readCount = 0;
    std::size_t writeCount = 0;
    std::size_t targetCount = 0;

    bool IsUsed() const { return firstUse != kUnusedPass; }
};

enum class RenderGraphResourceAccess
{
    Read,
    Write,
    Target
};

struct RenderGraphResourceTransition
{
    RenderGraphResourceHandle resource;
    RenderGraphPassHandle sourcePass;
    RenderGraphPassHandle destinationPass;
    RenderGraphResourceAccess sourceAccess = RenderGraphResourceAccess::Write;
    RenderGraphResourceAccess destinationAccess = RenderGraphResourceAccess::Read;
};

struct RenderPassDesc
{
    std::string name;
    RenderGraphPassType passType = RenderGraphPassType::Graphics;
    std::vector<RenderGraphResourceHandle> reads;
    std::vector<RenderGraphResourceHandle> writes;
    RenderGraphResourceHandle target;
    std::vector<RenderGraphPassHandle> dependencies;
    std::function<void()> callback;
};

class RenderGraph
{
public:
    using ExecuteCallback = std::function<void()>;

    class PassBuilder
    {
    public:
        PassBuilder(RenderGraph& graph, RenderGraphPassHandle handle);

        RenderGraphPassHandle GetHandle() const { return m_Handle; }
        PassBuilder& Type(RenderGraphPassType type);
        PassBuilder& Read(RenderGraphResourceHandle resource);
        PassBuilder& Read(std::initializer_list<RenderGraphResourceHandle> resources);
        PassBuilder& Write(RenderGraphResourceHandle resource);
        PassBuilder& Write(std::initializer_list<RenderGraphResourceHandle> resources);
        PassBuilder& Target(RenderGraphResourceHandle resource);
        PassBuilder& DependsOn(RenderGraphPassHandle pass);
        PassBuilder& DependsOn(std::initializer_list<RenderGraphPassHandle> passes);
        PassBuilder& Execute(ExecuteCallback callback);

    private:
        RenderGraph& m_Graph;
        RenderGraphPassHandle m_Handle;
    };

    // Remove all registered passes before building the next frame graph.
    void Clear();
    // Imports an externally owned resource, such as Scene data or the swapchain backbuffer.
    RenderGraphResourceHandle ImportResource(std::string name, RenderGraphResourceType type);
    // Declares a graph-produced resource. This project still owns allocations in pass objects,
    // but the graph now has an explicit resource identity for validation and future lifetimes.
    RenderGraphResourceHandle CreateResource(std::string name, RenderGraphResourceType type);
    // Adds a pass plus the resources it reads/writes and the passes it depends on.
    RenderGraphPassHandle AddPass(RenderPassDesc pass);
    // Builder overload for concise pass declarations.
    PassBuilder AddPass(std::string name);
    // Validates the graph and builds a stable execution order for this frame.
    void Compile();
    // Executes the already-compiled execution levels. CPU passes are submitted to JobSystem;
    // graphics/compute passes remain on the calling thread for API context ownership.
    void Execute() const;
    bool IsCompiled() const { return m_Compiled; }
    const std::vector<RenderPassDesc>& GetPasses() const { return m_Passes; }
    const std::vector<RenderGraphResourceDesc>& GetResources() const { return m_Resources; }
    const std::vector<RenderGraphPassHandle>& GetExecutionOrder() const { return m_ExecutionOrder; }
    const std::vector<std::vector<RenderGraphPassHandle>>& GetExecutionLevels() const
    {
        return m_ExecutionLevels;
    }
    const std::vector<RenderGraphResourceLifetime>& GetResourceLifetimes() const
    {
        return m_ResourceLifetimes;
    }
    const std::vector<RenderGraphResourceTransition>& GetResourceTransitions() const
    {
        return m_ResourceTransitions;
    }
    const RenderPassDesc& GetPass(RenderGraphPassHandle handle) const;
    const RenderGraphResourceDesc& GetResource(RenderGraphResourceHandle handle) const;

private:
    RenderGraphResourceHandle RegisterResource(
        std::string name,
        RenderGraphResourceType type,
        bool imported
    );
    RenderPassDesc& GetMutablePass(RenderGraphPassHandle handle);
    void InvalidateCompiledData();

    std::vector<RenderPassDesc> m_Passes;
    std::vector<RenderGraphResourceDesc> m_Resources;
    std::vector<RenderGraphPassHandle> m_ExecutionOrder;
    std::vector<std::vector<RenderGraphPassHandle>> m_ExecutionLevels;
    std::vector<std::vector<std::size_t>> m_CompiledDependencies;
    std::vector<RenderGraphResourceLifetime> m_ResourceLifetimes;
    std::vector<RenderGraphResourceTransition> m_ResourceTransitions;
    bool m_Compiled = false;
};
