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

// Owns the engine's shared uniform buffers and hands out per-draw slices of them.
//
// A ring slot is split into kFrameSliceCount equally sized frame regions. Frame N only
// ever writes into region N % kFrameSliceCount, and BeginFrame blocks on a fence proving
// the GPU has finished the last frame that used that region. That is what makes resetting
// the write cursor every frame safe: without it the CPU hands glBufferSubData bytes the
// GPU may still be reading, and the driver has to stall or rename the buffer on every
// upload to keep the result correct.
class ShaderBufferManager
{
public:
    // Three regions covers the usual "GPU is at most a frame or two behind" case while
    // keeping the allocation small. Raising it trades memory for a looser sync window.
    static constexpr std::size_t kFrameSliceCount = 3;
    // Starting size only. A slot whose frame needs more elements than this grows at the
    // next frame boundary, so callers never have to guess a worst-case draw count.
    static constexpr std::size_t kInitialElementsPerFrame = 64;

    ShaderBufferManager() = default;
    ~ShaderBufferManager();

    ShaderBufferManager(const ShaderBufferManager&) = delete;
    ShaderBufferManager& operator=(const ShaderBufferManager&) = delete;

    // Rotates to the next frame region, waits for the GPU to release it, and applies any
    // growth the previous frame asked for.
    void BeginFrame();
    // Fences this frame's region so a later BeginFrame knows when it may be reused.
    void EndFrame();

    void InitializeUniformBuffer(BufferBindingSlot slot, std::size_t size);
    void InitializeUniformRingBuffer(
        BufferBindingSlot slot,
        std::size_t elementSize,
        std::size_t elementsPerFrame = kInitialElementsPerFrame
    );

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
            InitializeUniformRingBuffer(slot, sizeof(T));
        }

        if (m_ElementCursors[slotIndex] + m_ElementStrides[slotIndex] > RingSliceEnd(slotIndex))
        {
            // This frame wants more elements than its region holds. Wrapping keeps the
            // frame correct -- glBufferSubData serializes against pending reads -- but
            // costs a stall, and the element count below tells BeginFrame to grow so the
            // next frame does not pay it again.
            m_ElementCursors[slotIndex] = RingSliceBegin(slotIndex);
        }

        const std::size_t offset = m_ElementCursors[slotIndex];
        const std::size_t clampedUploadSize = std::min<std::size_t>(uploadSize, sizeof(T));
        if (clampedUploadSize > 0)
        {
            m_Buffers[slotIndex].SetData(&data, clampedUploadSize, offset);
        }
        m_ElementCursors[slotIndex] += m_ElementStrides[slotIndex];
        ++m_FrameElementCounts[slotIndex];

        return BufferSlice{
            GetBindingPoint(slot),
            offset,
            sizeof(T)
        };
    }

    void Bind(BufferBindingSlot slot) const;
    void BindRange(BufferBindingSlot slot, std::size_t offset, std::size_t size) const;
    unsigned int GetBindingPoint(BufferBindingSlot slot) const;

    // Total bytes currently reserved for a slot, across every frame region. Exposed for
    // tooling and tests that track ring growth.
    std::size_t GetAllocatedSize(BufferBindingSlot slot) const;
    std::size_t GetElementsPerFrame(BufferBindingSlot slot) const;

private:
    static constexpr std::size_t kSlotCount = 4;

    void EnsureUniformAlignment();
    std::size_t AlignUniformSize(std::size_t size) const;
    // (Re)allocates a ring slot to hold `elementsPerFrame` elements in each frame region.
    void AllocateRing(std::size_t slotIndex, std::size_t elementsPerFrame);
    void WaitForFrameSlice(std::size_t frameSlice);
    std::size_t RingSliceBegin(std::size_t slotIndex) const;
    std::size_t RingSliceEnd(std::size_t slotIndex) const;

    std::array<ShaderBuffer, kSlotCount> m_Buffers;
    std::array<std::size_t, kSlotCount> m_Sizes{};
    std::array<std::size_t, kSlotCount> m_ElementStrides{};
    std::array<std::size_t, kSlotCount> m_ElementCursors{};
    std::array<std::size_t, kSlotCount> m_ElementsPerFrame{};
    // Elements handed out during the current frame. Exceeding m_ElementsPerFrame is the
    // signal that the slot is undersized.
    std::array<std::size_t, kSlotCount> m_FrameElementCounts{};
    std::array<bool, kSlotCount> m_Initialized{};
    std::array<bool, kSlotCount> m_IsRingBuffer{};
    std::array<GLsync, kFrameSliceCount> m_FrameFences{};
    std::size_t m_FrameSlice = 0;
    std::size_t m_UniformOffsetAlignment = 256;
    bool m_HasAlignment = false;
};
