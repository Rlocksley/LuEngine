#pragma once

#include "Global.hpp"
#include "flecs.h"
#include "Constant.hpp"
#include "AnimationLibrary.hpp"
#include "TripleBuffers.hpp"
#include "component/Transform.hpp"
#include "component/Skeleton.hpp"

// ============================================================================
//  AnimationStateMachine
//
//  USAGE
//    // 1. load the model: registers skeleton asset, baked clips ("Hero::Idle", ...) and a prefab
//    app.loadSkeletonModel("Hero", "asset/models/hero.fbx");
//
//    // 2. describe the graph ONCE (shared by every instance)
//    Lu::Core::GetAnimationLibrary().createMaskFromNode("Hero::UpperBody", "Hero", "mixamorig:Spine1");
//    Lu::AnimationGraphBuilder("HeroGraph", "Hero")
//        .clip("Idle", "Idle")
//        .clip("Walk", "Walk")
//        .clip("Run",  "Run")
//        .blend1D("Locomotion", "speed", {{"Idle", 0.0f}, {"Walk", 0.5f}, {"Run", 1.0f}})
//        .clip("Slash", "Slash", /*looping*/ false)
//        .layer("Attack", /*top*/ "Slash", /*base*/ "Locomotion", "Hero::UpperBody",
//               /*blendIn*/ 0.1f, /*blendOut*/ 0.15f, /*onFinish*/ "Locomotion")
//        .start("Locomotion")
//        .build();
//
//    // 3. instantiate: the ECS system evaluates it every frame, in parallel
//    world.entity().is_a(world.lookup("Skeleton::Hero"))
//         .set<Lu::Component::Transform>({...})
//         .set<Lu::Component::AnimationStateMachine>(
//              Lu::Component::AnimationStateMachine::create("HeroGraph"));
//
//    // 4. drive it from your own gameplay systems / controllers
//    asm_.setParam(speedId, capsule.moveSpeed / capsule.maxRunSpeed);   // ParamId cached
//    asm_.crossFade(attackId, 0.1f);
// ============================================================================

namespace Lu{
    namespace Component{

        namespace AnimationDetail {

            // LIFO pose scratch memory, one per thread: evaluating a state tree never
            // touches the heap.
            struct PoseScratch {
                static constexpr uint32_t kPoses = 24 * Core::MAX_BONES_PER_SKELETON;
                std::unique_ptr<Core::NodePose[]> data{new Core::NodePose[kPoses]};
                uint32_t top{0};

                Core::NodePose* alloc(uint32_t n) {
                    LU_ASSERT(top + n <= kPoses, "AnimationStateMachine", "PoseScratch",
                        "Animation graph is nested too deeply.")
                    Core::NodePose* p = data.get() + top;
                    top += n;
                    return p;
                }
                void release(uint32_t n) { top -= n; }
            };

            inline PoseScratch& scratch() {
                thread_local PoseScratch s;
                return s;
            }
        }

        struct AnimationStateMachine {
            // per node runtime values (the graph itself is shared and immutable)
            struct Runtime {
                float time{0.0f};      // clip: seconds | blend space: phase in [0,1) | layer: seconds since entered
                float duration{0.0f};  // seconds of one cycle at the current speed/blend
                uint8_t finished{0};   // one shot reached its end
                uint8_t oneShot{0};    // node ends by itself (non looping clip)
                uint8_t _pad[2]{};
            };

            Core::GraphId graph{UINT32_MAX};
            Core::StateId current{0};
            Core::StateId target{Core::kNoState};   // cross fade target
            float fadeTime{0.0f};
            float fadeDuration{0.0f};
            float speed{1.0f};                       // global playback speed
            float params[Core::MAX_ANIMATION_PARAMS]{};
            Runtime runtime[Core::MAX_ANIMATION_STATES]{};

            // ---- construction -----------------------------------------------------------
            static AnimationStateMachine create(const std::string& graphName) {
                const Core::GraphId id = Core::GetAnimationLibrary().findGraph(graphName);
                LU_ASSERT(id != UINT32_MAX, "AnimationStateMachine", "create", "Unknown animation graph: " + graphName)
                AnimationStateMachine m;
                m.graph = id;
                m.current = Core::GetAnimationLibrary().graph(id).start;
                return m;
            }

