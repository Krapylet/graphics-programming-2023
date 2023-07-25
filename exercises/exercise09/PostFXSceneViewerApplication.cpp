#include "PostFXSceneViewerApplication.h"

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

#include <ituGL/scene/Transform.h>
#include <span>

PostFXSceneViewerApplication::PostFXSceneViewerApplication()
    : Application(1024, 1024, "Post FX Scene Viewer demo")
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
{
}

void PostFXSceneViewerApplication::Initialize()
{
    Application::Initialize();

    // Initialize DearImGUI
    m_imGui.Initialize(GetMainWindow());

    InitializeCamera();
    InitializeLights();
    InitializeMaterials();
    InitializeModels();
    InitializeRenderer();
}

void PostFXSceneViewerApplication::Update()
{
    Application::Update();


    // Toggle freecam
    if (GetMainWindow().IsKeyPressed(GLFW_KEY_F)) {
        m_freeCamEnabled = !m_freeCamEnabled;
        m_cameraController.SetEnabled(m_freeCamEnabled); //We set this bool so we don't also have to press space to enable the freecam.
    }

    // Update camera controller
    // Old movement method is still worth keeping around for debugging purposes.
    if (m_freeCamEnabled) {
        m_cameraController.Update(GetMainWindow(), GetDeltaTime());
    }
    else {
        // Move player object
        HandlePlayerMovement();
        // make camera follow player object
        MakeCameraFollowPlayer();
    }
        

    // Add the scene nodes to the renderer
    RendererSceneVisitor rendererSceneVisitor(m_renderer);
    m_scene.AcceptVisitor(rendererSceneVisitor);
}

// Moves the player transform based in player input. Control with WASD
void PostFXSceneViewerApplication::HandlePlayerMovement() {

    // Take keyboard input
    const Window& window = GetMainWindow();
    // First rotation

    float inputAngularSpeed = 0;
    if (window.IsKeyPressed(GLFW_KEY_A))
        inputAngularSpeed += 1.0f;
    else if (window.IsKeyPressed(GLFW_KEY_D))
        inputAngularSpeed += -1.0f;

    // Then translation
    float inputSpeed = 0;
    if (window.IsKeyPressed(GLFW_KEY_W))
        inputSpeed += -1.0f;
    else if (window.IsKeyPressed(GLFW_KEY_S))
        inputSpeed += 0.3f;

    // Double speed if SHIFT is pressed
    float speedMultiplier = 1;
    if (window.IsKeyPressed(GLFW_KEY_LEFT_SHIFT))
        speedMultiplier = 2 - (1 - window.IsKeyPressed(GLFW_KEY_W));

    float potentialSpeed = m_playerCurrentSpeed + inputSpeed * m_playerAcceleration;
    potentialSpeed = glm::mix(potentialSpeed, 0.0f, m_playerFriction); // add friction to speed.
    m_playerCurrentSpeed = glm::clamp(potentialSpeed, -m_playerMaxSpeed, m_playerMaxSpeed) * speedMultiplier;

    // Find the local directions for the player objects
    // Once again, the reason we have this parent object, is that it makes rotating the player model and camera together much easier. 
    std::shared_ptr<Transform> parentTransform = m_parentModel->GetTransform();
    glm::mat3 transformMatrix = parentTransform->GetTransformMatrix(); // glm::transpose(parentTransform->GetTranslationMatrix());

    // Keeping unusde directions here so i can remember where they are placed.
    // NB: They are not normalized.
    glm::vec3 right = glm::normalize(transformMatrix[0]);
    glm::vec3 up = glm::normalize(transformMatrix[1]);
    glm::vec3 forward = glm::normalize(transformMatrix[2]);

    // Apply speed over time to the translation
    float delta = GetDeltaTime();
    glm::vec3 translation = parentTransform->GetTranslation();
    glm::vec3 rotation = parentTransform->GetRotation();

    translation += forward * m_playerCurrentSpeed * delta;
    rotation += up * inputAngularSpeed * delta;

    // apply the updated translation and rotation to the parent.
    parentTransform->SetTranslation(translation);
    parentTransform->SetRotation(rotation);

    // Apply move the visual model along with the parnet
    std::shared_ptr<Transform> playerModelTransform = m_visualPlayerModel->GetTransform();
    playerModelTransform->SetTranslation(translation);

    // prepend new player positions at specific intervals and update desert material.
    AttemptToPrependNewPlayerPosition();
}

