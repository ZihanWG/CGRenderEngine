#include "Tests/TestSupport.h"

#include <type_traits>
#include <utility>
#include <vector>

#include "Engine/RHI/Framebuffer.h"
#include "Engine/RHI/Texture2D.h"

// No GL context exists in this executable and glad is never loaded, so every GL entry
// point is a null function pointer. Any GL call made by the code below crashes the test.
// That is the point: these wrappers must not touch GL until they are first used, so they
// can be declared as members of objects built before the context, and stored in vectors.
int main()
{
    TestContext test;

    static_assert(!std::is_copy_constructible_v<Texture2D>, "Texture2D owns a GL name");
    static_assert(std::is_nothrow_move_constructible_v<Texture2D>, "Texture2D must be movable");
    static_assert(std::is_nothrow_move_assignable_v<Texture2D>, "Texture2D must be movable");
    static_assert(!std::is_copy_constructible_v<Framebuffer>, "Framebuffer owns a GL name");
    static_assert(std::is_nothrow_move_constructible_v<Framebuffer>, "Framebuffer must be movable");
    static_assert(std::is_nothrow_move_assignable_v<Framebuffer>, "Framebuffer must be movable");

    {
        Texture2D texture;
        EXPECT(test, texture.GetID() == 0);
        EXPECT(test, texture.GetWidth() == 0);

        Texture2D moved(std::move(texture));
        EXPECT(test, moved.GetID() == 0);

        Texture2D assigned;
        assigned = std::move(moved);
        EXPECT(test, assigned.GetID() == 0);
    }

    {
        Framebuffer framebuffer;
        Framebuffer moved(std::move(framebuffer));
        Framebuffer assigned;
        assigned = std::move(moved);
    }

    {
        // Growing a vector move-constructs every element into new storage.
        std::vector<Texture2D> textures(3);
        textures.emplace_back();
        std::vector<Framebuffer> framebuffers(2);
        framebuffers.emplace_back();
        EXPECT(test, textures.size() == 4);
        EXPECT(test, framebuffers.size() == 3);
    }

    return test.Finish("rhi");
}
