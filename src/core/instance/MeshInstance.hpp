#include "Global.hpp"
#include "instance/MaterialInstance.hpp"

namespace Lu{
    namespace Core{

    struct Mesh {
        Material material{};
        uint32_t transformId{0};        // Transform (body, e.g. driven by physics)
        uint32_t pipelineId{0};       
        uint32_t meshInfoId{0};
        uint32_t valid{0};
        uint32_t localTransformId{0};   // MeshTransform slot, 0 = identity slot (no MeshTransform)
        uint32_t _pad0{0};
        uint32_t _pad1{0};
        uint32_t _pad2{0};

        Mesh() = default;

        Mesh(const Material& material, const uint32_t transformId, const uint32_t localTransformId,
             const uint32_t pipelineId, const uint32_t meshInfoId):
            material(material),
            transformId(transformId),
            pipelineId(pipelineId),
            meshInfoId(meshInfoId),
            valid(1),
            localTransformId(localTransformId)
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
    static_assert(sizeof(Mesh) == 112);
    static_assert(offsetof(MeshInstance, id) == 112);
    static_assert(sizeof(MeshInstance) == 128);

}
}