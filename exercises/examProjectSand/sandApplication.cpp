#include "sandApplication.h"

#include <ituGL/asset/TextureCubemapLoader.h>
#include <ituGL/asset/ShaderLoader.h>
#include <ituGL/asset/ModelLoader.h>

#include <ituGL/camera/Camera.h>
#include <ituGL/scene/SceneCamera.h>

#include <ituGL/lighting/DirectionalLight.h>
#include <ituGL/lighting/PointLight.h>
#include <ituGL/scene/SceneLight.h>

#include <ituGL/shader/ShaderUniformCollection.h>
#include <ituGL/shader/Material.h>
#include <ituGL/geometry/Model.h>
#include <ituGL/scene/SceneModel.h>

#include <ituGL/renderer/SkyboxRenderPass.h>
#include <ituGL/renderer/GBufferRenderPass.h>
#include <ituGL/renderer/DeferredRenderPass.h>
#include <ituGL/renderer/ShadowMapRenderPass.h>
#include <ituGL/renderer/PostFXRenderPass.h>
#include <ituGL/scene/RendererSceneVisitor.h>

#include <ituGL/scene/ImGuiSceneVisitor.h>
#include <imgui.h>

// Includes for manual plane generation.
#include <ituGL/scene/Transform.h>


SandApplication::SandApplication()
    : Application(1024, 1024, "Cool Sand shader demo")
    , m_renderer(GetDevice())
    , m_sceneFramebuffer(std::make_shared<FramebufferObject>())
    , m_exposure(1.0f)
    , m_contrast(1.0f)
    , m_hueShift(0.0f)
    , m_saturation(1.0f)
    , m_colorFilter(1.0f)
    , m_blurIterations(1)
    , m_bloomRange(1.0f, 2.0f)
    , m_bloomIntensity(1.0f)
    , m_sampleDistance(0.01f)
{
}

void SandApplication::Initialize()
{
    Application::Initialize();

    // line enables wireframe view. Used for debugging.
    //GetDevice().SetWireframeEnabled(true);
    

    // Initialize DearImGUI
    // Commonly used open source UI framework. The one used in CSG and the like.
    m_imGui.Initialize(GetMainWindow());

    InitializeCamera();
    InitializeLights();

    // Materials are stored as field variables accessable from the the application class.
    InitializeMaterials();
    InitializeModels();
    InitializeRenderer();
}

void SandApplication::Update()
{
    Application::Update();

    // Update camera controller
    //m_cameraController.Update(GetMainWindow(), GetDeltaTime());

    // Move car with WASD
    HandlePlayerMovement();

    // Follow car insterad of free cam
    MakeCameraFollowPlayer();
    
    // Add the scene nodes to the renderer
    RendererSceneVisitor rendererSceneVisitor(m_renderer);
    m_scene.AcceptVisitor(rendererSceneVisitor);
}

// Moves the player transform based in player input. Control with WASD
void SandApplication::HandlePlayerMovement() {
    
    // Take keyboard input
    const Window& window = GetMainWindow();
    // First rotation
    
    float inputAngularSpeed = 0;
    if (window.IsKeyPressed(GLFW_KEY_A))
        inputAngularSpeed += 1.0f;
    if (window.IsKeyPressed(GLFW_KEY_D))
        inputAngularSpeed += -1.0f;
    
    // Then translation
    float inputSpeed = 0;
    if (window.IsKeyPressed(GLFW_KEY_W))
        inputSpeed += 1.0f;
    if (window.IsKeyPressed(GLFW_KEY_S))
        inputSpeed += -0.3f;

    // Multiply result by player parameters
    inputSpeed *= m_playerSpeed;
    inputAngularSpeed *= m_playerAngularSpeed;

    // Double speed if SHIFT is pressed
    if (window.IsKeyPressed(GLFW_KEY_LEFT_SHIFT))
        inputSpeed *= 2.0f;
    

    // Find the local directions for the player objects
    glm::vec3 right, up, forward;
    // Cut off the 4th row and collumn.
    std::shared_ptr<Transform> playerTransform = m_playerModel->GetTransform();
    glm::mat3 transposed = playerTransform->GetTransformMatrix(); // glm::transpose(playerTransform->GetTranslationMatrix());
    
    right = transposed[0];
    up = transposed[1];
    forward = transposed[2];

    // Apply speed over time to the translation
    float delta = GetDeltaTime();
    glm::vec3 translation = playerTransform->GetTranslation();
    glm::vec3 rotation = playerTransform->GetRotation();
    
    translation += forward * inputSpeed * delta;
    rotation += up * inputAngularSpeed * delta;

    // apply the updated translation and rotation to the model.
    playerTransform->SetTranslation(translation);
    playerTransform->SetRotation(rotation);
}

