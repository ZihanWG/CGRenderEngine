// Central binding-point registry for the engine's shared shader buffers.
#pragma once

#include <array>
#include <cstddef>

#include "Renderer/RenderBufferTypes.h"
#include "Renderer/ShaderBuffer.h"

class ShaderBufferManager
{
public:
    void InitializeUniformBuffer(BufferBindingSlot slot, std::size_t size);

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

    void Bind(BufferBindingSlot slot) const;
    unsigned int GetBindingPoint(BufferBindingSlot slot) const;

private:
    static constexpr std::size_t kSlotCount = 3;

    std::array<ShaderBuffer, kSlotCount> m_Buffers;
    std::array<std::size_t, kSlotCount> m_Sizes{};
    std::array<bool, kSlotCount> m_Initialized{};
};
