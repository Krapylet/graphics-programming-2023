#pragma once

#include <ituGL/renderer/RenderPass.h>

#include <glm/vec3.hpp>
#include <vector>

class Light;
class Camera;
class Material;

class ShadowMapRenderPass : public RenderPass
{
public:
    ShadowMapRenderPass(std::shared_ptr<Light> light, std::shared_ptr<const Material> defaultMaterial,
        std::shared_ptr<std::vector<std::shared_ptr<const Material>>> uniqueMaterials,
        std::shared_ptr<std::vector<std::shared_ptr<const Material>>> replacementMaterials,
        int drawcallCollectionIndex = 0);

    void SetVolume(glm::vec3 volumeCenter, glm::vec3 volumeSize);

    void Render() override;

private:
    void InitFramebuffer();
    void InitLightCamera(Camera& lightCamera) const;

private:
    std::shared_ptr<Light> m_light;

    std::shared_ptr<const Material> m_material;

    // Extra materials used to detect and replace a certain material with another material than the defualt empty.vert/frag.
    // We have to use two different materials, since the we only want to include the vertexshader from the exception material.
    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> m_uniqueMaterials;
    std::shared_ptr<std::vector<std::shared_ptr<const Material>>> m_replacementMaterials;

    int m_drawcallCollectionIndex;

    glm::vec3 m_volumeCenter;
    glm::vec3 m_volumeSize;
};
