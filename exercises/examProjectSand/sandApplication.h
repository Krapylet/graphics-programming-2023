#pragma once

#include <ituGL/application/Application.h>

#include <ituGL/scene/Scene.h>
#include <ituGL/scene/SceneModel.h>
#include <ituGL/texture/FramebufferObject.h>
#include <ituGL/renderer/Renderer.h>
#include <ituGL/camera/CameraController.h>
#include <ituGL/utils/DearImGui.h>
#include <array>
#include <ituGL/asset/ModelLoader.h>

class Texture2DObject;
class TextureCubemapObject;
class Material;
class Light;

class SandApplication : public Application
{
public:
    SandApplication();

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
    std::shared_ptr<SceneModel> AddProp(const char* objectName, const char* modelPath, ModelLoader loader);
    std::shared_ptr<Material> GeneratePropMaterial();

    std::shared_ptr<Material> CreatePostFXMaterial(const char* fragmentShaderPath, std::shared_ptr<Texture2DObject> sourceTexture = nullptr);

    Renderer::UpdateTransformsFunction GetFullscreenTransformFunction(std::shared_ptr<ShaderProgram> shaderProgramPtr) const;

    void RenderGUI();

    void MakeCameraFollowPlayer();
    void HandlePlayerMovement();

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
    std::shared_ptr<Material> m_deferredMaterial;
    std::shared_ptr<Material> m_shadowMapMaterial;
    std::shared_ptr<Material> m_composeMaterial;
    std::shared_ptr<Material> m_bloomMaterial;

    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> m_materialsWithUniqueShadows;
    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> m_uniqueShadowMaterials;

    std::shared_ptr<Material> m_desertSandMaterial;
    std::shared_ptr<Material> m_desertSandShadowMaterial;

    std::shared_ptr<Material> m_driveOnSandMaterial;
    std::shared_ptr<Material> m_driveOnSandShadowMaterial;

    // Decoration Object materials
    // The shader program is the same as the driveOnSand material, the uniform properties are assigned different textures
    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> m_propMaterials;
    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> m_propShadowMaterials;
    
    // Prop stuff
    std::shared_ptr<std::vector<std::shared_ptr<SceneModel>>> m_propModels;


    // Framebuffers
    std::shared_ptr<FramebufferObject> m_sceneFramebuffer;
    std::shared_ptr<Texture2DObject> m_depthTexture;
    std::shared_ptr<Texture2DObject> m_sceneTexture;
    std::array<std::shared_ptr<FramebufferObject>, 2> m_tempFramebuffers;
    std::array<std::shared_ptr<Texture2DObject>, 2> m_tempTextures;

    // Player stuff
    std::shared_ptr<SceneModel> m_visualPlayerModel;
    std::shared_ptr<SceneModel> m_parentModel;

    float m_cameraPlayerDistance = 5.0f;
    float m_playerSpeed = 100.0f;
    float m_playerAngularSpeed = 8.0f;

    // Desert stuff.
    std::shared_ptr<SceneModel> m_desertModel;
    std::shared_ptr<Texture2DObject> m_displacementMap;
    float m_tileSize = 10;
    float m_sampleDistance = 0.2f;
    float m_offsetStength = 2.0f;
    float m_enableFog = 0.0f;
    float m_desertWidth = 100;
    float m_desertLength = 100;
    float m_desertVertexRows = 100;
    float m_desertVertexCollumns = 100;



    // debug values
    float u;
    float v;

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
