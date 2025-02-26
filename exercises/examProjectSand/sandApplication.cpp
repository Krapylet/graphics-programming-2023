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
        inputSpeed += -1.0f;
    if (window.IsKeyPressed(GLFW_KEY_S))
        inputSpeed += 0.3f;

    // Multiply result by player parameters
    inputSpeed *= m_playerSpeed;
    inputAngularSpeed *= m_playerAngularSpeed;

    // Double speed if SHIFT is pressed
    if (window.IsKeyPressed(GLFW_KEY_LEFT_SHIFT))
        inputSpeed *= 2.0f;
    

    // Find the local directions for the player objects
    std::shared_ptr<Transform> parentTransform = m_parentModel->GetTransform();
    glm::mat3 transposed = parentTransform->GetTransformMatrix(); // glm::transpose(parentTransform->GetTranslationMatrix());
    
    // Keeping unsude directions here so i can remember where they are placed.
    // NB: They are not normalized.
    glm::vec3 right = transposed[0];
    glm::vec3 up = transposed[1];
    glm::vec3 forward = transposed[2];

    // Apply speed over time to the translation
    float delta = GetDeltaTime();
    glm::vec3 translation = parentTransform->GetTranslation();
    glm::vec3 rotation = parentTransform->GetRotation();
    
    translation += forward * inputSpeed * delta;
    rotation += up * inputAngularSpeed * delta; 

    // apply the updated translation and rotation to the parent.
    parentTransform->SetTranslation(translation);
    parentTransform->SetRotation(rotation);

    // Apply move the visual model along with the parnet
    std::shared_ptr<Transform> playerModelTransform = m_visualPlayerModel->GetTransform();
    playerModelTransform->SetTranslation(translation);
}

