// Directional shadow-map pass used by the forward PBR shader.
#include "Engine/Renderer/ShadowPass.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

#include "Engine/Assets/ResourceManager.h"
#include "Engine/RHI/Mesh.h"
#include "Engine/RHI/RenderBufferTypes.h"
#include "Engine/Renderer/RenderSubmission.h"
#include "Engine/Renderer/RenderWorld.h"
#include "Engine/Scene/Material.h"

namespace
{
    void ApplyShadowCullMode(const Material* material)
    {
        if (material && material->cullMode == MaterialCullMode::None)
        {
            glDisable(GL_CULL_FACE);
            return;
        }

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
    }
}

void ShadowPass::Initialize(ResourceManager& resourceManager, ShaderBufferManager& bufferManager)
{
    if (m_Initialized)
    {
        return;
    }

    m_BufferManager = &bufferManager;
    m_Shader = resourceManager.LoadShader("Shaders/shadow.vert", "Shaders/shadow.frag");
    m_Shader->Use();
    m_Shader->SetUniformBlockBinding("ObjectData", bufferManager.GetBindingPoint(BufferBindingSlot::Object));
    m_ShadowTexture.Allocate(
        kResolution,
        kResolution,
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr,
        GL_NEAREST,
        GL_NEAREST,
        GL_CLAMP_TO_BORDER,
        GL_CLAMP_TO_BORDER
    );
    m_ShadowTexture.SetBorderColor(1.0f, 1.0f, 1.0f, 1.0f);
    // Border depth stays lit outside the map to avoid hard clipping at the cascade edge.

    m_Framebuffer.Bind();
    m_Framebuffer.AttachDepthTexture(m_ShadowTexture);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (!m_Framebuffer.CheckComplete())
    {
        throw std::runtime_error("Shadow framebuffer is incomplete.");
    }

    Framebuffer::Unbind();
    m_Initialized = true;
}

void ShadowPass::Execute(const RenderWorld& renderWorld, const RenderSubmission& submission, bool enabled)
{
    if (!m_Initialized)
    {
        return;
    }

    glViewport(0, 0, kResolution, kResolution);
    m_Framebuffer.Bind();
    glClear(GL_DEPTH_BUFFER_BIT);

    if (!enabled)
    {
        Framebuffer::Unbind();
        return;
    }

    m_Shader->Use();
    m_Shader->SetMat4("uLightSpaceMatrix", renderWorld.perViewData.lightSpaceMatrix);

    for (const InstancedDrawBatch& batch : submission.renderQueue.shadowBatches)
    {
        if (!batch.mesh)
        {
            continue;
        }

        for (std::size_t startIndex = 0; startIndex < batch.perObjectDataIndices.size(); startIndex += kMaxObjectMatricesPerDraw)
        {
            ObjectUniformData objectData{};
            const std::size_t remainingCount = batch.perObjectDataIndices.size() - startIndex;
            const std::size_t instanceCount = std::min<std::size_t>(remainingCount, kMaxObjectMatricesPerDraw);
            for (std::size_t localIndex = 0; localIndex < instanceCount; ++localIndex)
            {
                const std::size_t perObjectDataIndex = batch.perObjectDataIndices[startIndex + localIndex];
                if (perObjectDataIndex >= renderWorld.perObjectData.size())
                {
                    continue;
                }

                objectData.modelMatrices[localIndex] = renderWorld.perObjectData[perObjectDataIndex].modelMatrix;
            }

            const BufferSlice objectSlice = m_BufferManager->UploadUniformRing(BufferBindingSlot::Object, objectData);
            m_BufferManager->BindRange(BufferBindingSlot::Object, objectSlice.offset, objectSlice.size);

            ApplyShadowCullMode(batch.material);
            batch.mesh->DrawInstanced(instanceCount);
        }
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    Framebuffer::Unbind();
}
