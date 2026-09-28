#include "../src/core/Channel.hpp"
#include "../src/core/component/Mesh.hpp"
#include <cassert>

int main(){
    flecs::world world;
    world.import<Lu::Module::Transform>();
    world.import<Lu::Module::TransformGpu>();
    world.import<Lu::Module::Material>();
    world.import<Lu::Module::Mesh>();

    auto parent = world.entity().set<Lu::Component::Transform>({}).add<Lu::Component::TransformGpu>();
    auto requests = Lu::GetChannel().drain(10);
    std::cout << "Requests1: " << requests.size() << std::endl; 
    assert(requests.size() == 1);


    auto meshAsset = world.entity();
    auto pipeline = world.entity();
    auto firstChild = world.entity().child_of(parent);

    firstChild.set<Lu::Component::Mesh>({meshAsset, pipeline});
    assert(Lu::GetChannel().drain(10).empty());
    firstChild.set<Lu::Component::Material>({});
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests1.1: " << requests.size() << std::endl;
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[0]));

    firstChild.set<Lu::Component::Material>({});
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[0]));

    firstChild.remove<Lu::Component::Material>();
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[0]));
    firstChild.set<Lu::Component::Material>({});
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[0]));
    firstChild.remove<Lu::Component::Mesh>();
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[0]));
    firstChild.set<Lu::Component::Mesh>({meshAsset, pipeline});
    firstChild.set<Lu::Component::Material>({});
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 2);
    for(const auto& request : requests){
        assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(request));
    }

    auto secondChild = world.entity().child_of(parent);
    secondChild.set<Lu::Component::Material>({});
    assert(Lu::GetChannel().drain(10).empty());
    secondChild.set<Lu::Component::Mesh>({meshAsset, pipeline});
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[0]));

    auto deletedChild = world.entity().child_of(parent);
    deletedChild.set<Lu::Component::Mesh>({meshAsset, pipeline});
    deletedChild.set<Lu::Component::Material>({});
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 1);
    deletedChild.destruct();
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[0]));

    auto grandParent = world.entity().add<Lu::Component::TransformGpu>().set<Lu::Component::Transform>({});
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests2: " << requests.size() << std::endl; 
    assert(requests.size() == 1);
    auto middle = world.entity().child_of(grandParent).set<Lu::Component::Transform>({}).add<Lu::Component::TransformGpu>();
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests2.1: " << requests.size() << std::endl; 
    assert(requests.size() == 1);
    auto nonRenderable = world.entity().child_of(middle);
    nonRenderable.set<Lu::Component::Mesh>({meshAsset, pipeline});
    nonRenderable.set<Lu::Component::Material>({});
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests2.2: " << requests.size() << std::endl; 
    assert(requests.size() == 1);
    
    nonRenderable.remove<Lu::Component::Material>();
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests2.3: " << requests.size() << std::endl; 
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[0]));
    
    nonRenderable.set<Lu::Component::Material>({});
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests2.4: " << requests.size() << std::endl; 
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[0]));
    
    middle.set<Lu::Component::Transform>({});
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests2.5: " << requests.size() << std::endl; 
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::CreateTransform>(requests[0]));

    middle.remove<Lu::Component::Transform>();
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests2.6: " << requests.size() << std::endl; 
    assert(requests.size() == 2);
    assert(std::holds_alternative<Lu::EcsRequest::DestroyTransform>(requests[0]));
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[1]));
    
    parent.set<Lu::Component::Transform>({});
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests3: " << requests.size() << std::endl; 
    assert(requests.size() == 1);
    assert(std::holds_alternative<Lu::EcsRequest::CreateTransform>(requests[0]));
    
    
    parent.remove<Lu::Component::Transform>();
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests3.1: " << requests.size() << std::endl; 
    assert(requests.size() == 3);
    assert(std::holds_alternative<Lu::EcsRequest::DestroyTransform>(requests[0]));
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[1]));
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[2]));
    
    parent.set<Lu::Component::Transform>({});
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests3: " << requests.size() << std::endl; 
    assert(requests.size() == 3);
    assert(std::holds_alternative<Lu::EcsRequest::CreateTransform>(requests[0]));
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[1]));
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[2]));

    parent.remove<Lu::Component::TransformGpu>();
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests3.1: " << requests.size() << std::endl; 
    assert(requests.size() == 3);
    assert(std::holds_alternative<Lu::EcsRequest::DestroyTransform>(requests[0]));
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[1]));
    assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(requests[2]));
    
    parent.add<Lu::Component::TransformGpu>();
    requests = Lu::GetChannel().drain(10);
    std::cout << "Requests3: " << requests.size() << std::endl; 
    assert(requests.size() == 3);
    assert(std::holds_alternative<Lu::EcsRequest::CreateTransform>(requests[0]));
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[1]));
    assert(std::holds_alternative<Lu::EcsRequest::CreateMesh>(requests[2]));


    std::cout << "No Asserts " << std::endl << "Everything OK" << std::endl;
}