// prepend the value to the vector, overwriting the last value
void PostFXSceneViewerApplication::AttemptToPrependNewPlayerPosition() {
    float newSampleThreshold = m_lastPosSampleTimestamp + 1 / m_playerPosSampleFrequency;
    
    bool isTooEarlyForNextSample = GetCurrentTime() < newSampleThreshold;
    if (isTooEarlyForNextSample)
        return;

    // Add newest value to the front
    glm::vec3 playerPos = m_parentModel->GetTransform()->GetTranslation();

    // for some reason there's a wierd global offset towards x, which we take care of here.
    playerPos -= glm::vec3(1, 0, 0);  

    // move the logged player position slightly behind the player, so that the waves arent right underneath them.
    glm::mat3 transformMatrix = m_parentModel->GetTransform()->GetTransformMatrix();
    glm::vec3 forward = glm::normalize(transformMatrix[2]);
    playerPos += forward;

    m_playerPositions->insert(m_playerPositions->begin(), playerPos);

    // delete the last value
    m_playerPositions->erase(m_playerPositions->begin() + m_playerPositions->size() - 1);


    // We update desert material here so it only happens whenever they change
    std::span<const glm::vec3> playerPosSpan(*m_playerPositions.get());
    m_desertSandMaterial->SetUniformValues("PlayerPositions", playerPosSpan);
}

// Makes camera follow the model in m_parentModel in a third person view.
void PostFXSceneViewerApplication::MakeCameraFollowPlayer() {
    // to make camera follow car: each frame, copy transform, and then rotate a bit around x, and then translate back.
    // First, match camera translation to followed object.
    const float PI = 3.14159265358979;

    std::shared_ptr<Transform> parentTransform = m_parentModel->GetTransform();
    std::shared_ptr<Transform> cameraTransform = m_cameraController.GetCamera()->GetTransform();

    // Then rotate the camera to point the same way as the player model around the y axis.
    glm::vec3 rotation = glm::vec3(0, parentTransform->GetRotation().y, 0);

    // rotate it slightly around the x axis to point it a bit downards.
    rotation += glm::vec3(-PI / 16, 0, 0);
    cameraTransform->SetRotation(rotation);

    //// then match the camera translation to the camera
    glm::vec3 translation = parentTransform->GetTranslation();

    // To move the camera, we need to know the camera's forward direction.
    glm::vec3 right; glm::vec3 up; glm::vec3 forward;
    std::shared_ptr<SceneCamera> camera = m_cameraController.GetCamera();
    camera->GetCamera()->ExtractVectors(right, up, forward);

    // Now we can move the camera diagonally back
    // The distance we move it is based off of the current speed.
    float speedPercentage = abs(m_playerCurrentSpeed) / m_playerMaxSpeed / 2;
    float easingValue = speedPercentage < 0.5f ? 4 * powf(speedPercentage, 3) : 1 - powf(-2 * speedPercentage + 2, 3) / 2;
    float cameraDistance = m_cameraBaseDistance + m_cameraExtraDistance * easingValue;
    translation += (forward + up / 3.0f) * cameraDistance;
    
    // also move the camera based on the offset of the desert
    translation += up * m_offsetStrength / 2.0f;

    cameraTransform->SetTranslation(translation);

    // Lastly, make the actual camera viewport update according to the transform changes.
    camera->MatchCameraToTransform();
}

void PostFXSceneViewerApplication::Render()
{
    Application::Render();

    GetDevice().Clear(true, Color(0.0f, 0.0f, 0.0f, 1.0f), true, 1.0f);

    // Render the scene
    m_renderer.Render();

    // Render the debug user interface
    RenderGUI();
}

void PostFXSceneViewerApplication::Cleanup()
{
    // Cleanup DearImGUI
    m_imGui.Cleanup();

    Application::Cleanup();
}