// Makes camera follow the model in m_playerModel in a third person view.
void SandApplication::MakeCameraFollowPlayer() {
    // to make camera follow car: each frame, copy transform, and then rotate a bit around x, and then translate back.
    // First, match camera translation to followed object.
    const float PI = 3.14159265358979;

    std::shared_ptr<Transform> playerTransform = m_playerModel->GetTransform();
    std::shared_ptr<Transform> cameraTransform = m_cameraController.GetCamera()->GetTransform();

    // Then rotate the camera to point mostly the same way as the player model around the y axis.
    // (camera and player rotation is inverted on the x and z axsis, so we have to negate them to match them.)
    glm::vec3 rotation = glm::vec3(0, playerTransform->GetRotation().y, 0);

    // Add pi around y axis so that camera points the right way.
    rotation += glm::vec3(0, PI, 0);

    // Also rotate it slightly around the x axis to point it a bit downards.
    rotation += glm::vec3(-PI / 16, 0, 0);
    cameraTransform->SetRotation(rotation);


    //// then match the camera translation to the camera
    glm::vec3 translation = playerTransform->GetTranslation();

    // To move the camera, we need to know the camera's forward direction.
    glm::vec3 right; glm::vec3 up; glm::vec3 forward;
    std::shared_ptr<SceneCamera> camera = m_cameraController.GetCamera();
    camera->GetCamera()->ExtractVectors(right, up, forward);

    // Now we can move the camera back and a bit up to get the player model in frame
    translation += (forward + up / 3.0f) * m_cameraPlayerDistance;
    cameraTransform->SetTranslation(translation);

    // Lastly, make the actual camera viewport update according to the transform changes.
    camera->MatchCameraToTransform();
}

void SandApplication::Render()
{
    Application::Render();

    GetDevice().Clear(true, Color(0.0f, 0.0f, 0.0f, 1.0f), true, 1.0f);

    // Render the scene
    m_renderer.Render();
    
    // Debug output to check the shadow map
    //m_desertSandMaterial->SetUniformValue("ColorTexture", m_mainLight->GetShadowMap());

    // Render the debug user interface
    RenderGUI();
}

void SandApplication::Cleanup()
{
    // Cleanup DearImGUI
    m_imGui.Cleanup();

    Application::Cleanup();
}

void SandApplication::InitializeCamera()
{
    // Create the main camera
    std::shared_ptr<Camera> camera = std::make_shared<Camera>();
    camera->SetViewMatrix(glm::vec3(-2, 1, -2), glm::vec3(0, 0.5f, 0), glm::vec3(0, 1, 0));
    camera->SetPerspectiveProjectionMatrix(1.0f, 1.0f, 0.1f, 100.0f);

    // Create a scene node for the camera
    std::shared_ptr<SceneCamera> sceneCamera = std::make_shared<SceneCamera>("camera", camera);

    // Add the camera node to the scene
    m_scene.AddSceneNode(sceneCamera);

    // Set the camera scene node to be controlled by the camera controller
    m_cameraController.SetCamera(sceneCamera);
}