            // ---- lookups (cache the ids, do not call the string overloads every frame) ---
            Core::StateId findState(const std::string& name) const {
                const Core::StateId id = library().graph(graph).findState(name);
                LU_ASSERT(id != Core::kNoState, "AnimationStateMachine", "findState", "Unknown state: " + name)
                return id;
            }
            Core::ParamId findParam(const std::string& name) const {
                bool found = false;
                const Core::ParamId id = library().graph(graph).findParam(name, &found);
                LU_ASSERT(found, "AnimationStateMachine", "findParam", "Unknown parameter: " + name)
                return id;
            }

            // ---- control -------------------------------------------------------------------
            void setParam(Core::ParamId id, float value) { params[id] = value; }
            void setParam(const std::string& name, float value) { params[findParam(name)] = value; }
            float getParam(Core::ParamId id) const { return params[id]; }

            // hard cut: restarts the state
            void setState(Core::StateId state, bool restart = true) {
                LU_ASSERT(state < library().graph(graph).nodes.size(), "AnimationStateMachine", "setState", "Invalid state id.")
                if(restart) resetNode(state);
                current = state;
                target = Core::kNoState;
                fadeTime = fadeDuration = 0.0f;
            }
            void setState(const std::string& name) { setState(findState(name)); }

            // cross fade from the current pose into `state`
            void crossFade(Core::StateId state, float duration = 0.25f) {
                LU_ASSERT(state < library().graph(graph).nodes.size(), "AnimationStateMachine", "crossFade", "Invalid state id.")
                if(state == current && target == Core::kNoState) return;
                if(state == target) return;
                if(target != Core::kNoState) current = target;   // finish the running fade instantly
                resetNode(state);
                target = state;
                fadeTime = 0.0f;
                fadeDuration = std::max(duration, 0.0f);
            }
            void crossFade(const std::string& name, float duration = 0.25f) { crossFade(findState(name), duration); }

            // ---- queries ---------------------------------------------------------------------
            Core::StateId activeState() const { return target != Core::kNoState ? target : current; }
            bool isFading() const { return target != Core::kNoState; }
            bool isFinished(Core::StateId state) const { return runtime[state].finished != 0; }
            float stateTime(Core::StateId state) const { return runtime[state].time; }

            // ---- evaluation (called by the ECS system, also usable manually) ---------------
            // Writes nodeCount poses to `out` and advances all playheads by dt.
            void update(float dt, Core::NodePose* out) {
                const Core::AnimationLibrary& lib = library();
                const Core::AnimationGraph& g = lib.graph(graph);
                const uint32_t n = lib.skeleton(g.skeleton).nodeCount();
                auto& s = AnimationDetail::scratch();
                const uint32_t mark = s.top;

                Evaluator ev{lib, g, *this, n, dt, s};
                if(target == Core::kNoState) {
                    ev.eval(current, out);
                } else {
                    Core::NodePose* a = s.alloc(n);
                    Core::NodePose* b = s.alloc(n);
                    ev.eval(current, a);
                    ev.eval(target, b);
                    const float t = fadeDuration > 1e-6f ? std::min(1.0f, fadeTime / fadeDuration) : 1.0f;
                    Core::blendPose(out, a, b, t, n);
                    fadeTime += dt;
                    if(fadeTime >= fadeDuration) {
                        current = target;
                        target = Core::kNoState;
                    }
                }
                s.top = mark;

                // one shot layer finished -> continue with its follow up state without restarting it
                if(const auto* layer = std::get_if<Core::LayerNode>(&g.nodes[current])) {
                    if(runtime[current].finished && layer->onFinish != Core::kNoState) {
                        setState(layer->onFinish, false);
                    }
                }
            }

        private:
            static const Core::AnimationLibrary& library() { return Core::GetAnimationLibrary(); }

            void resetNode(Core::StateId id) {
                runtime[id] = Runtime{};
                if(const auto* layer = std::get_if<Core::LayerNode>(&library().graph(graph).nodes[id])) {
                    if(layer->top != Core::kNoState) resetNode(layer->top);
                }
            }

            struct Evaluator {
                const Core::AnimationLibrary& lib;
                const Core::AnimationGraph& g;
                AnimationStateMachine& m;
                uint32_t n;
                float dt;
                AnimationDetail::PoseScratch& s;

                void eval(Core::StateId id, Core::NodePose* out) {
                    std::visit(variant_match{
                        [&](const Core::ClipNode& node)       { evalClip(id, node, out); },
                        [&](const Core::BlendSpace1DNode& node){ evalBlend(id, node, out); },
                        [&](const Core::LayerNode& node)      { evalLayer(id, node, out); }
                    }, g.nodes[id]);
                }

