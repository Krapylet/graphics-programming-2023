#include <ituGL/geometry/Model.h>

#include <ituGL/geometry/Mesh.h>
#include <ituGL/shader/Material.h>

#include <ituGL/geometry/VertexFormat.h>

Model::Model(std::shared_ptr<Mesh> mesh) : m_mesh(mesh)
{
}

Mesh& Model::GetMesh()
{
    return *m_mesh;
}

const Mesh& Model::GetMesh() const
{
    return *m_mesh;
}

void Model::SetMesh(std::shared_ptr<Mesh> mesh)
{
    // Clear the material list before changing the mesh
    assert(m_materials.empty());
    m_mesh = mesh;
}

unsigned int Model::GetMaterialCount()
{
    return static_cast<unsigned int>(m_materials.size());
}

Material& Model::GetMaterial(unsigned int index)
{
    return *m_materials[index];
}

const Material& Model::GetMaterial(unsigned int index) const
{
    return *m_materials[index];
}

void Model::SetMaterial(unsigned int index, std::shared_ptr<Material> material)
{
    m_materials[index] = material;
}

unsigned int Model::AddMaterial(std::shared_ptr<Material> material)
{
    unsigned int index = static_cast<unsigned int>(m_materials.size());
    m_materials.push_back(material);
    return index;
}

void Model::ClearMaterials()
{
    m_materials.clear();
}

void Model::Draw()
{
    if (m_mesh)
    {
        // Ensure that there is a material for each submesh
        assert(m_mesh->GetSubmeshCount() == m_materials.size());

        for (unsigned int submeshIndex = 0; submeshIndex < m_mesh->GetSubmeshCount(); ++submeshIndex)
        {
            Material& material = GetMaterial(submeshIndex);

            // Set up the material before rendering
            material.Use();

            // Draw the submesh
            m_mesh->DrawSubmesh(submeshIndex);
        }
    }
}

std::shared_ptr<Model> Model::GeneratePlane(float length, float width, int rows, int collumns) {
    // Generate plane.
// 
// 1. Define constants
    int vertexCount = rows * collumns;

    // 2. Define the vertex structure
    struct Vertex
    {
        Vertex() = default;
        Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec3& tangent, const glm::vec3& bitangent, const glm::vec2& texCoord)
            : position(position), normal(normal), tangent(tangent), bitangent(bitangent), texCoord(texCoord) {}
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 texCoord;  // texture UV coordinate
    };

    // 3. Define the vertex format matching vertex structure
    VertexFormat vertexFormat;
    vertexFormat.AddVertexAttribute<float>(3);
    vertexFormat.AddVertexAttribute<float>(3);
    vertexFormat.AddVertexAttribute<float>(3);
    vertexFormat.AddVertexAttribute<float>(3);
    vertexFormat.AddVertexAttribute<float>(2);

    // Initialize VBO and EBO
    std::vector<Vertex> vertices; // VBO
    std::vector<unsigned short> indices; // EBO

    // 4. Generate verticies
    // Since its just a plane, normal, tangent and bitangent becomes super easy to calculate
    //glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    //glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    //glm::vec3 bitangent = glm::vec3(0.0f, 0.0f, 1.0f);

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < collumns; c++)
        {
            glm::vec3 tangent = glm::vec3(0, 0, 1);
            glm::vec3 bitangent = glm::vec3(1, 0, 0);
            glm::vec3 normal = glm::normalize(glm::cross(bitangent, tangent));

            // 4.1 Calculate position
            float x = r * length / (rows - 1);
            float y = 0;
            float z = c * width / (collumns - 1);

            glm::vec3 vertexPos = glm::vec3(x, y, z);

            // 4.2 calulate texture coordinate (UV). Both are clamped betwen 0-1
            // For some reason, sampling near the edges becomes a problem. It seems to be a promlem with the loader? The image gets compressed quite a bit.
            float u = x / length;
            float v = z / width;
            glm::vec2 texCoord = glm::vec2(u, v);

            // 4.3 Add vetexes to VBO
            vertices.emplace_back(vertexPos, normal, tangent, bitangent, texCoord);
        }
    }

    // 6. Calculate triangles
    // We loop over rows-1 and collumns-1 because for each vertex 'o', we add two triangles as seen in this sketch:
    //  o---*   *     // A---B
    //  | / |         // | / |
    //  *---*   *     // C---D
    //
    //  *   *   *
    for (int c = 0; c < collumns - 1; c++)
    {
        for (int r = 0; r < rows - 1; r++)
        {
            // 5.1 Calculate VBO indexes of vertexes 
            int A = c + collumns * r;
            int B = c + collumns * r + 1;
            int C = c + collumns * (r + 1);
            int D = c + collumns * (r + 1) + 1;

            // 5.2 Add tris to the EBO. The front face is determined by counter clockwise winding.
            // Upper triangle: A B C
            indices.push_back(A); indices.push_back(B); indices.push_back(C);

            // Lower triangle: B D C
            indices.push_back(B); indices.push_back(D); indices.push_back(C);
        }
    }

    // 7. Create the new model with all the data
    std::shared_ptr planeMesh = std::make_shared<Mesh>();
    planeMesh->AddSubmesh<Vertex, unsigned short, VertexFormat::LayoutIterator>(Drawcall::Primitive::Triangles, vertices, indices,
        vertexFormat.LayoutBegin(static_cast<int>(vertices.size()), true /* interleaved */), vertexFormat.LayoutEnd());

    // 8. Assign model to a model and give it a material.
    std::shared_ptr<Model> planeModel = std::make_shared<Model>(planeMesh);
    //planeModel->AddMaterial(defaultMaterial);

    return planeModel;
}
