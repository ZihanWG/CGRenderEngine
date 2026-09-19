#include "Tests/TestSupport.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Engine/Renderer/RenderGraph.h"

int main()
{
    TestContext test;

    RenderGraph graph;
    const auto input = graph.ImportResource("input", RenderGraphResourceType::Texture);
    const auto intermediate = graph.CreateResource("intermediate", RenderGraphResourceType::Texture);
    const auto output = graph.CreateResource("output", RenderGraphResourceType::Texture);
    std::vector<std::string> calls;

    graph.AddPass("produce")
        .Read(input)
        .Write(intermediate)
        .Execute([&]() { calls.push_back("produce"); });
    graph.AddPass("consume")
        .Read(intermediate)
        .Write(output)
        .Execute([&]() { calls.push_back("consume"); });

    graph.Compile();
    EXPECT(test, graph.IsCompiled());
    EXPECT(test, graph.GetExecutionOrder().size() == 2);
    EXPECT(test, graph.GetExecutionOrder()[0].id == 0);
    EXPECT(test, graph.GetExecutionOrder()[1].id == 1);
    EXPECT(test, graph.GetResourceTransitions().size() == 1);
    EXPECT(test, graph.GetResourceLifetimes()[intermediate.id].readCount == 1);
    EXPECT(test, graph.GetResourceLifetimes()[intermediate.id].writeCount == 1);
    EXPECT(test, graph.GetCompileCount() == 1);
    graph.Compile();
    EXPECT(test, graph.GetCompileCount() == 1);

    graph.Execute();
    EXPECT(test, calls == std::vector<std::string>({"produce", "consume"}));

    RenderGraph missingProducer;
    const auto orphan = missingProducer.CreateResource("orphan", RenderGraphResourceType::Texture);
    missingProducer.AddPass("reader").Read(orphan).Execute([]() {});
    EXPECT_THROWS(test, missingProducer.Compile());

    RenderGraph multipleProducers;
    const auto shared = multipleProducers.CreateResource("shared", RenderGraphResourceType::Texture);
    multipleProducers.AddPass("first").Write(shared).Execute([]() {});
    multipleProducers.AddPass("second").Write(shared).Execute([]() {});
    EXPECT_THROWS(test, multipleProducers.Compile());

    RenderGraph invalidCpuAccess;
    const auto gpuTexture = invalidCpuAccess.ImportResource("gpu", RenderGraphResourceType::Texture);
    invalidCpuAccess.AddPass("cpu")
        .Type(RenderGraphPassType::CPU)
        .Read(gpuTexture)
        .Execute([]() {});
    EXPECT_THROWS(test, invalidCpuAccess.Compile());

    RenderGraph feedbackLoop;
    const auto feedback = feedbackLoop.ImportResource("feedback", RenderGraphResourceType::Texture);
    feedbackLoop.AddPass("feedback_loop")
        .Read(feedback)
        .Write(feedback)
        .Execute([]() {});
    EXPECT_THROWS(test, feedbackLoop.Compile());

    RenderGraph duplicateRead;
    const auto duplicate = duplicateRead.ImportResource("duplicate", RenderGraphResourceType::Texture);
    duplicateRead.AddPass("duplicate_read")
        .Read({duplicate, duplicate})
        .Execute([]() {});
    EXPECT_THROWS(test, duplicateRead.Compile());

    // A graphics pass that throws must still join the CPU jobs its level started. Those
    // jobs run pass callbacks that reference renderer state, so letting the stack unwind
    // past them leaves worker threads writing into objects the main thread is destroying.
    {
        RenderGraph throwingGraph;
        const auto cpuInput = throwingGraph.ImportResource("cpu_input", RenderGraphResourceType::CPUData);
        const auto cpuOutput = throwingGraph.CreateResource("cpu_output", RenderGraphResourceType::CPUData);
        const auto gpuOutput = throwingGraph.CreateResource("gpu_output", RenderGraphResourceType::Texture);

        std::atomic<bool> cpuPassFinished{false};

        throwingGraph.AddPass("slow_cpu")
            .Type(RenderGraphPassType::CPU)
            .Read(cpuInput)
            .Write(cpuOutput)
            .Execute([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                cpuPassFinished.store(true, std::memory_order_release);
            });

        // Declares no dependency on the CPU pass, so both land in execution level 0 and the
        // CPU job is guaranteed to still be in flight when this one throws.
        throwingGraph.AddPass("throwing_graphics")
            .Write(gpuOutput)
            .Execute([]() { throw std::runtime_error("graphics pass failed"); });

        throwingGraph.Compile();
        EXPECT(test, throwingGraph.GetExecutionLevels().size() == 1);
        EXPECT_THROWS(test, throwingGraph.Execute());
        EXPECT(test, cpuPassFinished.load(std::memory_order_acquire));
    }

    const auto extra = graph.ImportResource("extra", RenderGraphResourceType::Texture);
    graph.AddPass("extra_pass").Read(extra).Execute([]() {});
    graph.Compile();
    EXPECT(test, graph.GetCompileCount() == 2);

    return test.Finish("RenderGraphTests");
}
