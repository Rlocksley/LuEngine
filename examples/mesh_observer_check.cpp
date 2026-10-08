#include "../src/core/Channel.hpp"
#include "../src/core/component/Mesh.hpp"
#include <cassert>

int main(){
    flecs::world world;
    Lu::Module::Mesh module(world);

    auto parent = world.entity().set<Lu::Component::Transform>({});
    auto meshAsset = world.entity();
    auto pipeline = world.entity();
    auto firstChild = world.entity().child_of(parent);

    firstChild.set<Lu::Component::Mesh>({meshAsset, pipeline});
    assert(Lu::GetChannel().drain(10).empty());
    firstChild.set<Lu::Component::Material>({});
    auto requests = Lu::GetChannel().drain(10);
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

    auto grandParent = world.entity().set<Lu::Component::Transform>({});
    auto middle = world.entity().child_of(grandParent);
    auto nonRenderable = world.entity().child_of(middle);
    nonRenderable.set<Lu::Component::Mesh>({meshAsset, pipeline});
    nonRenderable.set<Lu::Component::Material>({});
    assert(Lu::GetChannel().drain(10).empty());
    nonRenderable.remove<Lu::Component::Material>();
    assert(Lu::GetChannel().drain(10).empty());

    parent.remove<Lu::Component::Transform>();
    requests = Lu::GetChannel().drain(10);
    assert(requests.size() == 2);
    for(const auto& request : requests){
        assert(std::holds_alternative<Lu::EcsRequest::DestroyMesh>(request));
    }
}