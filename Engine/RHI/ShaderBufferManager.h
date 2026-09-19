// Central binding-point registry for the engine's shared shader buffers.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "Engine/RHI/RenderBufferTypes.h"
#include "Engine/RHI/ShaderBuffer.h"

struct BufferSlice
{
    unsigned int bindingPoint = 0;
    std::size_t offset = 0;
    std::size_t size = 0;
};

class ShaderBufferManager
{
public:
    void BeginFrame();
    void InitializeUniformBuffer(BufferBindingSlot slot, std::size_t size);
    void InitializeUniformRingBuffer(BufferBindingSlot slot, std::size_t elementSize, std::size_t elementCapacity);

    template <typename T>
    void UploadUniform(BufferBindingSlot slot, const T& data)
    {
        // Resize on demand so callers do not need to pre-negotiate exact block sizes.
        const std::size_t slotIndex = static_cast<std::size_t>(slot);
        if (!m_Initialized[slotIndex] || sizeof(T) > m_Sizes[slotIndex])
        {
            InitializeUniformBuffer(slot, sizeof(T));
        }

        m_Buffers[slotIndex].SetData(&data, sizeof(T));
    }

    template <typename T>
    BufferSlice UploadUniformRing(BufferBindingSlot slot, const T& data)
    {
        return UploadUniformRing(slot, data, sizeof(T));
    }

    // Partial-upload overload for blocks the caller only fills a prefix of, such as the
    // instance matrix array: a batch with 3 instances writes 3 mat4 but the block is
    // declared as mat4[128], so transferring the whole struct moves 8 KiB to push 192
    // useful bytes.
    //
    // The reserved element and the returned slice still span the full sizeof(T). GL
    // requires a bound uniform range to cover the block size declared in the shader,
    // and keeping the stride constant is what keeps `offset` aligned to
    // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT. Only the transfer shrinks; the untouched tail
    // of the element is never read, because the shader indexes it by gl_InstanceID.
    template <typename T>
    BufferSlice UploadUniformRing(BufferBindingSlot slot, const T& data, std::size_t uploadSize)
    {
        const std::size_t slotIndex = static_cast<std::size_t>(slot);
        if (!m_Initialized[slotIndex] || !m_IsRingBuffer[slotIndex] || sizeof(T) > m_ElementStrides[slotIndex])
        {
            InitializeUniformRingBuffer(slot, sizeof(T), kRingElementCapacity);
        }

        if (m_ElementCursors[slotIndex] + m_ElementStrides[slotIndex] > m_Sizes[slotIndex])
        {
            m_ElementCursors[slotIndex] = 0;
        }

        const std::size_t offset = m_ElementCursors[slotIndex];
        const std::size_t clampedUploadSize = std::min<std::size_t>(uploadSize, sizeof(T));
        if (clampedUploadSize > 0)
        {
            m_Buffers[slotIndex].SetData(&data, clampedUploadSize, offset);
        }
        m_ElementCursors[slotIndex] += m_ElementStrides[slotIndex];

        return BufferSlice{
            GetBindingPoint(slot),
            offset,
            sizeof(T)
        };
    }

    void Bind(BufferBindingSlot slot) const;
    void BindRange(BufferBindingSlot slot, std::size_t offset, std::size_t size) const;
    unsigned int GetBindingPoint(BufferBindingSlot slot) const;

private:
    static constexpr std::size_t kSlotCount = 4;
    static constexpr std::size_t kRingElementCapacity = 4096;
    void EnsureUniformAlignment();
    std::size_t AlignUniformSize(std::size_t size) const;

    std::array<ShaderBuffer, kSlotCount> m_Buffers;
    std::array<std::size_t, kSlotCount> m_Sizes{};
    std::array<std::size_t, kSlotCount> m_ElementStrides{};
    std::array<std::size_t, kSlotCount> m_ElementCursors{};
    std::array<bool, kSlotCount> m_Initialized{};
    std::array<bool, kSlotCount> m_IsRingBuffer{};
    std::size_t m_UniformOffsetAlignment = 256;
    bool m_HasAlignment = false;
};
