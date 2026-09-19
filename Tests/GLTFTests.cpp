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

    // A successful decode must not write anything into errorMessage. Parser warnings used to
    // land there, so callers treating a non-empty message as failure saw phantom errors.
    EXPECT(test, message.empty());

    // The fixture above happens to parse cleanly, so it cannot prove warnings stay out of
    // errorMessage. This one makes tinygltf warn -- it references an image file that does not
    // exist, which tinygltf treats as non-fatal -- while the document still decodes. That is
    // exactly the case the old code mishandled: the decode succeeded but the caller was
    // handed a non-empty error string and had to guess whether it meant failure.
    std::string untouchedMessage = "sentinel";
    const std::shared_ptr<DecodedSceneModel> warningModel = loader.DecodeModel(
        (fixtureRoot / "warning_missing_image.gltf").string(),
        &untouchedMessage
    );
    EXPECT(test, warningModel != nullptr);
    EXPECT(test, untouchedMessage == "sentinel");
    if (warningModel)
    {
        EXPECT(test, warningModel->warnings.find("image") != std::string::npos);
    }

    message.clear();
    const std::shared_ptr<DecodedSceneModel> invalidModel = loader.DecodeModel(
        (fixtureRoot / "unsupported_lines.gltf").string(),
        &message
    );
    EXPECT(test, invalidModel == nullptr);
    EXPECT(test, message.find("triangle-list") != std::string::npos);

    // Unsupported glTF features must produce an actionable decode error rather than a
    // silently wrong static import.
    message.clear();
    const std::shared_ptr<DecodedSceneModel> skinnedModel = loader.DecodeModel(
        (fixtureRoot / "unsupported_skin.gltf").string(),
        &message
    );
    EXPECT(test, skinnedModel == nullptr);
    EXPECT(test, message.find("skinning is not supported") != std::string::npos);

    message.clear();
    const std::shared_ptr<DecodedSceneModel> animatedModel = loader.DecodeModel(
        (fixtureRoot / "unsupported_animation.gltf").string(),
        &message
    );
    EXPECT(test, animatedModel == nullptr);
    EXPECT(test, message.find("animation is not supported") != std::string::npos);

    message.clear();
    const std::shared_ptr<DecodedSceneModel> morphedModel = loader.DecodeModel(
        (fixtureRoot / "unsupported_morph_targets.gltf").string(),
        &message
    );
    EXPECT(test, morphedModel == nullptr);
    EXPECT(test, message.find("morph target") != std::string::npos);

    return test.Finish("GLTFTests");
}

