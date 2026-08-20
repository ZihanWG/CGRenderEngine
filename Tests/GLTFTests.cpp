#include "Tests/TestSupport.h"

#include <filesystem>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "Engine/Assets/GLTFLoader.h"

int main()
{
    TestContext test;
    GLTFLoader loader;
    std::string message;

    const std::filesystem::path fixtureRoot(CGENGINE_TEST_FIXTURE_DIR);
    const std::shared_ptr<DecodedSceneModel> model = loader.DecodeModel(
        (fixtureRoot / "alpha_mask_normalized_uv.gltf").string(),
        &message
    );

    EXPECT(test, model != nullptr);
    if (model)
    {
        EXPECT(test, model->objects.size() == 1);
        if (!model->objects.empty())
        {
            const DecodedRenderObject& object = model->objects.front();
            EXPECT(test, object.vertices.size() == 3);
            EXPECT(test, object.indices.size() == 3);
            EXPECT(test, object.indices[0] == 0 && object.indices[1] == 1 && object.indices[2] == 2);
            EXPECT(test, NearlyEqual(object.vertices[1].texCoord.x, 1.0f));
            EXPECT(test, NearlyEqual(object.vertices[2].texCoord.y, 1.0f));
            EXPECT(test, NearlyEqual(glm::length(object.vertices[0].normal), 1.0f));
            EXPECT(test, NearlyEqual(object.material.opacity, 0.8f));
            EXPECT(test, NearlyEqual(object.material.alphaCutoff, 0.25f));
            EXPECT(test, object.material.castShadows);
            EXPECT(test, object.material.cullMode == MaterialCullMode::None);
        }
    }

    message.clear();
    const std::shared_ptr<DecodedSceneModel> invalidModel = loader.DecodeModel(
        (fixtureRoot / "unsupported_lines.gltf").string(),
        &message
    );
    EXPECT(test, invalidModel == nullptr);
    EXPECT(test, message.find("triangle-list") != std::string::npos);

    return test.Finish("GLTFTests");
}