                void evalClip(Core::StateId id, const Core::ClipNode& node, Core::NodePose* out) {
                    const Core::AnimationClip& clip = lib.clip(node.clip);
                    Runtime& rt = m.runtime[id];
                    clip.sample(rt.time, out);

                    const float speed = std::max(std::abs(node.speed * m.speed), 1e-4f);
                    rt.duration = clip.duration / speed;
                    rt.oneShot = node.looping ? 0 : 1;

                    rt.time += dt * node.speed * m.speed;
                    if(node.looping) {
                        if(clip.duration > 1e-6f) {
                            rt.time = std::fmod(rt.time, clip.duration);
                            if(rt.time < 0.0f) rt.time += clip.duration;
                        }
                    } else if(rt.time >= clip.duration) {
                        rt.time = clip.duration;
                        rt.finished = 1;
                    }
                }

                void evalBlend(Core::StateId id, const Core::BlendSpace1DNode& node, Core::NodePose* out) {
                    Runtime& rt = m.runtime[id];
                    const uint32_t count = node.count;
                    const float p = m.params[node.param];

                    uint32_t i0 = 0, i1 = 0;
                    float f = 0.0f;
                    if(count > 1) {
                        if(p <= node.points[0]) {
                            i0 = i1 = 0;
                        } else if(p >= node.points[count - 1]) {
                            i0 = i1 = count - 1;
                        } else {
                            while(i0 + 2 < count && p >= node.points[i0 + 1]) ++i0;
                            i1 = i0 + 1;
                            const float range = node.points[i1] - node.points[i0];
                            f = range > 1e-6f ? (p - node.points[i0]) / range : 0.0f;
                        }
                    }

                    const Core::AnimationClip& c0 = lib.clip(node.clips[i0]);
                    const Core::AnimationClip& c1 = lib.clip(node.clips[i1]);
                    // phase is normalised so clips of different length stay foot-synchronised
                    c0.sample(rt.time * c0.duration, out);
                    if(i1 != i0 && f > 1e-4f) {
                        Core::NodePose* tmp = s.alloc(n);
                        c1.sample(rt.time * c1.duration, tmp);
                        Core::blendPose(out, out, tmp, f, n);
                        s.release(n);
                    }

                    const float d0 = c0.duration / std::max(std::abs(node.speeds[i0] * m.speed), 1e-4f);
                    const float d1 = c1.duration / std::max(std::abs(node.speeds[i1] * m.speed), 1e-4f);
                    rt.duration = d0 + (d1 - d0) * f;
                    rt.oneShot = 0;
                    if(rt.duration > 1e-6f) {
                        rt.time = std::fmod(rt.time + dt / rt.duration, 1.0f);
                    }
                }

                void evalLayer(Core::StateId id, const Core::LayerNode& node, Core::NodePose* out) {
                    Runtime& rt = m.runtime[id];
                    Core::NodePose* top = s.alloc(n);
                    eval(node.top, top);
                    const Runtime& topRt = m.runtime[node.top];

                    float factor = 1.0f;
                    if(node.blendIn > 1e-6f && rt.time < node.blendIn) {
                        factor = rt.time / node.blendIn;
                    }
                    if(topRt.oneShot && node.blendOut > 1e-6f) {
                        const float remaining = oneShotRemaining(node.top);
                        if(remaining < node.blendOut) factor = std::min(factor, remaining / node.blendOut);
                    }
                    factor = std::clamp(factor, 0.0f, 1.0f);

                    if(node.base != Core::kNoState) {
                        Core::NodePose* base = s.alloc(n);
                        eval(node.base, base);
                        const float* weights = node.mask != UINT32_MAX ? lib.mask(node.mask).weights.data() : nullptr;
                        Core::blendPoseMasked(out, base, top, weights, factor, n);
                        s.release(n);
                    } else {
                        std::memcpy(out, top, sizeof(Core::NodePose) * n);
                    }
                    s.release(n);

                    rt.time += dt;
                    rt.duration = topRt.duration;
                    rt.oneShot = topRt.oneShot;
                    rt.finished = topRt.finished;
                }

