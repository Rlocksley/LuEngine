#include "Global.hpp"
#include "component/Mesh.hpp"
#include "component/Material.hpp"
#include "instance/MaterialInstance.hpp"

namespace Lu{
    namespace Core{

    struct Mesh {
        Material material{};
        uint32_t transformId{0};   
        uint32_t pipelineId{0};       
        uint32_t meshInfoId{0};
        uint32_t valid{0};

        Mesh() = default;

        Mesh(const Component::Material& material, const uint32_t transformId, 
             const uint32_t pipelineId, const uint32_t meshInfoId):
            material(material),
            transformId(transformId),
            pipelineId(pipelineId),
            meshInfoId(meshInfoId),
            valid(1)
            {}
    };

    using MeshId = uint32_t;

    struct MeshInstance {
        Mesh mesh{};
        MeshId id{0};
        glm::vec3 _pad{0.f, 0.f, 0.f};

        MeshInstance() = default;

        MeshInstance(const Mesh mesh, MeshId id) :
        mesh(mesh), id(id) {}
    };

    static_assert(sizeof(Material) == 80);
    static_assert(sizeof(Mesh) == 96);
    static_assert(offsetof(MeshInstance, id) == 96);
    static_assert(sizeof(MeshInstance) == 112);

}
}