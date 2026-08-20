#include "Tests/TestSupport.h"

#include <glm/glm.hpp>

#include "Engine/Scene/Material.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Transform.h"

int main()
{
    TestContext test;

    Transform transform;
    transform.position = glm::vec3(2.0f, 3.0f, 4.0f);
    transform.scale = glm::vec3(2.0f);
    const glm::mat4 matrix = transform.GetMatrix();
    EXPECT(test, NearlyEqual(matrix[3].x, 2.0f));
    EXPECT(test, NearlyEqual(matrix[3].y, 3.0f));
    EXPECT(test, NearlyEqual(matrix[3].z, 4.0f));

    Scene scene;
    const std::size_t initialVersion = scene.GetContentVersion();
    RenderObject object;
    object.name = "test";
    scene.AddObject(object);
    EXPECT(test, scene.GetObjects().size() == 1);
    EXPECT(test, scene.GetContentVersion() > initialVersion);

    ImageTexture image;
    image.width = 2;
    image.height = 2;
    image.channels = 4;
    image.pixels = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 255, 255
    };
    const glm::vec4 center = image.Sample(glm::vec2(0.5f));
    EXPECT(test, NearlyEqual(center.r, 0.5f, 0.01f));
    EXPECT(test, NearlyEqual(center.g, 0.5f, 0.01f));
    EXPECT(test, NearlyEqual(center.b, 0.5f, 0.01f));
    EXPECT(test, NearlyEqual(center.a, 1.0f, 0.01f));

    return test.Finish("SceneTests");
}

