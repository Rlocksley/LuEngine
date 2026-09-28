#include <flecs.h>
#include <iostream>

// 1. Definiere dein Struct mit Entity-Typen
struct MeshComponent {
    flecs::entity meshId;      // Nutze flecs::entity_t statt uint32_t
    flecs::entity pipelineId;  // Das ist unter der Haube auch ein uint64_t / ID
};

int main() {
    flecs::string json;
    {
        flecs::world world;

        // 2. Registriere das Struct im Flecs Meta-System
        // Durch .expr() weiß Flecs, wie das Struct aufgebaut ist und dass es sich um Entities handelt.
        world.component<MeshComponent>()
            .member<flecs::entity>("meshId")
            .member<flecs::entity>("pipelineId");

        // 3. Erstelle deine "Assets" als Named-Entities in Flecs
        // Diese Namen werden für das JSON-File verwendet!
        flecs::entity playerMesh = world.entity("assets::models::player_mesh");
        flecs::entity opaquePipeline = world.entity("pipelines::opaque_lit");

        // 4. Erstelle eine Entity, die diese Komponente nutzt
        flecs::entity e = world.entity("PlayerEntity")
            .set<MeshComponent>({ playerMesh, opaquePipeline });

        flecs::entity e2 = world.entity("PlayerEntity2")
            .set<MeshComponent>({ playerMesh, opaquePipeline });

        auto q = world.query_builder().with<MeshComponent>().build();
        flecs::iter_to_json_desc_t desc{};
        desc.serialize_values = true;
        desc.serialize_table = true;
        json = q.iter().to_json(&desc);

        std::cout << json << std::endl;
    }
    // // 5. Serialisiere die Welt zu JSON
    // flecs::string json = world.to_json();
    // std::cout << json.c_str() << std::endl;
    flecs::world world2;
    
    world2.component<MeshComponent>()
    .member<flecs::entity>("meshId")
    .member<flecs::entity>("pipelineId");
    
    flecs::entity playerMesh = world2.entity("assets::models::player_mesh");
    flecs::entity opaquePipeline = world2.entity("pipelines::opaque_lit");

    std::cout << "Before deserialization:" << std::endl;
    std::cout << "PlayerEntity Mesh ID: " << playerMesh.name() << playerMesh.id() << std::endl;
    std::cout << "PlayerEntity Pipeline ID: " << opaquePipeline.name() << opaquePipeline.id() << std::endl;


    world2.from_json(json);

    std::cout << "After deserialization:" << std::endl;

    const auto& mc = world2.lookup("PlayerEntity").get<MeshComponent>();
    std::cout << "PlayerEntity Mesh ID: " << mc.meshId.name() << mc.meshId.id() << std::endl;
    std::cout << "PlayerEntity Pipeline ID: " << mc.pipelineId.name() << mc.pipelineId.id() << std::endl;

    return 0;
}