void PostFXSceneViewerApplication::InitializeCamera()
{
    // Create the main camera
    std::shared_ptr<Camera> camera = std::make_shared<Camera>();
    camera->SetViewMatrix(glm::vec3(-2, 1, -2), glm::vec3(0, 0.5f, 0), glm::vec3(0, 1, 0));
    camera->SetPerspectiveProjectionMatrix(1.0f, 1.0f, 0.1f, m_cameraFarPlane);

    // Create a scene node for the camera
    std::shared_ptr<SceneCamera> sceneCamera = std::make_shared<SceneCamera>("camera", camera);

    // Add the camera node to the scene
    m_scene.AddSceneNode(sceneCamera);

    // Set the camera scene node to be controlled by the camera controller
    m_cameraController.SetCamera(sceneCamera);
}

void PostFXSceneViewerApplication::InitializeLights()
{
    // Create a directional light and add it to the scene
    std::shared_ptr<DirectionalLight> directionalLight = std::make_shared<DirectionalLight>();
    directionalLight->SetDirection(m_lightDirection); // It will be normalized inside the function
    directionalLight->SetIntensity(3.0f);
//    directionalLight->SetColor(glm::vec3(0, 0, 1));
    m_scene.AddSceneNode(std::make_shared<SceneLight>("directional light", directionalLight));

    // Create a point light and add it to the scene
    //std::shared_ptr<PointLight> pointLight = std::make_shared<PointLight>();
    //pointLight->SetPosition(glm::vec3(0, 0, 0));
    //pointLight->SetDistanceAttenuation(glm::vec2(5.0f, 10.0f));
    //m_scene.AddSceneNode(std::make_shared<SceneLight>("point light", pointLight));

    m_mainLight = directionalLight;
}

