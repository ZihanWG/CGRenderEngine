// Owns material default textures and writes per-draw material data into the shared UBO.
#pragma once

#include "Engine/RHI/RenderBufferTypes.h"
#include "Engine/RHI/ShaderBufferManager.h"
#include "Engine/RHI/Texture2D.h"

struct Material;

class MaterialBinder
{
public:
    void Initialize(ShaderBufferManager& bufferManager);
    // Bind all textures and upload the material block expected by the PBR shader.
    void Bind(const Material& material) const;

private:
    ShaderBufferManager* m_BufferManager = nullptr;
    Texture2D m_DefaultBaseColorTexture;
    Texture2D m_DefaultMetallicRoughnessTexture;
    Texture2D m_DefaultNormalTexture;
    Texture2D m_DefaultOcclusionTexture;
    Texture2D m_DefaultEmissiveTexture;
};