// Makes camera follow the model in m_parentModel in a third person view.
void SandApplication::MakeCameraFollowPlayer() {
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

    // Now we can move the camera back and a bit up to get the player model in frame
    translation += (forward + up / 3.0f) * m_cameraPlayerDistance + up * m_offsetStength/2.0f;
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
    // Initialize shadow replacement 
    m_materialsWithUniqueShadows = std::make_shared<std::vector<std::shared_ptr<const Material>>>();
    m_uniqueShadowMaterials = std::make_shared<std::vector<std::shared_ptr<const Material>>>();

    m_displacementMap = Texture2DLoader::LoadTextureShared("textures/SandDisplacementMapTest2.jpg", TextureObject::FormatR, TextureObject::InternalFormatR, true, false, false);

    // default shadow map material
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
        m_desertSandMaterial->SetUniformValue("DepthMap", m_displacementMap);

        // Initial depth parameters
        m_desertSandMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        m_desertSandMaterial->SetUniformValue("OffsetStrength", m_offsetStength);

        // Normal texture
        std::shared_ptr<Texture2DObject> normalMap = Texture2DLoader::LoadTextureShared("textures/SandNormalMap.png", TextureObject::FormatRGB, TextureObject::InternalFormatRGB8, true, false);
        m_desertSandMaterial->SetUniformValue("NormalTexture", normalMap);
        m_desertSandMaterial->SetUniformValue("ObjectSize", glm::vec2(m_desertWidth, m_desertLength));
        m_desertSandMaterial->SetUniformValue("TileSize", 10.0f);



        m_desertSandMaterial->SetUniformValue("ColorTexture", normalMap);

        m_materialsWithUniqueShadows->push_back(m_desertSandMaterial);
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
        m_desertSandShadowMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

        // Set material uniforms
        // Depth map. Since it's black and white, there's no reason to load more than one channel.
        m_desertSandShadowMaterial->SetUniformValue("DepthMap", m_displacementMap);

        // Initial depth parameters
        m_desertSandShadowMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        m_desertSandShadowMaterial->SetUniformValue("OffsetStrength", m_offsetStength);

        m_uniqueShadowMaterials->push_back(m_desertSandShadowMaterial);
    }


    // Drive on sand material
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
        ShaderProgram::Location objectUVPositionLocation = shaderProgramPtr->GetUniformLocation("DesertUV");
        ShaderProgram::Location objectPivotPositionLocation = shaderProgramPtr->GetUniformLocation("PivotPosition");
        ShaderProgram::Location forwardLocation = shaderProgramPtr->GetUniformLocation("Right");
        ShaderProgram::Location offsetStrengthLocation = shaderProgramPtr->GetUniformLocation("OffsetStrength");
        ShaderProgram::Location sampleDistanceLocation = shaderProgramPtr->GetUniformLocation("SampleDistance");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(offsetStrengthLocation, m_offsetStength);
                shaderProgram.SetUniform(sampleDistanceLocation, m_sampleDistance);
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
        shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
        glm::vec3 modelPos = m_parentModel->GetTransform()->GetTranslation();
        shaderProgram.SetUniform(objectPivotPositionLocation, modelPos);

        // Calculate the models position on the desert model in UV coordinates.
        glm::vec3 desertPos = m_desertModel->GetTransform()->GetTranslation();
        glm::vec3 desertScale = m_desertModel->GetTransform()->GetScale();
        glm::vec3 desertPosOnDesert = modelPos - desertPos;
        u = desertPosOnDesert.x / m_desertLength * desertScale.x + 0.5;
        v = desertPosOnDesert.z / m_desertWidth * desertScale.z + 0.5;
        shaderProgram.SetUniform(objectUVPositionLocation, glm::vec2(u, v));

        // Calculate model right direction direction, so we can cross it with the plane's normal to get the new model forward.
        glm::mat3 transposed = m_parentModel->GetTransform()->GetTransformMatrix(); // glm::transpose(parentTransform->GetTranslationMatrix());
        glm::vec3 right = transposed[0];
        shaderProgram.SetUniform(forwardLocation, right);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");
        filteredUniforms.insert("desertUV");
        filteredUniforms.insert("PivotPositon");
        filteredUniforms.insert("Right");

        // Create material
        m_driveOnSandMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

        //// Set material uniforms

        // Color
        m_driveOnSandMaterial->SetUniformValue("Color", glm::vec3(1.0f, 1.0f, 1.0f));  // Sand ground color

        // Depth map. Since it's black and white, there's no reason to load more than one channel.
        m_driveOnSandMaterial->SetUniformValue("DepthMap", m_displacementMap);

        m_materialsWithUniqueShadows->push_back(m_driveOnSandMaterial);
    }

    // drive on desert shadow material.
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
        fragmentShaderPaths.push_back("shaders/renderer/empty.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");
        ShaderProgram::Location objectUVPositionLocation = shaderProgramPtr->GetUniformLocation("DesertUV");
        ShaderProgram::Location objectPivotPositionLocation = shaderProgramPtr->GetUniformLocation("PivotPosition");
        ShaderProgram::Location forwardLocation = shaderProgramPtr->GetUniformLocation("Right");


        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
                shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
                glm::vec3 modelPos = m_parentModel->GetTransform()->GetTranslation();
                shaderProgram.SetUniform(objectPivotPositionLocation, modelPos);

                // Calculate the models position on the desert model in UV coordinates.
                glm::vec3 desertPos = m_desertModel->GetTransform()->GetTranslation();
                glm::vec3 desertScale = m_desertModel->GetTransform()->GetScale();
                glm::vec3 desertPosOnDesert = modelPos - desertPos;
                u = desertPosOnDesert.x / m_desertLength * desertScale.x + 0.5;
                v = desertPosOnDesert.z / m_desertWidth * desertScale.z + 0.5;
                shaderProgram.SetUniform(objectUVPositionLocation, glm::vec2(u, v));

                // Calculate model right direction direction, so we can cross it with the plane's normal to get the new model forward.
                glm::mat3 transposed = m_parentModel->GetTransform()->GetTransformMatrix(); // glm::transpose(parentTransform->GetTranslationMatrix());
                glm::vec3 right = transposed[0];
                shaderProgram.SetUniform(forwardLocation, right);
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");
        filteredUniforms.insert("desertUV");
        filteredUniforms.insert("PivotPositon");
        filteredUniforms.insert("Right");

        // Create material
        m_driveOnSandShadowMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

        //// Set material uniforms
        // Depth map. Since it's black and white, there's no reason to load more than one channel.
        m_driveOnSandShadowMaterial->SetUniformValue("DepthMap", m_displacementMap);

        // Initial depth parameters
        m_driveOnSandShadowMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        m_driveOnSandShadowMaterial->SetUniformValue("OffsetStrength", m_offsetStength);

        m_uniqueShadowMaterials->push_back(m_driveOnSandShadowMaterial);
    }

    // Prop material ( Needs to be complied individually for each prop)
    


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