void SandApplication::InitializeLights()
{
    // Create a directional light and add it to the scene
    std::shared_ptr<DirectionalLight> directionalLight = std::make_shared<DirectionalLight>();
    directionalLight->SetDirection(glm::vec3(0.0f, -1.0f, -0.3f)); // It will be normalized inside the function
    directionalLight->SetIntensity(3.0f);
    m_scene.AddSceneNode(std::make_shared<SceneLight>("directional light", directionalLight));

    // Create a point light and add it to the scene
    //std::shared_ptr<PointLight> pointLight = std::make_shared<PointLight>();
    //pointLight->SetPosition(glm::vec3(0, 0, 0));
    //pointLight->SetDistanceAttenuation(glm::vec2(5.0f, 10.0f));
    //m_scene.AddSceneNode(std::make_shared<SceneLight>("point light", pointLight));

    m_mainLight = directionalLight;
}

void SandApplication::InitializeMaterials()
{
    std::shared_ptr<Texture2DObject> displacementMap = Texture2DLoader::LoadTextureShared("textures/SandDisplacementMapPOT.png", TextureObject::FormatR, TextureObject::InternalFormatR, true, false, false);

    // Shadow map material
    {
        // Load and build shader
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/renderer/empty.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/renderer/empty.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewProjMatrix");

        // Create material
        m_shadowMapMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
        
        // We get an error if cull is set to None, and front won't let planes cast shadows.
        m_shadowMapMaterial->SetCullMode(Material::CullMode::Back);
    }


    // Shadow map replacement material for the desert sand
    // This allows the desert sand to cast shadows even with modified vertexes.
    {
        // Load and build shader
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/depthMapUtils.glsl");
        vertexShaderPaths.push_back("shaders/normalGenerator.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/renderer/empty.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
        shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");

        // Create material
        m_desertSandShadowReplacementMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

        // Set material uniforms
        // Depth map. Since it's black and white, there's no reason to load more than one channel.
        m_desertSandShadowReplacementMaterial->SetUniformValue("DepthMap", displacementMap);

        // Initial depth parameters
        m_desertSandShadowReplacementMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        m_desertSandShadowReplacementMaterial->SetUniformValue("OffsetStrength", m_offsetStength);
    }

    // G-buffer material
    {
        // Load and build shader
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/default.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/utils.glsl");
        fragmentShaderPaths.push_back("shaders/default.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");

        // Create material
        m_defaultMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
    }

    // Sand material
    {
        // Load and build shader
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/depthMapUtils.glsl");
        vertexShaderPaths.push_back("shaders/normalGenerator.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/utils.glsl");
        fragmentShaderPaths.push_back("shaders/normalGenerator.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");

        // Create material
        m_desertSandMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

        // Set material uniforms
        
        // Color
        m_desertSandMaterial->SetUniformValue("Color", glm::vec3(0.15f, 0.06f, 0.01f));  // Sand ground color

        // Depth map. Since it's black and white, there's no reason to load more than one channel.
        m_desertSandMaterial->SetUniformValue("DepthMap", displacementMap);
        
        // Initial depth parameters
        m_desertSandMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        m_desertSandMaterial->SetUniformValue("OffsetStrength", m_offsetStength);        
    }

    // Flat color material
    {
        // Load and build shader
        // Shader input and output attributes correspond directly to 
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/default.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/utils.glsl");
        fragmentShaderPaths.push_back("shaders/flatColor.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");

        // Create material
        m_flatColorMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
    }

    // Deferred material
    {
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/renderer/deferred.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/utils.glsl");
        fragmentShaderPaths.push_back("shaders/lambert-ggx.glsl");
        fragmentShaderPaths.push_back("shaders/lighting.glsl");
        fragmentShaderPaths.push_back("shaders/renderer/deferred.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("InvViewMatrix");
        filteredUniforms.insert("InvProjMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");
        filteredUniforms.insert("LightIndirect");
        filteredUniforms.insert("LightColor");
        filteredUniforms.insert("LightPosition");
        filteredUniforms.insert("LightDirection");
        filteredUniforms.insert("LightAttenuation");

        // Get transform related uniform locations
        ShaderProgram::Location invViewMatrixLocation = shaderProgramPtr->GetUniformLocation("InvViewMatrix");
        ShaderProgram::Location invProjMatrixLocation = shaderProgramPtr->GetUniformLocation("InvProjMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                if (cameraChanged)
                {
                    shaderProgram.SetUniform(invViewMatrixLocation, glm::inverse(camera.GetViewMatrix()));
                    shaderProgram.SetUniform(invProjMatrixLocation, glm::inverse(camera.GetProjectionMatrix()));
                }
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
            },
            m_renderer.GetDefaultUpdateLightsFunction(*shaderProgramPtr)
                );

        // Create material
        m_deferredMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

        m_deferredMaterial->SetUniformValue("FadeColor", glm::vec3(0.42f, 0.32f, 0.09f));  // Sand Sky color
        m_deferredMaterial->SetUniformValue("EnableFog", m_enableFog);
    }
}

void SandApplication::InitializeModels()
{
    m_skyboxTexture = TextureCubemapLoader::LoadTextureShared("models/skybox/DesertSkybox.hdr", TextureObject::FormatRGB, TextureObject::InternalFormatRGB16F);

    m_skyboxTexture->Bind();
    float maxLod;
    m_skyboxTexture->GetParameter(TextureObject::ParameterFloat::MaxLod, maxLod);
    TextureCubemapObject::Unbind();

    // Set the environment texture on the deferred material
    m_deferredMaterial->SetUniformValue("EnvironmentTexture", m_skyboxTexture);
    m_deferredMaterial->SetUniformValue("EnvironmentMaxLod", maxLod);

    // Configure loader
    ModelLoader loader(m_defaultMaterial);

    // Create a new material copy for each submaterial
    loader.SetCreateMaterials(true);

    // Flip vertically textures loaded by the model loader
    loader.GetTexture2DLoader().SetFlipVertical(true);

    // Link vertex properties to attributes found in the matrial provided to the loader.
    loader.SetMaterialAttribute(VertexAttribute::Semantic::Position, "VertexPosition");
    loader.SetMaterialAttribute(VertexAttribute::Semantic::Normal, "VertexNormal");
    loader.SetMaterialAttribute(VertexAttribute::Semantic::Tangent, "VertexTangent");
    loader.SetMaterialAttribute(VertexAttribute::Semantic::Bitangent, "VertexBitangent");
    loader.SetMaterialAttribute(VertexAttribute::Semantic::TexCoord0, "VertexTexCoord");

    // Link material properties to uniforms
    loader.SetMaterialProperty(ModelLoader::MaterialProperty::DiffuseColor, "Color");
    loader.SetMaterialProperty(ModelLoader::MaterialProperty::DiffuseTexture, "ColorTexture");
    loader.SetMaterialProperty(ModelLoader::MaterialProperty::NormalTexture, "NormalTexture");
    loader.SetMaterialProperty(ModelLoader::MaterialProperty::SpecularTexture, "SpecularTexture");

    // Load models. ALL MODELS NEED UNIQUE NAMES. Otherwise they won't be rendered.
    // The loader probably needs to be configured differntly for each different material we use for an object.
    std::shared_ptr<Model> cannonModel = loader.LoadShared("models/cannon/cannon.obj");
    std::shared_ptr<SceneModel> player =  std::make_shared<SceneModel>("cannon", cannonModel);
    m_scene.AddSceneNode(player);

    m_playerModel = player;

    // Generate ground plane
    std::shared_ptr<Model> planeModel = Model::GeneratePlane(10, 30, 100, 300);
    planeModel->AddMaterial(m_desertSandMaterial);
   
    // plane model to scene
    std::shared_ptr<SceneModel> plane = std::make_shared<SceneModel>("Plane", planeModel);
    m_scene.AddSceneNode(plane);
}

void SandApplication::InitializeFramebuffers()
{
    int width, height;
    GetMainWindow().GetDimensions(width, height);

    // Scene Texture
    m_sceneTexture = std::make_shared<Texture2DObject>();
    m_sceneTexture->Bind();
    m_sceneTexture->SetImage(0, width, height, TextureObject::FormatRGBA, TextureObject::InternalFormat::InternalFormatRGBA16F);
    m_sceneTexture->SetParameter(TextureObject::ParameterEnum::MinFilter, GL_LINEAR);
    m_sceneTexture->SetParameter(TextureObject::ParameterEnum::MagFilter, GL_LINEAR);
    Texture2DObject::Unbind();

    // Scene framebuffer
    m_sceneFramebuffer->Bind();
    m_sceneFramebuffer->SetTexture(FramebufferObject::Target::Draw, FramebufferObject::Attachment::Depth, *m_depthTexture);
    m_sceneFramebuffer->SetTexture(FramebufferObject::Target::Draw, FramebufferObject::Attachment::Color0, *m_sceneTexture);
    m_sceneFramebuffer->SetDrawBuffers(std::array<FramebufferObject::Attachment, 1>({ FramebufferObject::Attachment::Color0 }));
    FramebufferObject::Unbind();

    // Add temp textures and frame buffers
    for (int i = 0; i < m_tempFramebuffers.size(); ++i)
    {
        m_tempTextures[i] = std::make_shared<Texture2DObject>();
        m_tempTextures[i]->Bind();
        m_tempTextures[i]->SetImage(0, width, height, TextureObject::FormatRGBA, TextureObject::InternalFormat::InternalFormatRGBA16F);
        m_tempTextures[i]->SetParameter(TextureObject::ParameterEnum::WrapS, GL_CLAMP_TO_EDGE);
        m_tempTextures[i]->SetParameter(TextureObject::ParameterEnum::WrapT, GL_CLAMP_TO_EDGE);
        m_tempTextures[i]->SetParameter(TextureObject::ParameterEnum::MinFilter, GL_LINEAR);
        m_tempTextures[i]->SetParameter(TextureObject::ParameterEnum::MagFilter, GL_LINEAR);

        m_tempFramebuffers[i] = std::make_shared<FramebufferObject>();
        m_tempFramebuffers[i]->Bind();
        m_tempFramebuffers[i]->SetTexture(FramebufferObject::Target::Draw, FramebufferObject::Attachment::Color0, *m_tempTextures[i]);
        m_tempFramebuffers[i]->SetDrawBuffers(std::array<FramebufferObject::Attachment, 1>({ FramebufferObject::Attachment::Color0 }));
    }
    Texture2DObject::Unbind();
    FramebufferObject::Unbind();
}

void SandApplication::InitializeRenderer()
{
    int width, height;
    GetMainWindow().GetDimensions(width, height);

    // Add shadow map pass
    if (m_mainLight)
    {

        if (!m_mainLight->GetShadowMap())
        {
            m_mainLight->CreateShadowMap(glm::vec2(512, 512));
            m_mainLight->SetShadowBias(0.01f);
        }
        std::unique_ptr<ShadowMapRenderPass> shadowMapRenderPass(std::make_unique<ShadowMapRenderPass>(m_mainLight, m_shadowMapMaterial
            ,m_desertSandMaterial, m_desertSandShadowReplacementMaterial));
        // This volume should follow the player to render high quality shadows only near the player.
        shadowMapRenderPass->SetVolume(glm::vec3(-3.0f * m_mainLight->GetDirection()), glm::vec3(30.0f));
        m_renderer.AddRenderPass(std::move(shadowMapRenderPass));
    }

    // Set up deferred passes
    {
        std::unique_ptr<GBufferRenderPass> gbufferRenderPass(std::make_unique<GBufferRenderPass>(width, height));

        // Set the g-buffer textures as properties of the deferred material
        m_deferredMaterial->SetUniformValue("DepthTexture", gbufferRenderPass->GetDepthTexture());
        m_deferredMaterial->SetUniformValue("AlbedoTexture", gbufferRenderPass->GetAlbedoTexture());
        m_deferredMaterial->SetUniformValue("NormalTexture", gbufferRenderPass->GetNormalTexture());
        m_deferredMaterial->SetUniformValue("OthersTexture", gbufferRenderPass->GetOthersTexture());

        // Get the depth texture from the gbuffer pass - This could be reworked
        m_depthTexture = gbufferRenderPass->GetDepthTexture();

        // Add the render passes
        m_renderer.AddRenderPass(std::move(gbufferRenderPass));
        m_renderer.AddRenderPass(std::make_unique<DeferredRenderPass>(m_deferredMaterial, m_sceneFramebuffer));
    }

    // Initialize the framebuffers and the textures they use
    InitializeFramebuffers();

    // Skybox pass
    m_renderer.AddRenderPass(std::make_unique<SkyboxRenderPass>(m_skyboxTexture));

    // Create a copy pass from m_sceneTexture to the first temporary texture
    std::shared_ptr<Material> copyMaterial = CreatePostFXMaterial("shaders/postfx/copy.frag", m_sceneTexture);
    m_renderer.AddRenderPass(std::make_unique<PostFXRenderPass>(copyMaterial, m_tempFramebuffers[0]));

    // Replace the copy pass with a new bloom pass
    m_bloomMaterial = CreatePostFXMaterial("shaders/postfx/bloom.frag", m_sceneTexture);
    m_bloomMaterial->SetUniformValue("Range", glm::vec2(2.0f, 3.0f));
    m_bloomMaterial->SetUniformValue("Intensity", 1.0f);
    m_renderer.AddRenderPass(std::make_unique<PostFXRenderPass>(m_bloomMaterial, m_tempFramebuffers[0]));

    // Add blur passes
    std::shared_ptr<Material> blurHorizontalMaterial = CreatePostFXMaterial("shaders/postfx/blur.frag", m_tempTextures[0]);
    blurHorizontalMaterial->SetUniformValue("Scale", glm::vec2(1.0f / width, 0.0f));
    std::shared_ptr<Material> blurVerticalMaterial = CreatePostFXMaterial("shaders/postfx/blur.frag", m_tempTextures[1]);
    blurVerticalMaterial->SetUniformValue("Scale", glm::vec2(0.0f, 1.0f / height));
    for (int i = 0; i < m_blurIterations; ++i)
    {
        m_renderer.AddRenderPass(std::make_unique<PostFXRenderPass>(blurHorizontalMaterial, m_tempFramebuffers[1]));
        m_renderer.AddRenderPass(std::make_unique<PostFXRenderPass>(blurVerticalMaterial, m_tempFramebuffers[0]));
    }

    // Final pass
    m_composeMaterial = CreatePostFXMaterial("shaders/postfx/compose.frag", m_sceneTexture);

    // Set exposure uniform default value
    m_composeMaterial->SetUniformValue("Exposure", m_exposure);

    // Set uniform default values
    m_composeMaterial->SetUniformValue("Contrast", m_contrast);
    m_composeMaterial->SetUniformValue("HueShift", m_hueShift);
    m_composeMaterial->SetUniformValue("Saturation", m_saturation);
    m_composeMaterial->SetUniformValue("ColorFilter", m_colorFilter);

    // Set the bloom texture uniform
    m_composeMaterial->SetUniformValue("BloomTexture", m_tempTextures[0]);

    m_renderer.AddRenderPass(std::make_unique<PostFXRenderPass>(m_composeMaterial, m_renderer.GetDefaultFramebuffer()));
}

std::shared_ptr<Material> SandApplication::CreatePostFXMaterial(const char* fragmentShaderPath, std::shared_ptr<Texture2DObject> sourceTexture)
{
    // We could keep this vertex shader and reuse it, but it looks simpler this way
    std::vector<const char*> vertexShaderPaths;
    vertexShaderPaths.push_back("shaders/version330.glsl");
    vertexShaderPaths.push_back("shaders/renderer/fullscreen.vert");
    Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

    std::vector<const char*> fragmentShaderPaths;
    fragmentShaderPaths.push_back("shaders/version330.glsl");
    fragmentShaderPaths.push_back("shaders/utils.glsl");
    fragmentShaderPaths.push_back(fragmentShaderPath);
    Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

    std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
    shaderProgramPtr->Build(vertexShader, fragmentShader);

    // Create material
    std::shared_ptr<Material> material = std::make_shared<Material>(shaderProgramPtr);
    material->SetUniformValue("SourceTexture", sourceTexture);

    return material;
}

Renderer::UpdateTransformsFunction SandApplication::GetFullscreenTransformFunction(std::shared_ptr<ShaderProgram> shaderProgramPtr) const
{
    // Get transform related uniform locations
    ShaderProgram::Location invViewMatrixLocation = shaderProgramPtr->GetUniformLocation("InvViewMatrix");
    ShaderProgram::Location invProjMatrixLocation = shaderProgramPtr->GetUniformLocation("InvProjMatrix");
    ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");

    // Return transform function
    return [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
    {
        if (cameraChanged)
        {
            shaderProgram.SetUniform(invViewMatrixLocation, glm::inverse(camera.GetViewMatrix()));
            shaderProgram.SetUniform(invProjMatrixLocation, glm::inverse(camera.GetProjectionMatrix()));
        }
        shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
    };
}

void SandApplication::RenderGUI()
{
    m_imGui.BeginFrame();

    // Draw GUI for scene nodes, using the visitor pattern
    ImGuiSceneVisitor imGuiVisitor(m_imGui, "Scene");
    m_scene.AcceptVisitor(imGuiVisitor);

    // Draw GUI for camera controller
    m_cameraController.DrawGUI(m_imGui);

    if (auto window = m_imGui.UseWindow("Shader Uniforms"))
    {
        if (ImGui::DragFloat("Sample distance", &m_sampleDistance, 0.0f, 0.0001f, 0.1f))
        {
            m_desertSandMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        }

        if (ImGui::DragFloat("Offset strength", &m_offsetStength, 0.0f, 0.1f, 10.0f))
        {
            m_desertSandMaterial->SetUniformValue("OffsetStrength", m_offsetStength);
        }

        if (ImGui::DragFloat("EnableFog", &m_enableFog, 0.1f, 0, 1)) {
            m_deferredMaterial->SetUniformValue("EnableFog", m_enableFog);
        }
    }
    

    if (auto window = m_imGui.UseWindow("Player parameters"))
    {
        ImGui::DragFloat("Camera distance", &m_cameraPlayerDistance, 1.0f, 2.0f, 10.0f);
        ImGui::DragFloat("Speed", &m_playerSpeed, 1.0f, 1.0f, 10.0f);
        ImGui::DragFloat("AngularSpeed", &m_playerAngularSpeed, 0.2f, 0.1f, 4.0f);
    }


    if (auto window = m_imGui.UseWindow("Post FX"))
    {
        if (m_composeMaterial)
        {
            if (ImGui::DragFloat("Exposure", &m_exposure, 0.01f, 0.01f, 5.0f))
            {
                m_composeMaterial->SetUniformValue("Exposure", m_exposure);
            }

            ImGui::Separator();

            if (ImGui::SliderFloat("Contrast", &m_contrast, 0.5f, 1.5f))
            {
                m_composeMaterial->SetUniformValue("Contrast", m_contrast);
            }
            if (ImGui::SliderFloat("Hue Shift", &m_hueShift, -0.5f, 0.5f))
            {
                m_composeMaterial->SetUniformValue("HueShift", m_hueShift);
            }
            if (ImGui::SliderFloat("Saturation", &m_saturation, 0.0f, 2.0f))
            {
                m_composeMaterial->SetUniformValue("Saturation", m_saturation);
            }
            if (ImGui::ColorEdit3("Color Filter", &m_colorFilter[0]))
            {
                m_composeMaterial->SetUniformValue("ColorFilter", m_colorFilter);
            }

            ImGui::Separator();

            if (ImGui::DragFloat2("Bloom Range", &m_bloomRange[0], 0.1f, 0.1f, 10.0f))
            {
                m_bloomMaterial->SetUniformValue("Range", m_bloomRange);
            }
            if (ImGui::DragFloat("Bloom Intensity", &m_bloomIntensity, 0.1f, 0.0f, 5.0f))
            {
                m_bloomMaterial->SetUniformValue("Intensity", m_bloomIntensity);
            }
        }
    }

    m_imGui.EndFrame();
}