                // seconds until a one shot node ends
                float oneShotRemaining(Core::StateId id) const {
                    if(const auto* clip = std::get_if<Core::ClipNode>(&g.nodes[id])) {
                        const float dur = lib.clip(clip->clip).duration;
                        const float speed = std::max(std::abs(clip->speed * m.speed), 1e-4f);
                        return (dur - m.runtime[id].time) / speed;
                    }
                    return std::numeric_limits<float>::max();
                }
            };
        };

        // Fluent builder for the shared graph asset.
        class AnimationGraphBuilder {
        public:
            struct Point {
                std::string clip;
                float value{0.0f};
                float speed{1.0f};
            };

            AnimationGraphBuilder(const std::string& graphName, const std::string& skeletonName) {
                const Core::SkeletonAsset* sk = Core::GetAnimationLibrary().findSkeleton(skeletonName);
                LU_ASSERT(sk, "AnimationGraphBuilder", "AnimationGraphBuilder", "Unknown skeleton: " + skeletonName)
                skeletonPrefix = skeletonName + "::";
                graph.name = graphName;
                graph.skeleton = sk->id;
            }

            AnimationGraphBuilder& clip(const std::string& state, const std::string& clipName,
                                        bool looping = true, float speed = 1.0f) {
                Core::ClipNode node;
                node.clip = resolveClip(clipName);
                node.looping = looping;
                node.speed = speed;
                return add(state, node);
            }

            AnimationGraphBuilder& blend1D(const std::string& state, const std::string& param,
                                           std::vector<Point> points) {
                LU_ASSERT(!points.empty() && points.size() <= Core::BlendSpace1DNode::MAX_POINTS,
                    "AnimationGraphBuilder", "blend1D", "A blend space needs 1.." +
                    std::to_string(Core::BlendSpace1DNode::MAX_POINTS) + " points: " + state)
                std::sort(points.begin(), points.end(), [](const Point& a, const Point& b){ return a.value < b.value; });
                Core::BlendSpace1DNode node;
                node.count = static_cast<uint32_t>(points.size());
                node.param = paramId(param);
                for(uint32_t i = 0; i < node.count; ++i) {
                    node.clips[i] = resolveClip(points[i].clip);
                    node.points[i] = points[i].value;
                    node.speeds[i] = points[i].speed;
                }
                return add(state, node);
            }

            // top / base / onFinish are state names; they may be defined later in the chain.
            AnimationGraphBuilder& layer(const std::string& state, const std::string& top, const std::string& base,
                                         const std::string& maskName = "", float blendIn = 0.15f,
                                         float blendOut = 0.15f, const std::string& onFinish = "") {
                Core::LayerNode node;
                node.blendIn = blendIn;
                node.blendOut = blendOut;
                if(!maskName.empty()) {
                    node.mask = Core::GetAnimationLibrary().findMask(maskName);
                    LU_ASSERT(node.mask != UINT32_MAX, "AnimationGraphBuilder", "layer", "Unknown mask: " + maskName)
                }
                pendingLayers.push_back({static_cast<uint32_t>(graph.nodes.size()), top, base, onFinish});
                return add(state, node);
            }

            AnimationGraphBuilder& start(const std::string& state) { startName = state; return *this; }

            Core::GraphId build() {
                LU_ASSERT(!graph.nodes.empty(), "AnimationGraphBuilder", "build", "Graph has no states: " + graph.name)
                for(const PendingLayer& pl : pendingLayers) {
                    auto& node = std::get<Core::LayerNode>(graph.nodes[pl.index]);
                    node.top = stateId(pl.top);
                    node.base = pl.base.empty() ? Core::kNoState : stateId(pl.base);
                    node.onFinish = pl.onFinish.empty() ? Core::kNoState : stateId(pl.onFinish);
                }
                graph.start = startName.empty() ? 0 : stateId(startName);
                return Core::GetAnimationLibrary().addGraph(std::move(graph));
            }

        private:
            struct PendingLayer { uint32_t index; std::string top, base, onFinish; };

            Core::AnimationGraph graph;
            std::string skeletonPrefix;
            std::string startName;
            std::vector<PendingLayer> pendingLayers;

            template<typename Node>
            AnimationGraphBuilder& add(const std::string& state, const Node& node) {
                LU_ASSERT(graph.nodes.size() < Core::MAX_ANIMATION_STATES, "AnimationGraphBuilder", "add",
                    "Too many states (MAX_ANIMATION_STATES) in graph " + graph.name)
                LU_ASSERT(graph.findState(state) == Core::kNoState, "AnimationGraphBuilder", "add",
                    "Duplicate state: " + state)
                graph.nodes.emplace_back(node);
                graph.nodeNames.push_back(state);
                return *this;
            }

