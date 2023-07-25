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
#include <map>

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
    void AttemptToPrependNewPlayerPosition();

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

    // Contains materias that need unique shadows as keys, and materials providing those shadows as values
    std::shared_ptr<std::vector<std::pair<std::shared_ptr<const Material>, std::shared_ptr<const Material>>>>  m_shadowReplacements;

    // Named models
    std::shared_ptr<SceneModel> m_parentModel;
    std::shared_ptr<SceneModel> m_visualPlayerModel;
    std::shared_ptr<SceneModel> m_desertModel;
    
    // Prop models and materials
    std::shared_ptr<std::vector<std::shared_ptr<SceneModel>>> m_propModels;
    std::shared_ptr<std::vector<std::shared_ptr<Material>>> m_propMaterials;

    // Depth map that is shared across multiple different shaders
    std::shared_ptr<Texture2DObject> m_displacementMap;

    // Player parameters
    float m_playerCurrentSpeed = 0;
    float m_playerMaxSpeed = 10;
    float m_playerAcceleration = 1;
    float m_playerAngularSpeed = 4;
    float m_playerFriction = 0.1f;
    float m_lastPosSampleTimestamp = 0;
    float m_playerPosSampleFrequency = 2; // how many times pr. second to sample player position.
    int m_playerPosSampleCount = 24; // How many samples to keep at a time. NEEDS TO MATCH ARRAY SIZE IN NORMALGENERATOR.VERT!
    std::shared_ptr<std::vector<glm::vec3>> m_playerPositions;

    // camera parameters
    bool m_freeCamEnabled = false;
    float m_cameraBaseDistance = 10;
    float m_cameraExtraDistance = 10;
    float m_cameraDepth = 4;
    float m_cameraFarPlane = 100.0f;
    float m_cameraFov = 1.0f;

    // Light values
    glm::vec3 m_lightDirection = glm::vec3(0.0f, -1.0f, -2.14f);
    glm::vec3 m_fogColor = glm::vec3(1, 0.8f, 0.43f);
    glm::vec3 m_specularColor = glm::vec3(0.28f, 0.06f, 0);
    float m_fogStrength = 0.06;
    float m_fogDistance = -0.06f;

    // Desert values
    glm::vec3 m_desertColor = glm::vec3(0.8, 0.4, 0.2);
    float m_offsetStrength = 8;
    float m_sampleDistance = 0.01;

    float m_waveWidth = 0.4f;
    float m_waveStength = 0.5f;

    float m_ambientOcclusion = 1;
    float m_metalness = 0.35f;
    float m_roughness = 0;

    glm::vec2 m_tileFrequency = glm::vec2(10, 60);
    glm::vec2 m_noiseTilefrequency = glm::vec2(2, 12);
    float m_noiseStrength = 0.15f;

    float m_desertWidth = 600;
    float m_desertLength = 100;
    int m_desertVertexCollumns = 600;
    int m_desertVertexRows = 100;

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