void PostFXSceneViewerApplication::InitializeMaterials()
{
    m_propMaterials = std::make_shared<std::vector<std::shared_ptr<Material>>>();

    m_displacementMap = Texture2DLoader::LoadTextureShared("textures/SandDisplacementMapPOT.png", TextureObject::FormatR, TextureObject::InternalFormatR, true, false, false);
    
    m_shadowReplacements = std::make_shared<std::vector<std::pair<std::shared_ptr<const Material>, std::shared_ptr<const Material>>>>();

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
        m_shadowMapMaterial->SetCullMode(Material::CullMode::Front);
    }

    // default G-buffer material
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
        // Use for all "pr. object" values
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");

        // Create material
        // These initial uniforms values are saved even when the material is copied as new instances are generated when a model is loaded.
        m_defaultMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
        m_defaultMaterial->SetUniformValue("Color", glm::vec3(1.0f));
    }

    // Sand deformation shader
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
        m_desertSandMaterial->SetUniformValue("OffsetStrength", m_offsetStrength);
        m_desertSandMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        m_desertSandMaterial->SetUniformValue("AmbientOcclusion", m_ambientOcclusion);
        m_desertSandMaterial->SetUniformValue("Metalness", m_metalness);
        m_desertSandMaterial->SetUniformValue("Roughness", m_roughness);
        m_desertSandMaterial->SetUniformValue("Unused", m_unused);
        m_desertSandMaterial->SetUniformValue("Color", glm::vec3(0.8, 0.4, 0.2));
        m_desertSandMaterial->SetUniformValue("TileSize", 10.0f);
        m_desertSandMaterial->SetUniformValue("NoiseStrength", m_noiseStrength);
        m_desertSandMaterial->SetUniformValue("NoiseTileFrequency", m_noiseTilefrequency);
        m_desertSandMaterial->SetUniformValue("DepthMap", m_displacementMap);
        m_desertSandMaterial->SetUniformValue("WaveWidth", m_waveWidth);
        m_desertSandMaterial->SetUniformValue("WaveStrength", m_waveStength);

        // Car positions are initialized to 0, since the car hasn't driven anywhere yet.
        m_playerPositions = std::make_shared<std::vector<glm::vec3>>(m_playerPosSampleCount, glm::vec3(0,0,0));
        std::span<const glm::vec3> playerPosSpan(*m_playerPositions.get());
        m_desertSandMaterial->SetUniformValues("PlayerPositions", playerPosSpan);
        
        // load textures
        std::shared_ptr<Texture2DObject> noiseTexture = Texture2DLoader::LoadTextureShared("textures/PixelNoise.png", TextureObject::FormatRGB, TextureObject::InternalFormatRGB16);
        m_desertSandMaterial->SetUniformValue("NoiseTexture", noiseTexture);

        std::shared_ptr<Texture2DObject> normalMap = Texture2DLoader::LoadTextureShared("textures/SandNormalMap.png", TextureObject::FormatRGB, TextureObject::InternalFormatRGB16);
        m_desertSandMaterial->SetUniformValue("NormalTexture", normalMap);


        // can maybe take scale into account as well if scale in multiplied in here.
        m_desertSandMaterial->SetUniformValue("ObjectSize", glm::vec2(m_desertLength, m_desertWidth));
        
    }

    // Sand deformation shadow shader
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
        ShaderProgram::Location offsetStrengthLocation = shaderProgramPtr->GetUniformLocation("OffsetStrength");
        ShaderProgram::Location sampleDistanceLocation = shaderProgramPtr->GetUniformLocation("SampleDistance");
        ShaderProgram::Location playerPositionsLocation = shaderProgramPtr->GetUniformLocation("PlayerPositions");
        ShaderProgram::Location waveWidthLocation = shaderProgramPtr->GetUniformLocation("WaveWidth");
        ShaderProgram::Location waveStrengthLocation = shaderProgramPtr->GetUniformLocation("WaveStrength");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
                shaderProgram.SetUniform(offsetStrengthLocation, m_offsetStrength);
                shaderProgram.SetUniform(sampleDistanceLocation, m_sampleDistance);
                shaderProgram.SetUniform(waveWidthLocation, m_waveWidth);
                shaderProgram.SetUniform(waveStrengthLocation, m_waveStength);

                std::span<const glm::vec3> playerPosSpan(*m_playerPositions.get());
                shaderProgram.SetUniforms(playerPositionsLocation, playerPosSpan);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");


        // Create material
        std::shared_ptr<Material> desertSandShadowMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
        desertSandShadowMaterial->SetUniformValue("DepthMap", m_displacementMap);

        // can maybe take scale into account as well if scale in multiplied in here.
        desertSandShadowMaterial->SetUniformValue("ObjectSize", glm::vec2(m_desertLength, m_desertWidth));

        // add shadow to list of replacements
        m_shadowReplacements->push_back(std::pair(m_desertSandMaterial, desertSandShadowMaterial));
    }

    // drive on sand material
    {
        // Load and build shader
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/depthMapUtils.glsl");
        vertexShaderPaths.push_back("shaders/driveOnSand.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/utils.glsl");
        fragmentShaderPaths.push_back("shaders/driveOnSand.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");
        ShaderProgram::Location dersertUVLocation = shaderProgramPtr->GetUniformLocation("DesertUV");
        ShaderProgram::Location rightLocation = shaderProgramPtr->GetUniformLocation("Right");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);

                // Calculate the models position on the desert model in UV coordinates.
                // Will not work if desert is rotated.
                glm::vec3 modelPos = m_parentModel->GetTransform()->GetTranslation();
                glm::vec3 desertPos = m_desertModel->GetTransform()->GetTranslation();
                glm::vec3 desertScale = m_desertModel->GetTransform()->GetScale();
                glm::vec3 desertPosOnDesert = modelPos - desertPos;
                float u = desertPosOnDesert.x / m_desertLength * desertScale.x + 0.5;
                float v = desertPosOnDesert.z / m_desertWidth * desertScale.z + 0.5;
                shaderProgram.SetUniform(dersertUVLocation, glm::vec2(u, v));

                // calculate the model's right
                glm::mat3 modelTransform = m_parentModel->GetTransform()->GetTransformMatrix();
                glm::vec3 right = glm::normalize(modelTransform[0]);
                shaderProgram.SetUniform(rightLocation, right);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        // Use for all "pr. object" values
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");
        filteredUniforms.insert("DesertUV");
        filteredUniforms.insert("Right");

        // Create material
        // These initial uniforms values are saved even when the material is copied as new instances are generated when a model is loaded.
        m_driveOnSandMateral = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
        m_driveOnSandMateral->SetUniformValue("Color", glm::vec3(1.0f));
        m_driveOnSandMateral->SetUniformValue("AmbientOcclusion", m_ambientOcclusion);
        m_driveOnSandMateral->SetUniformValue("Metalness", m_metalness);
        m_driveOnSandMateral->SetUniformValue("Roughness", m_roughness);
        m_driveOnSandMateral->SetUniformValue("Unused", m_unused);

        m_driveOnSandMateral->SetUniformValue("OffsetStrength", m_offsetStrength);
        m_driveOnSandMateral->SetUniformValue("SampleDistance", m_sampleDistance);
        m_driveOnSandMateral->SetUniformValue("DepthMap", m_displacementMap);

        m_driveOnSandMateral->SetUniformValue("DesertSize", glm::vec2(m_desertLength, m_desertWidth));
    }

    // drive on sand shadow material
    {
        // Load and build shader
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/depthMapUtils.glsl");
        vertexShaderPaths.push_back("shaders/driveOnSand.vert");
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
        ShaderProgram::Location dersertUVLocation = shaderProgramPtr->GetUniformLocation("DesertUV");
        ShaderProgram::Location rightLocation = shaderProgramPtr->GetUniformLocation("Right");
        ShaderProgram::Location offsetStrengthLocation = shaderProgramPtr->GetUniformLocation("OffsetStrength");
        ShaderProgram::Location sampleDistanceLocation = shaderProgramPtr->GetUniformLocation("SampleDistance");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);

                // Calculate the models position on the desert model in UV coordinates.
                glm::vec3 modelPos = m_parentModel->GetTransform()->GetTranslation();
                glm::vec3 desertPos = m_desertModel->GetTransform()->GetTranslation();
                glm::vec3 desertScale = m_desertModel->GetTransform()->GetScale();
                glm::vec3 desertPosOnDesert = modelPos - desertPos;
                float u = desertPosOnDesert.x / m_desertLength * desertScale.x + 0.5;
                float v = desertPosOnDesert.z / m_desertWidth * desertScale.z + 0.5;
                shaderProgram.SetUniform(dersertUVLocation, glm::vec2(u, v));

                // calculate the model's right
                glm::mat3 modelTransform = m_parentModel->GetTransform()->GetTransformMatrix();
                glm::vec3 right = glm::normalize(modelTransform[0]);
                shaderProgram.SetUniform(rightLocation, right);


                shaderProgram.SetUniform(offsetStrengthLocation, m_offsetStrength);
                shaderProgram.SetUniform(sampleDistanceLocation, m_sampleDistance);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        // Use for all "pr. object" values
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");
        filteredUniforms.insert("DesertUV");
        filteredUniforms.insert("Right");

        // Create material
        // These initial uniforms values are saved even when the material is copied as new instances are generated when a model is loaded.
        std::shared_ptr<Material> driveOnSandShadowMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
        //driveOnSandShadowMaterial->SetUniformValue("OffsetStrength", m_offsetStrength);
        //driveOnSandShadowMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        driveOnSandShadowMaterial->SetUniformValue("DepthMap", m_displacementMap);
        driveOnSandShadowMaterial->SetUniformValue("DesertSize", glm::vec2(m_desertLength, m_desertWidth));

        m_shadowReplacements->push_back(std::pair(m_driveOnSandMateral, driveOnSandShadowMaterial));
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
        fragmentShaderPaths.push_back("shaders/depthMapUtils.glsl");
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
        ShaderProgram::Location cameraCarDistanceLocation = shaderProgramPtr->GetUniformLocation("CameraCarDistance");

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
                float camDist = glm::length(m_cameraController.GetCamera()->GetTransform()->GetTranslation() - m_parentModel->GetTransform()->GetTranslation());
                shaderProgram.SetUniform(cameraCarDistanceLocation, camDist);
            },
            m_renderer.GetDefaultUpdateLightsFunction(*shaderProgramPtr)
        );

        // Create material
        m_deferredMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
        m_deferredMaterial->SetUniformValue("FogColor", m_fogColor);
        m_deferredMaterial->SetUniformValue("SpecularColor", m_specularColor);
        m_deferredMaterial->SetUniformValue("FogStrength", m_fogStrength);
        m_deferredMaterial->SetUniformValue("CameraFarPlane", m_cameraFarPlane);
        m_deferredMaterial->SetUniformValue("FogDistance", m_fogDistance);
    }
}

