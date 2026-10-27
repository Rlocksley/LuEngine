#include "Physics.hpp"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <algorithm>
#include <thread>

namespace Lu::Core {

void Physics::init() {
    if (isInitialized()) return;

    if (!::JPH::Factory::sInstance) {
        ::JPH::RegisterDefaultAllocator();
        ::JPH::Factory::sInstance = new ::JPH::Factory();
        ::JPH::RegisterTypes();
    }

    tempAllocator = std::make_unique<::JPH::TempAllocatorImpl>(tempAllocatorSize);
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    jobSystemThreadPool = std::make_unique<::JPH::JobSystemThreadPool>(
        ::JPH::cMaxPhysicsJobs, ::JPH::cMaxPhysicsBarriers, (int)std::max(1u, hw - 1));

    broadPhaseLayerInterface      = std::make_unique<BPLayerInterfaceImpl>();
    objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    objectLayerPairFilter         = std::make_unique<ObjectLayerPairFilterImpl>();

    physicsSystem = std::make_unique<::JPH::PhysicsSystem>();
    physicsSystem->Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
                        *broadPhaseLayerInterface, *objectVsBroadPhaseLayerFilter, *objectLayerPairFilter);

    contactListener = std::make_unique<FlecsContactListener>(bodyMap);
    characterContactListener = std::make_unique<FlecsCharacterContactListener>(bodyMap);
    physicsSystem->SetContactListener(contactListener.get());
}

void Physics::shutdown() {
    if (!isInitialized()) return;
    physicsSystem.reset();
    contactListener.reset();
    characterContactListener.reset();
    jobSystemThreadPool.reset();
    tempAllocator.reset();
    objectLayerPairFilter.reset();
    objectVsBroadPhaseLayerFilter.reset();
    broadPhaseLayerInterface.reset();
    bodyMap.clear();
    ::JPH::UnregisterTypes();
    delete ::JPH::Factory::sInstance;
    ::JPH::Factory::sInstance = nullptr;
}

::JPH::BodyID Physics::createBody(const ::JPH::BodyCreationSettings& settings, flecs::entity_t owner) {
    ::JPH::Body* body = getBodyInterface().CreateBody(settings);
    if (!body) return ::JPH::BodyID();          // body limit reached
    body->SetUserData((::JPH::uint64)owner);
    bodyMap.set(body->GetID(), owner);
    return body->GetID();
}

bool Physics::isInWorld(::JPH::BodyID id) const {
    return physicsSystem && !id.IsInvalid() && physicsSystem->GetBodyInterface().IsAdded(id);
}

void Physics::addBody(::JPH::BodyID id, bool activate) {
    if (!physicsSystem || id.IsInvalid() || isInWorld(id)) return;
    getBodyInterface().AddBody(id, activate ? ::JPH::EActivation::Activate : ::JPH::EActivation::DontActivate);
}

void Physics::removeBody(::JPH::BodyID id) {
    if (!isInWorld(id)) return;
    getBodyInterface().RemoveBody(id);
}

void Physics::destroyBody(::JPH::BodyID id) {
    if (!physicsSystem || id.IsInvalid()) return;
    removeBody(id);
    getBodyInterface().DestroyBody(id);
    bodyMap.erase(id);
}

void Physics::optimizeBroadPhase() {
    if (physicsSystem) physicsSystem->OptimizeBroadPhase();
}

int Physics::step(float dt) {
    if (!physicsSystem) return 0;
    accumulator += std::min(dt, 0.25f);   // avoid spiral of death
    int steps = 0;
    while (accumulator >= fixedTimeStep && steps < maxSubSteps) {
        physicsSystem->Update(fixedTimeStep, 1, tempAllocator.get(), jobSystemThreadPool.get());
        accumulator -= fixedTimeStep;
        ++steps;
    }
    if (steps == maxSubSteps) accumulator = 0.f;
    return steps;
}

} // namespace Lu::Core
