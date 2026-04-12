#include "Renderer/ShaderBufferManager.h"

void ShaderBufferManager::InitializeUniformBuffer(BufferBindingSlot slot, std::size_t size)
{
    const std::size_t slotIndex = static_cast<std::size_t>(slot);
    m_Buffers[slotIndex].Allocate(ShaderBufferKind::Uniform, size);
    m_Sizes[slotIndex] = size;
    m_Initialized[slotIndex] = true;
    m_Buffers[slotIndex].BindBase(GetBindingPoint(slot));
}

void ShaderBufferManager::Bind(BufferBindingSlot slot) const
{
    const std::size_t slotIndex = static_cast<std::size_t>(slot);
    if (!m_Initialized[slotIndex])
    {
        return;
    }

    m_Buffers[slotIndex].BindBase(GetBindingPoint(slot));
}

unsigned int ShaderBufferManager::GetBindingPoint(BufferBindingSlot slot) const
{
    return static_cast<unsigned int>(slot);
}
