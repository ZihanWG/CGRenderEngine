// Binds the engine's shared uniform buffers to fixed binding slots.
#include "Engine/RHI/ShaderBufferManager.h"

#include <algorithm>

void ShaderBufferManager::BeginFrame()
{
    for (std::size_t slotIndex = 0; slotIndex < kSlotCount; ++slotIndex)
    {
        if (m_IsRingBuffer[slotIndex])
        {
            m_ElementCursors[slotIndex] = 0;
        }
    }
}

void ShaderBufferManager::InitializeUniformBuffer(BufferBindingSlot slot, std::size_t size)
{
    const std::size_t slotIndex = static_cast<std::size_t>(slot);
    // Re-initialization is allowed and simply reallocates the slot to the new size.
    m_Buffers[slotIndex].Allocate(ShaderBufferKind::Uniform, size);
    m_Sizes[slotIndex] = size;
    m_ElementStrides[slotIndex] = size;
    m_ElementCursors[slotIndex] = 0;
    m_Initialized[slotIndex] = true;
    m_IsRingBuffer[slotIndex] = false;
    m_Buffers[slotIndex].BindBase(GetBindingPoint(slot));
}

void ShaderBufferManager::InitializeUniformRingBuffer(
    BufferBindingSlot slot,
    std::size_t elementSize,
    std::size_t elementCapacity
)
{
    EnsureUniformAlignment();
    const std::size_t slotIndex = static_cast<std::size_t>(slot);
    const std::size_t stride = AlignUniformSize(elementSize);
    const std::size_t totalSize = stride * std::max<std::size_t>(elementCapacity, 1);

    m_Buffers[slotIndex].Allocate(ShaderBufferKind::Uniform, totalSize);
    m_Sizes[slotIndex] = totalSize;
    m_ElementStrides[slotIndex] = stride;
    m_ElementCursors[slotIndex] = 0;
    m_Initialized[slotIndex] = true;
    m_IsRingBuffer[slotIndex] = true;
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

void ShaderBufferManager::BindRange(BufferBindingSlot slot, std::size_t offset, std::size_t size) const
{
    const std::size_t slotIndex = static_cast<std::size_t>(slot);
    if (!m_Initialized[slotIndex])
    {
        return;
    }

    m_Buffers[slotIndex].BindRange(GetBindingPoint(slot), offset, size);
}

unsigned int ShaderBufferManager::GetBindingPoint(BufferBindingSlot slot) const
{
    return static_cast<unsigned int>(slot);
}

void ShaderBufferManager::EnsureUniformAlignment()
{
    if (m_HasAlignment)
    {
        return;
    }

    GLint alignment = 256;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    m_UniformOffsetAlignment = std::max<std::size_t>(static_cast<std::size_t>(alignment), 1);
    m_HasAlignment = true;
}

std::size_t ShaderBufferManager::AlignUniformSize(std::size_t size) const
{
    const std::size_t alignment = std::max<std::size_t>(m_UniformOffsetAlignment, 1);
    const std::size_t remainder = size % alignment;
    return remainder == 0 ? size : size + (alignment - remainder);
}
