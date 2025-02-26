#include <ituGL/renderer/ShadowMapRenderPass.h>

#include <ituGL/renderer/Renderer.h>
#include <ituGL/lighting/Light.h>
#include <ituGL/camera/Camera.h>
#include <ituGL/shader/Material.h>
#include <ituGL/texture/Texture2DObject.h>
#include <ituGL/texture/FramebufferObject.h>

ShadowMapRenderPass::ShadowMapRenderPass(std::shared_ptr<Light> light, std::shared_ptr<const Material> defaultMaterial,
    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> uniqueMaterials,
    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> replacementMaterials,
    int drawcallCollectionIndex)
    : m_light(light)
    , m_material(defaultMaterial)
    , m_uniqueMaterials(uniqueMaterials)
    , m_replacementMaterials(replacementMaterials)
    , m_drawcallCollectionIndex(drawcallCollectionIndex)
    , m_volumeCenter(0.0f)
    , m_volumeSize(1.0f)
{
    InitFramebuffer();
}

ShadowMapRenderPass::ShadowMapRenderPass(std::shared_ptr<Light> light, std::shared_ptr<const Material> defaultMaterial,
    int drawcallCollectionIndex)
    : m_light(light)
    , m_material(defaultMaterial)
    , m_uniqueMaterials(nullptr)
    , m_replacementMaterials(nullptr)
    , m_drawcallCollectionIndex(drawcallCollectionIndex)
    , m_volumeCenter(0.0f)
    , m_volumeSize(1.0f)
{
    InitFramebuffer();
}

void ShadowMapRenderPass::SetVolume(glm::vec3 volumeCenter, glm::vec3 volumeSize)
{
    m_volumeCenter = volumeCenter;
    m_volumeSize = volumeSize;
}

void ShadowMapRenderPass::InitFramebuffer()
{
    std::shared_ptr<FramebufferObject> targetFramebuffer = std::make_shared<FramebufferObject>();

    targetFramebuffer->Bind();

    std::shared_ptr<const TextureObject> shadowMap = m_light->GetShadowMap();
    assert(shadowMap);
    targetFramebuffer->SetTexture(FramebufferObject::Target::Draw, FramebufferObject::Attachment::Depth, *shadowMap);

    FramebufferObject::Unbind();

    m_targetFramebuffer = targetFramebuffer;
}

void ShadowMapRenderPass::Render()
{
    Renderer& renderer = GetRenderer();
    DeviceGL& device = renderer.GetDevice();

    const auto& drawcallCollection = renderer.GetDrawcalls(m_drawcallCollectionIndex);

    device.Clear(false, Color(), true, 1.0f);

    // Use shadow map shader
    m_material->Use();
    std::shared_ptr<const ShaderProgram> shaderProgram = m_material->GetShaderProgram();

    // Backup current viewport
    glm::ivec4 currentViewport;
    device.GetViewport(currentViewport.x, currentViewport.y, currentViewport.z, currentViewport.w);

    // Set viewport to texture size
    device.SetViewport(0, 0, 512, 512);

    // Backup current camera
    const Camera& currentCamera = renderer.GetCurrentCamera();

    // Set up light as the camera
    Camera lightCamera;
    InitLightCamera(lightCamera);
    renderer.SetCurrentCamera(lightCamera);

    // for all drawcalls
    bool first = true;
    for (const Renderer::DrawcallInfo& drawcallInfo : drawcallCollection)
    {
        // Bind the vao
        drawcallInfo.vao.Bind();


        // if no unique materials are defined, use the default one.
        if (m_uniqueMaterials == nullptr) {
            renderer.UpdateTransforms(shaderProgram, drawcallInfo.worldMatrixIndex, first);
            drawcallInfo.drawcall.Draw();
            first = false;
            continue;
        }


        //// Set up object matrix
        // check if the material uses a non-default shadow material.
        bool drawcallShouldUseReplacementMaterial = false;
        int shadowIndex = 0;
        for (size_t i = 0; i < m_uniqueMaterials->size(); i++)
        {
            drawcallShouldUseReplacementMaterial = drawcallInfo.material.GetShaderProgram() == m_uniqueMaterials->at(i)->GetShaderProgram();

            if (drawcallShouldUseReplacementMaterial) {
                shadowIndex = i;
                break;
            }
        }

        // Use unique material if nessecary 
        if (drawcallShouldUseReplacementMaterial) {
            std::shared_ptr<const Material> replacementMaterial = m_replacementMaterials->at(shadowIndex);
            replacementMaterial->Use();
            renderer.UpdateTransforms(replacementMaterial->GetShaderProgram(), drawcallInfo.worldMatrixIndex, first);
            drawcallInfo.drawcall.Draw();
            m_material->Use();
        }
        // else use the default empty shader program.
        else {
            renderer.UpdateTransforms(shaderProgram, drawcallInfo.worldMatrixIndex, first);
            drawcallInfo.drawcall.Draw();
        }
        
        // Render drawcall
        first = false;
    }

    m_light->SetShadowMatrix(lightCamera.GetViewProjectionMatrix());

    // Restore viewport
    renderer.GetDevice().SetViewport(currentViewport.x, currentViewport.y, currentViewport.z, currentViewport.w);

    // Restore current camera
    renderer.SetCurrentCamera(currentCamera);

    // Restore default framebuffer to avoid drawing to the shadow map
    renderer.SetCurrentFramebuffer(renderer.GetDefaultFramebuffer());
}

void ShadowMapRenderPass::InitLightCamera(Camera& lightCamera) const
{
    // View matrix
    glm::vec3 position = m_light->GetPosition(m_volumeCenter);
    glm::vec3 direction = m_light->GetDirection();
    lightCamera.SetViewMatrix(position, position + direction, std::abs(direction.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1));

    // Projection matrix
    glm::vec4 attenuation = m_light->GetAttenuation();
    switch (m_light->GetType())
    {
    case Light::Type::Directional:
        lightCamera.SetOrthographicProjectionMatrix(-0.5f * m_volumeSize, 0.5f * m_volumeSize);
        break;
    case Light::Type::Spot:
        lightCamera.SetPerspectiveProjectionMatrix(attenuation.w, 1.0f, 0.01f, attenuation.y);
        break;
    default:
        assert(false);
        break;
    }
}