            Core::StateId stateId(const std::string& name) const {
                const Core::StateId id = graph.findState(name);
                LU_ASSERT(id != Core::kNoState, "AnimationGraphBuilder", "build", "Unknown state: " + name)
                return id;
            }

            Core::ParamId paramId(const std::string& name) {
                bool found = false;
                const Core::ParamId id = graph.findParam(name, &found);
                if(found) return id;
                LU_ASSERT(graph.paramNames.size() < Core::MAX_ANIMATION_PARAMS, "AnimationGraphBuilder", "param",
                    "Too many parameters (MAX_ANIMATION_PARAMS) in graph " + graph.name)
                graph.paramNames.push_back(name);
                return static_cast<Core::ParamId>(graph.paramNames.size() - 1);
            }

            Core::ClipId resolveClip(const std::string& clipName) const {
                const Core::AnimationLibrary& lib = Core::GetAnimationLibrary();
                Core::ClipId id = lib.findClip(clipName);
                if(id == UINT32_MAX) id = lib.findClip(skeletonPrefix + clipName);
                LU_ASSERT(id != UINT32_MAX, "AnimationGraphBuilder", "clip", "Unknown clip: " + clipName)
                LU_ASSERT(lib.clip(id).skeleton == graph.skeleton, "AnimationGraphBuilder", "clip",
                    "Clip belongs to another skeleton: " + clipName)
                return id;
            }
        };
    }

    using Component::AnimationGraphBuilder;

    namespace Module{
        struct AnimationStateMachine{
            AnimationStateMachine(flecs::world& world){
                world.component<Component::AnimationStateMachine>();

                // Poses of all animated skeletons of one ECS frame form ONE snapshot of the
                // pose triple buffer: begin (reset cursors) -> evaluate (parallel) -> publish.
                world.system("SkeletonPoseBegin")
                    .kind(flecs::OnValidate)
                    .run([](flecs::iter&){
                        auto& snap = Core::GetSkeletonPoseTripleBuffer().write_slot();
                        snap.headerCursor.store(0, std::memory_order_relaxed);
                        snap.poseCursor.store(0, std::memory_order_relaxed);
                    });

                // Entities tagged Static keep their rest pose (uploaded once on creation).
                world.system<Component::AnimationStateMachine, const Component::Skeleton>("AnimationStateMachineUpdate")
                    .without<Component::Static>()
                    .kind(flecs::PostUpdate)
                    .multi_threaded()
                    .each([](flecs::iter& it, size_t i, Component::AnimationStateMachine& machine, const Component::Skeleton&){
                        if(machine.graph == UINT32_MAX) return;

                        const Core::AnimationLibrary& lib = Core::GetAnimationLibrary();
                        const uint32_t nodeCount = lib.skeleton(lib.graph(machine.graph).skeleton).nodeCount();

                        auto& snap = Core::GetSkeletonPoseTripleBuffer().write_slot();
                        const uint32_t firstPose = snap.poseCursor.fetch_add(nodeCount, std::memory_order_relaxed);
                        if(firstPose + nodeCount > snap.poses.size()) return;          // capacity: skip this frame
                        const uint32_t header = snap.headerCursor.fetch_add(1, std::memory_order_relaxed);
                        if(header >= snap.headers.size()) return;

                        machine.update(it.delta_time(), snap.poses.data() + firstPose);
                        snap.headers[header] = EcsRequest::UpdateSkeletonPose{it.entity(i).id(), firstPose, nodeCount};
                    });

                world.system("SkeletonPosePublish")
                    .kind(flecs::PreStore)
                    .run([](flecs::iter&){
                        auto& buffer = Core::GetSkeletonPoseTripleBuffer();
                        auto& snap = buffer.write_slot();
                        snap.headerCount = std::min<uint32_t>(snap.headerCursor.load(std::memory_order_relaxed),
                                                              static_cast<uint32_t>(snap.headers.size()));
                        snap.poseCount = std::min<uint32_t>(snap.poseCursor.load(std::memory_order_relaxed),
                                                            static_cast<uint32_t>(snap.poses.size()));
                        buffer.publish();
                    });
            }
        };
    }
}
