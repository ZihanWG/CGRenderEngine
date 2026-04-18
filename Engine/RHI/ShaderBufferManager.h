// Central binding-point registry for the engine's shared shader buffers.
#pragma once

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
        const std::size_t slotIndex = static_cast<std::size_t>(slot);
        if (!m_Initialized[slotIndex] || !m_IsRingBuffer[slotIndex] || sizeof(T) > m_ElementStrides[slotIndex])
        {
            InitializeUniformRingBuffer(slot, sizeof(T), 4096);
        }

        if (m_ElementCursors[slotIndex] + m_ElementStrides[slotIndex] > m_Sizes[slotIndex])
        {
            m_ElementCursors[slotIndex] = 0;
        }

        const std::size_t offset = m_ElementCursors[slotIndex];
        m_Buffers[slotIndex].SetData(&data, sizeof(T), offset);
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