// Generates a G-buffer material for a generic prop with acces to the prop's model
// This way props can include stuff like object position in their shader uniforms.
std::shared_ptr<Material> PostFXSceneViewerApplication::GeneratePropMaterial(int propIndex) {
    
    
    // Load and build shader
    std::vector<const char*> vertexShaderPaths;
    vertexShaderPaths.push_back("shaders/version330.glsl");
    vertexShaderPaths.push_back("shaders/depthMapUtils.glsl");
    vertexShaderPaths.push_back("shaders/prop.vert");
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
    ShaderProgram::Location dersertUVLocation = shaderProgramPtr->GetUniformLocation("DesertUV");
    ShaderProgram::Location offsetStrengthLocation = shaderProgramPtr->GetUniformLocation("OffsetStrength");
    ShaderProgram::Location sampleDistanceLocation = shaderProgramPtr->GetUniformLocation("SampleDistance");

    auto transformUpdateFunction = [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
    {
        shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
        shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);

        // Calculate the models position on the desert model in UV coordinates.
        // Will not work if desert is rotated.
        glm::vec3 propPos = m_propModels->at(propIndex)->GetTransform()->GetTranslation();
        glm::vec3 desertPos = m_desertModel->GetTransform()->GetTranslation();
        glm::vec3 desertScale = m_desertModel->GetTransform()->GetScale();
        glm::vec3 desertPosOnDesert = propPos - desertPos;
        float u = desertPosOnDesert.x / m_desertLength * desertScale.x + 0.5;
        float v = desertPosOnDesert.z / m_desertWidth * desertScale.z + 0.5;
        shaderProgram.SetUniform(dersertUVLocation, glm::vec2(u, v));

        shaderProgram.SetUniform(offsetStrengthLocation, m_offsetStrength);
        shaderProgram.SetUniform(sampleDistanceLocation, m_sampleDistance);

    };

    // Register shader with renderer
    m_renderer.RegisterShaderProgram(shaderProgramPtr, transformUpdateFunction, nullptr);

    // Filter out uniforms that are not material properties
    // Use for all "pr. object" values
    ShaderUniformCollection::NameSet filteredUniforms;
    filteredUniforms.insert("WorldViewMatrix");
    filteredUniforms.insert("WorldViewProjMatrix");
    filteredUniforms.insert("DesertUV");

    // Create material
    std::shared_ptr<Material> propMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);
    m_propMaterials->push_back(propMaterial);


    // These initial uniforms values are saved even when the material is copied as new instances are generated when a model is loaded.
    propMaterial->SetUniformValue("Color", glm::vec3(1.0f));
    propMaterial->SetUniformValue("OffsetStrength", m_offsetStrength);
    propMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
    propMaterial->SetUniformValue("DepthMap", m_displacementMap);


    // ---------------- Generate shadow material --------------------
    
     // Load and build shader
    std::vector<const char*> shadowFragmentShaderPaths;
    shadowFragmentShaderPaths.push_back("shaders/version330.glsl");
    shadowFragmentShaderPaths.push_back("shaders/renderer/empty.frag");
    Shader shadowFragmentShader = ShaderLoader(Shader::FragmentShader).Load(shadowFragmentShaderPaths);

    std::shared_ptr<ShaderProgram> shadowShaderProgramPtr = std::make_shared<ShaderProgram>();
    shadowShaderProgramPtr->Build(vertexShader, shadowFragmentShader);

    // Get transform related uniform locations
    ShaderProgram::Location shadowWorldViewMatrixLocation = shadowShaderProgramPtr->GetUniformLocation("WorldViewMatrix");
    ShaderProgram::Location shadowWorldViewProjMatrixLocation = shadowShaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");
    ShaderProgram::Location shadowDersertUVLocation = shadowShaderProgramPtr->GetUniformLocation("DesertUV");
    ShaderProgram::Location shadowOffsetStrengthLocation = shadowShaderProgramPtr->GetUniformLocation("OffsetStrength");
    ShaderProgram::Location shadowSampleDistanceLocation = shadowShaderProgramPtr->GetUniformLocation("SampleDistance");

    auto shadowTransformUpdateFunction = [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
    {
        shaderProgram.SetUniform(shadowWorldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
        shaderProgram.SetUniform(shadowWorldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);

        // Calculate the models position on the desert model in UV coordinates.
        // Will not work if desert is rotated.
        glm::vec3 propPos = m_propModels->at(propIndex)->GetTransform()->GetTranslation();
        glm::vec3 desertPos = m_desertModel->GetTransform()->GetTranslation();
        glm::vec3 desertScale = m_desertModel->GetTransform()->GetScale();
        glm::vec3 desertPosOnDesert = propPos - desertPos;
        float u = desertPosOnDesert.x / m_desertLength * desertScale.x + 0.5;
        float v = desertPosOnDesert.z / m_desertWidth * desertScale.z + 0.5;
        shaderProgram.SetUniform(shadowDersertUVLocation, glm::vec2(u, v));

        shaderProgram.SetUniform(shadowOffsetStrengthLocation, m_offsetStrength);
        shaderProgram.SetUniform(shadowSampleDistanceLocation, m_sampleDistance);

    };

    // Register shader with renderer
    m_renderer.RegisterShaderProgram(shadowShaderProgramPtr, shadowTransformUpdateFunction, nullptr);

    // Create material
    std::shared_ptr<Material> shadowPropMaterial = std::make_shared<Material>(shadowShaderProgramPtr, filteredUniforms);
    shadowPropMaterial->SetUniformValue("OffsetStrength", m_offsetStrength);
    shadowPropMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
    shadowPropMaterial->SetUniformValue("DepthMap", m_displacementMap);
    
    m_shadowReplacements->push_back(std::pair(propMaterial, shadowPropMaterial));

    return propMaterial;
}

std::shared_ptr<SceneModel> PostFXSceneViewerApplication::SpawnProp(ModelLoader loader, const char* objectName, const char* modelPath) {

    // the index of the new prop is equal to the current amount of props since we index from 0
    int propIndex = m_propModels->size();
    std::shared_ptr<Material> propMaterial = GeneratePropMaterial(propIndex);

    loader.SetReferenceMaterial(propMaterial);

    std::shared_ptr<Model> model = loader.LoadShared(modelPath);
    std::shared_ptr<SceneModel> sceneModel = std::make_shared<SceneModel>(objectName, model);
    m_scene.AddSceneNode(sceneModel);

    // add prop to list of props
    m_propModels->push_back(sceneModel);

    return sceneModel;
}

void PostFXSceneViewerApplication::InitializeModels()
{
    m_propModels = std::make_shared<std::vector<std::shared_ptr<SceneModel>>>();
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
    // if true, you need to set uniforms by looping over every single material on the model you wanna update, since each material is a different instance.
    loader.SetCreateMaterials(true);

    // Flip vertically textures loaded by the model loader
    loader.GetTexture2DLoader().SetFlipVertical(true);

    // Link vertex properties to attributes
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

    // Generate ground plane
    std::shared_ptr<Model> planeModel = Model::GeneratePlane(m_desertLength, m_desertWidth, m_desertVertexRows, m_desertVertexCollumns);
    planeModel->AddMaterial(m_desertSandMaterial);
    m_desertModel = std::make_shared<SceneModel>("Plane", planeModel);
    m_scene.AddSceneNode(m_desertModel);
    m_desertModel->GetTransform()->SetTranslation(glm::vec3(1, 0, 0));

    //// Load models
    // Parent model shoudl have an empty model.
    m_parentModel = std::make_shared<SceneModel>("parent", nullptr);
    m_scene.AddSceneNode(m_parentModel);

    // Player model. Automatically follows parent model.
    loader.SetReferenceMaterial(m_driveOnSandMateral);

    std::shared_ptr<Model> carModel = std::make_shared<Model>(loader.Load("models/car/car.obj"));
    m_visualPlayerModel = std::make_shared<SceneModel>("car", carModel);
    m_scene.AddSceneNode(m_visualPlayerModel);

    // Generate two props and see if they get different materials that react to them.
    // Afterwards, thest that they can retrieve locations and use it to set alpha or something like that.
    std::shared_ptr<SceneModel> archA = SpawnProp(loader, "archA", "models/arch/arch.obj");
    std::shared_ptr<SceneModel> archB = SpawnProp(loader, "archB", "models/arch/arch.obj");
    archB->GetTransform()->SetTranslation(glm::vec3(2, 0, 2));
}

void PostFXSceneViewerApplication::InitializeFramebuffers()
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

void PostFXSceneViewerApplication::InitializeRenderer()
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
        std::unique_ptr<ShadowMapRenderPass> shadowMapRenderPass(std::make_unique<ShadowMapRenderPass>(m_mainLight, m_shadowMapMaterial, m_shadowReplacements));
        shadowMapRenderPass->SetVolume(m_visualPlayerModel->GetTransform()->GetTranslation(), glm::vec3(30.0f));
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

std::shared_ptr<Material> PostFXSceneViewerApplication::CreatePostFXMaterial(const char* fragmentShaderPath, std::shared_ptr<Texture2DObject> sourceTexture)
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

Renderer::UpdateTransformsFunction PostFXSceneViewerApplication::GetFullscreenTransformFunction(std::shared_ptr<ShaderProgram> shaderProgramPtr) const
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

void PostFXSceneViewerApplication::RenderGUI()
{
    m_imGui.BeginFrame();

    // Draw GUI for scene nodes, using the visitor pattern
    ImGuiSceneVisitor imGuiVisitor(m_imGui, "Scene");
    m_scene.AcceptVisitor(imGuiVisitor);

    // Draw GUI for camera controller
    m_cameraController.DrawGUI(m_imGui);

    if (auto window = m_imGui.UseWindow("Desert sand uniforms"))
    {
        if (ImGui::DragFloat("AmbientOcclusion", &m_ambientOcclusion, 0.1f, 0, 1))
        {
            m_desertSandMaterial->SetUniformValue("AmbientOcclusion", m_ambientOcclusion);
        }
        if (ImGui::DragFloat("Metalness", &m_metalness, 0.1f, 0, 1))
        {
            m_desertSandMaterial->SetUniformValue("Metalness", m_metalness);
        }
        if (ImGui::DragFloat("Roughness", &m_roughness, 0.1f, 0, 1))
        {
            m_desertSandMaterial->SetUniformValue("Roughness", m_roughness);
        }
        if (ImGui::DragFloat("Unused", &m_unused, 0.1f, 0, 1))
        {
            m_desertSandMaterial->SetUniformValue("Unused", m_unused);
        }
        if (ImGui::DragFloat("OffsetStrength", &m_offsetStrength, 0.1f, 0, 10))
        {
            m_visualPlayerModel->GetModel()->SetUniformOnAllMaterials("OffsetStrength", m_offsetStrength);
            m_desertSandMaterial->SetUniformValue("OffsetStrength", m_offsetStrength);
        }
        if (ImGui::DragFloat("SampleDistance", &m_sampleDistance, 0.01f, 0.01f, 1))
        {
            m_visualPlayerModel->GetModel()->SetUniformOnAllMaterials("SampleDistance", m_sampleDistance);
            m_desertSandMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        }
        if (ImGui::DragFloat("NoiseStrength", &m_noiseStrength, 0.1f, -1, 1))
        {
            m_desertSandMaterial->SetUniformValue("NoiseStrength", m_noiseStrength);
        }
        if (ImGui::DragFloat("NoiseTileFrequency", &m_noiseTilefrequency, 0.1f, 0, 10))
        {
            m_desertSandMaterial->SetUniformValue("NoiseTileFrequency", m_noiseTilefrequency);
        }
        if (ImGui::DragFloat("WaveWidth", &m_waveWidth, 0.1f, 0, 3))
        {
            m_desertSandMaterial->SetUniformValue("WaveWidth", m_waveWidth);
        }
        if (ImGui::DragFloat("WaveStrength", &m_waveStength, 0.1f, 0, 3))
        {
            m_desertSandMaterial->SetUniformValue("WaveStrength", m_waveStength);
        }

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

            ImGui::Separator();
            if (ImGui::ColorEdit3("Specular Color", &m_specularColor[0]))
            {
                m_deferredMaterial->SetUniformValue("SpecularColor", m_specularColor);
            }
            if (ImGui::ColorEdit3("Fog Color", &m_fogColor[0]))
            {
                m_deferredMaterial->SetUniformValue("FogColor", m_fogColor);
            }
            if (ImGui::SliderFloat("Fog strength", &m_fogStrength, 0.0f, 1.0f))
            {
                m_deferredMaterial->SetUniformValue("FogStrength", m_fogStrength);
            }
            if (ImGui::SliderFloat("Fog distance", &m_fogDistance, -1.0f, 4.0f))
            {
                m_deferredMaterial->SetUniformValue("FogDistance", m_fogDistance);
            }
        }
    }

    m_imGui.EndFrame();
}
