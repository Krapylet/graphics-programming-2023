#pragma once

#include <ituGL/application/Application.h>

#include <ituGL/scene/Scene.h>
#include <ituGL/texture/FramebufferObject.h>
#include <ituGL/renderer/Renderer.h>
#include <ituGL/camera/CameraController.h>
#include <ituGL/utils/DearImGui.h>
#include <array>

#include <ituGL/scene/SceneModel.h>
#include <ituGL/asset/ModelLoader.h>

class Texture2DObject;
class TextureCubemapObject;
class Material;
class Light;

class PostFXSceneViewerApplication : public Application
{
public:
    PostFXSceneViewerApplication();

protected:
    void Initialize() override;
    void Update() override;
    void Render() override;
    void Cleanup() override;

private:
    void InitializeCamera();
    void InitializeLights();
    void InitializeMaterials();
    void InitializeModels();
    void InitializeFramebuffers();
    void InitializeRenderer();

    std::shared_ptr<Material> CreatePostFXMaterial(const char* fragmentShaderPath, std::shared_ptr<Texture2DObject> sourceTexture = nullptr);

    Renderer::UpdateTransformsFunction GetFullscreenTransformFunction(std::shared_ptr<ShaderProgram> shaderProgramPtr) const;

    std::shared_ptr<Material> GeneratePropMaterial(int propIndex);
    std::shared_ptr<SceneModel> SpawnProp(ModelLoader loader, const char* objectName, const char* modelPath);

    void HandlePlayerMovement();
    void MakeCameraFollowPlayer();

    void RenderGUI();

private:
    // Helper object for debug GUI
    DearImGui m_imGui;

    // Camera controller
    CameraController m_cameraController;

    // Global scene
    Scene m_scene;

    // Renderer
    Renderer m_renderer;

    // Skybox texture
    std::shared_ptr<TextureCubemapObject> m_skyboxTexture;

    // Main light
    std::shared_ptr<Light> m_mainLight;

    // Materials
    std::shared_ptr<Material> m_defaultMaterial;
    std::shared_ptr<Material> m_driveOnSandMateral;
    std::shared_ptr<Material> m_deferredMaterial;
    std::shared_ptr<Material> m_shadowMapMaterial;
    std::shared_ptr<Material> m_composeMaterial;
    std::shared_ptr<Material> m_bloomMaterial;
    std::shared_ptr<Material> m_desertSandMaterial;

    // Named models
    std::shared_ptr<SceneModel> m_parentModel;
    std::shared_ptr<SceneModel> m_visualPlayerModel;
    std::shared_ptr<SceneModel> m_desertModel;
    
    // Prop models and materials
    std::shared_ptr<std::vector<std::shared_ptr<SceneModel>>> m_propModels;
    std::shared_ptr<std::vector<std::shared_ptr<Material>>> m_propMaterials;

    // Player control parameters
    float m_playerSpeed = 10;
    float m_playerAngularSpeed = 4;
    float m_cameraDistance = 5;

    // debug values for materials.
    float m_ambientOcclusion = 1;
    float m_metalness = 0.35f;
    float m_roughness = 0;
    float m_unused = 0;
    float m_sampleDistance = 0;
    float m_offsetStrength = 0;

    // Light values
    glm::vec3 m_lightDirection = glm::vec3(0.0f, -1.0f, -3.14f);

    // Desert values
    float m_tileSize = 10;
    float m_desertWidth = 10;
    float m_desertLength = 10;
    float m_desertVertexRows = 100;
    float m_desertVertexCollumns = 100;
    float m_noiseStrength = 0.15f;
    float m_noiseTilefrequency = 2;

    // Framebuffers
    std::shared_ptr<FramebufferObject> m_sceneFramebuffer;
    std::shared_ptr<Texture2DObject> m_depthTexture;
    std::shared_ptr<Texture2DObject> m_sceneTexture;
    std::array<std::shared_ptr<FramebufferObject>, 2> m_tempFramebuffers;
    std::array<std::shared_ptr<Texture2DObject>, 2> m_tempTextures;

    // Configuration values
    float m_exposure;
    float m_contrast;
    float m_hueShift;
    float m_saturation;
    glm::vec3 m_colorFilter;
    int m_blurIterations;
    glm::vec2 m_bloomRange;
    float m_bloomIntensity;
};