std::shared_ptr<Material> SandApplication::GeneratePropMaterial() {
    {
        // Load and build shader
        std::vector<const char*> vertexShaderPaths;
        vertexShaderPaths.push_back("shaders/version330.glsl");
        vertexShaderPaths.push_back("shaders/depthMapUtils.glsl");
        vertexShaderPaths.push_back("shaders/prop.vert");
        Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

        std::vector<const char*> fragmentShaderPaths;
        fragmentShaderPaths.push_back("shaders/version330.glsl");
        fragmentShaderPaths.push_back("shaders/utils.glsl");
        fragmentShaderPaths.push_back("shaders/prop.frag");
        Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

        std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
        shaderProgramPtr->Build(vertexShader, fragmentShader);

        // Get transform related uniform locations
        ShaderProgram::Location worldViewMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewMatrix");
        ShaderProgram::Location worldViewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldViewProjMatrix");
        ShaderProgram::Location objectUVPositionLocation = shaderProgramPtr->GetUniformLocation("DesertUV");
        ShaderProgram::Location offsetStrengthLocation = shaderProgramPtr->GetUniformLocation("OffsetStrength");
        ShaderProgram::Location sampleDistanceLocation = shaderProgramPtr->GetUniformLocation("SampleDistance");

        // Register shader with renderer
        m_renderer.RegisterShaderProgram(shaderProgramPtr,
            [=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
            {
                shaderProgram.SetUniform(offsetStrengthLocation, m_offsetStength);
        shaderProgram.SetUniform(sampleDistanceLocation, m_sampleDistance);
        shaderProgram.SetUniform(worldViewMatrixLocation, camera.GetViewMatrix() * worldMatrix);
        shaderProgram.SetUniform(worldViewProjMatrixLocation, camera.GetViewProjectionMatrix() * worldMatrix);
        glm::vec3 modelPos = m_propModels->at(0)->GetTransform()->GetTranslation();

        // Calculate the models position on the desert model in UV coordinates.
        glm::vec3 desertPos = m_desertModel->GetTransform()->GetTranslation();
        glm::vec3 desertScale = m_desertModel->GetTransform()->GetScale();
        glm::vec3 desertPosOnDesert = modelPos - desertPos;
        u = desertPosOnDesert.x / m_desertLength * desertScale.x + 0.5;
        v = desertPosOnDesert.z / m_desertWidth * desertScale.z + 0.5;
        shaderProgram.SetUniform(objectUVPositionLocation, glm::vec2(u, v));

        // Calculate model right direction direction, so we can cross it with the plane's normal to get the new model forward.
        glm::mat3 transposed = m_propModels->at(0)->GetTransform()->GetTransformMatrix(); // glm::transpose(parentTransform->GetTranslationMatrix());
        glm::vec3 right = transposed[0];
            },
            nullptr
                );

        // Filter out uniforms that are not material properties
        ShaderUniformCollection::NameSet filteredUniforms;
        filteredUniforms.insert("WorldViewMatrix");
        filteredUniforms.insert("WorldViewProjMatrix");
        filteredUniforms.insert("DesertUV");
        filteredUniforms.insert("OffsetStrength");
        filteredUniforms.insert("SampleDist");

        // Create material
        std::shared_ptr<Material> propMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

        //// Set material uniforms

        // Color
        propMaterial->SetUniformValue("Color", glm::vec3(1.0f, 1.0f, 1.0f));  // Sand ground color

        // Depth map. Since it's black and white, there's no reason to load more than one channel.
        propMaterial->SetUniformValue("DepthMap", m_displacementMap);

        //m_materialsWithUniqueShadows->push_back(propMaterial);

        return propMaterial;
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
    ModelLoader loader(m_driveOnSandMaterial);

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
    std::shared_ptr<Model> cannonModel = loader.LoadShared("models/temple-ruin/Temple ruin.obj");
    std::shared_ptr<SceneModel> player =  std::make_shared<SceneModel>("cannon", cannonModel);
    m_scene.AddSceneNode(player);
    m_visualPlayerModel = player;

    // Replace the default model material with the special material that offsets vertex positions by the height map.
    int materialCount = cannonModel->GetMaterialCount();
    for (unsigned int i = 0; i < materialCount; i++)
    {
        //cannonModel->SetMaterial(i, m_driveOnSandMaterial);
    }

    // The parent framework doesn't look like it's done, so Instead I'm doing a quick and dirty hack to emulate an empty parent of the
    // camera and the player visual model.
    // there's probably techinaclly a memory leak here, but i don't think i have time to fix it. It's small anyway.
    std::shared_ptr<Model> debugCanonModel(loader.LoadNew("models/cannon/cannon.obj"));
    std::shared_ptr<SceneModel> parent = std::make_shared<SceneModel>("parent", debugCanonModel);
    parent->GetTransform()->SetScale(glm::vec3(0.1f, 0.1f, 0.1f));
    m_scene.AddSceneNode(parent);
    m_parentModel = parent;

    // Generate ground plane
    std::shared_ptr<Model> planeModel = Model::GeneratePlane(m_desertLength,m_desertWidth, m_desertVertexRows, m_desertVertexCollumns);
    planeModel->AddMaterial(m_desertSandMaterial);
    std::shared_ptr<SceneModel> plane = std::make_shared<SceneModel>("Plane", planeModel);
    m_scene.AddSceneNode(plane);
    m_desertModel = plane;

    // Load props
    m_propModels = std::make_shared<std::vector<std::shared_ptr<SceneModel>>>();
    //AddProp("Temple Ruin", "models/temple-ruin/Temple ruin.obj", loader);
}

std::shared_ptr<SceneModel> SandApplication::AddProp(const char* objectName, const char* modelPath, ModelLoader loader) {
    std::shared_ptr<Material> propMaterial = GeneratePropMaterial();

    // Set the generated material as reference, so that object textures are inserted correctly.
    loader.SetReferenceMaterial(propMaterial);
    std::shared_ptr<Model> model = loader.LoadShared(modelPath);
    std::shared_ptr<SceneModel> sceneModel = std::make_shared<SceneModel>(objectName, model);
    m_propModels->push_back(sceneModel);

    // add prop to scene.
    m_scene.AddSceneNode(m_propModels->at(0));

    return m_propModels->at(0);
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
            ,m_materialsWithUniqueShadows, m_uniqueShadowMaterials));
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
        // For some reason Setting sampleDistance and OffsetDistance has no effect on the driveOnSand material, but it does on the shadow version...
        if (ImGui::DragFloat("Sample distance", &m_sampleDistance, 0.001f, 0.0f, 0.1f))
        {
            m_desertSandMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
            m_desertSandShadowMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
            m_driveOnSandMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
            m_driveOnSandShadowMaterial->SetUniformValue("SampleDistance", m_sampleDistance);
        }

        if (ImGui::DragFloat("Offset strength", &m_offsetStength,0.1f, 0.0f, 10.0f))
        {
            m_desertSandMaterial->SetUniformValue("OffsetStrength", m_offsetStength);
            m_desertSandShadowMaterial->SetUniformValue("OffsetStrength", m_offsetStength);
            m_driveOnSandMaterial->SetUniformValue("OffsetStrength", m_offsetStength);
            m_driveOnSandShadowMaterial->SetUniformValue("OffsetStrength", m_offsetStength);
        }

        if (ImGui::DragFloat("EnableFog", &m_enableFog, 0.1f, 0, 1)) {
            m_deferredMaterial->SetUniformValue("EnableFog", m_enableFog);
        }

        ImGui::InputFloat("U", &u);
        ImGui::InputFloat("V", &v);
    }
    

    if (auto window = m_imGui.UseWindow("Player parameters"))
    {
        ImGui::DragFloat("Camera distance", &m_cameraPlayerDistance, 1.0f, 2.0f, 10.0f);
        ImGui::DragFloat("Speed", &m_playerSpeed, 10.0f, 10.0f, 100.0f);
        ImGui::DragFloat("AngularSpeed", &m_playerAngularSpeed, 1.0f, 4.0f, 8.0f);
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
