// Binds the engine's shared uniform buffers to fixed binding slots.
#include "Engine/RHI/ShaderBufferManager.h"

#include <algorithm>

namespace
{
    // Grow to the next power of two so a slot settles on a stable size after a frame or
    // two instead of reallocating every time the draw count creeps up by one.
    std::size_t NextPowerOfTwo(std::size_t value)
    {
        std::size_t result = 1;
        while (result < value)
        {
            result <<= 1;
        }

        return result;
    }
}

ShaderBufferManager::~ShaderBufferManager()
{
    // Sync objects are GL objects. The Renderer owns this manager and is destroyed before
    // the window, so the context is still current here.
    for (GLsync& fence : m_FrameFences)
    {
        if (fence)
        {
            glDeleteSync(fence);
            fence = nullptr;
        }
    }
}

void ShaderBufferManager::BeginFrame()
{
    m_FrameSlice = (m_FrameSlice + 1) % kFrameSliceCount;
    WaitForFrameSlice(m_FrameSlice);

    for (std::size_t slotIndex = 0; slotIndex < kSlotCount; ++slotIndex)
    {
        if (!m_IsRingBuffer[slotIndex])
        {
            continue;
        }

        // Reallocation has to happen at a frame boundary rather than mid-frame: glBufferData
        // replaces the storage behind ranges that earlier draw calls have already bound.
        if (m_FrameElementCounts[slotIndex] > m_ElementsPerFrame[slotIndex])
        {
            AllocateRing(slotIndex, NextPowerOfTwo(m_FrameElementCounts[slotIndex]));
        }

        m_FrameElementCounts[slotIndex] = 0;
        m_ElementCursors[slotIndex] = RingSliceBegin(slotIndex);
    }
}

void ShaderBufferManager::EndFrame()
{
    if (m_FrameFences[m_FrameSlice])
    {
        glDeleteSync(m_FrameFences[m_FrameSlice]);
    }

    // Signalled once the GPU finishes every command issued for this frame, which is exactly
    // when this frame's region becomes safe to overwrite.
    m_FrameFences[m_FrameSlice] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void ShaderBufferManager::WaitForFrameSlice(std::size_t frameSlice)
{
    GLsync& fence = m_FrameFences[frameSlice];
    if (!fence)
    {
        return;
    }

    // One second is a safety valve, not an expected path. Timing out means the GPU is more
    // than kFrameSliceCount frames behind; proceeding anyway just falls back to the driver's
    // own hazard handling instead of hanging the window.
    constexpr GLuint64 kTimeoutNanoseconds = 1000000000ull;
    glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, kTimeoutNanoseconds);
    glDeleteSync(fence);
    fence = nullptr;
}

void ShaderBufferManager::InitializeUniformBuffer(BufferBindingSlot slot, std::size_t size)
{
    const std::size_t slotIndex = static_cast<std::size_t>(slot);
    // Re-initialization is allowed and simply reallocates the slot to the new size.
    m_Buffers[slotIndex].Allocate(ShaderBufferKind::Uniform, size);
    m_Sizes[slotIndex] = size;
    m_ElementStrides[slotIndex] = size;
    m_ElementCursors[slotIndex] = 0;
    m_ElementsPerFrame[slotIndex] = 1;
    m_FrameElementCounts[slotIndex] = 0;
    m_Initialized[slotIndex] = true;
    m_IsRingBuffer[slotIndex] = false;
    m_Buffers[slotIndex].BindBase(GetBindingPoint(slot));
}

void ShaderBufferManager::InitializeUniformRingBuffer(
    BufferBindingSlot slot,
    std::size_t elementSize,
    std::size_t elementsPerFrame
)
{
    EnsureUniformAlignment();
    const std::size_t slotIndex = static_cast<std::size_t>(slot);
    m_ElementStrides[slotIndex] = AlignUniformSize(elementSize);
    m_FrameElementCounts[slotIndex] = 0;
    m_Initialized[slotIndex] = true;
    m_IsRingBuffer[slotIndex] = true;
    AllocateRing(slotIndex, elementsPerFrame);
}

void ShaderBufferManager::AllocateRing(std::size_t slotIndex, std::size_t elementsPerFrame)
{
    const std::size_t safeElementsPerFrame = std::max<std::size_t>(elementsPerFrame, 1);
    const std::size_t totalSize = m_ElementStrides[slotIndex] * safeElementsPerFrame * kFrameSliceCount;

    m_Buffers[slotIndex].Allocate(ShaderBufferKind::Uniform, totalSize);
    m_Sizes[slotIndex] = totalSize;
    m_ElementsPerFrame[slotIndex] = safeElementsPerFrame;
    m_ElementCursors[slotIndex] = RingSliceBegin(slotIndex);
    m_Buffers[slotIndex].BindBase(GetBindingPoint(static_cast<BufferBindingSlot>(slotIndex)));
}

std::size_t ShaderBufferManager::RingSliceBegin(std::size_t slotIndex) const
{
    return m_FrameSlice * m_ElementsPerFrame[slotIndex] * m_ElementStrides[slotIndex];
}

std::size_t ShaderBufferManager::RingSliceEnd(std::size_t slotIndex) const
{
    return RingSliceBegin(slotIndex) + m_ElementsPerFrame[slotIndex] * m_ElementStrides[slotIndex];
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

std::size_t ShaderBufferManager::GetAllocatedSize(BufferBindingSlot slot) const
{
    return m_Sizes[static_cast<std::size_t>(slot)];
}

std::size_t ShaderBufferManager::GetElementsPerFrame(BufferBindingSlot slot) const
{
    return m_ElementsPerFrame[static_cast<std::size_t>(slot)];
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
