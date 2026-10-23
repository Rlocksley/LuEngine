#pragma once
#include "Global.hpp"
#include "Constant.hpp"
#include "instance/SkeletonInstance.hpp"

#include <cmath>
#include <variant>

// ============================================================================
//  Animation assets (immutable after loading)
//
//  Everything in here is created while the app is set up (loadSkeletonModel,
//  AnimationGraphBuilder, ...) BEFORE App::run(). While the game runs the ECS
//  worker threads only READ the library, so no locking is needed.
//
//  Performance notes
//   * Clips are baked at import time to a fixed frame rate, one NodePose per
//     skeleton node per frame. Sampling is two array reads + a lerp per node,
//     no keyframe search, no per-channel indirection.
//   * States/params/masks are addressed by small integer ids. Names are only
//     used while building the graph or in convenience overloads.
// ============================================================================

namespace Lu{
    namespace Core{

        // ---- math helpers (also used by the evaluator) ------------------------------------
        inline glm::quat nlerp(const glm::quat& a, const glm::quat& b, float t) {
            const float d = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
            const float s = d < 0.0f ? -1.0f : 1.0f;     // shortest path
            glm::quat r;
            r.x = a.x + (s*b.x - a.x) * t;
            r.y = a.y + (s*b.y - a.y) * t;
            r.z = a.z + (s*b.z - a.z) * t;
            r.w = a.w + (s*b.w - a.w) * t;
            const float len2 = r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w;
            const float inv = len2 > 1e-12f ? 1.0f / std::sqrt(len2) : 1.0f;
            r.x *= inv; r.y *= inv; r.z *= inv; r.w *= inv;
            return r;
        }

        inline void blendPose(NodePose* out, const NodePose* a, const NodePose* b, float t, uint32_t n) {
            for(uint32_t i = 0; i < n; ++i) {
                out[i].rotation = nlerp(a[i].rotation, b[i].rotation, t);
                out[i].position = a[i].position + (b[i].position - a[i].position) * t;
                out[i].scale    = a[i].scale    + (b[i].scale    - a[i].scale)    * t;
            }
        }

        // out[i] = lerp(base[i], top[i], factor * weights[i])  (weights == nullptr -> 1)
        inline void blendPoseMasked(NodePose* out, const NodePose* base, const NodePose* top,
                                    const float* weights, float factor, uint32_t n) {
            for(uint32_t i = 0; i < n; ++i) {
                const float w = (weights ? weights[i] : 1.0f) * factor;
                if(w <= 0.0f) { out[i] = base[i]; continue; }
                if(w >= 1.0f) { out[i] = top[i];  continue; }
                out[i].rotation = nlerp(base[i].rotation, top[i].rotation, w);
                out[i].position = base[i].position + (top[i].position - base[i].position) * w;
                out[i].scale    = base[i].scale    + (top[i].scale    - base[i].scale)    * w;
            }
        }

        // ---- skeleton asset ---------------------------------------------------------------
        struct SkeletonNode {
            std::string name;
            int32_t parent{-1};          // always < own index (parents come first)
            bool isBone{false};          // has an inverse bind matrix (deforms vertices)
            glm::mat4 inverseBind{1.0f};
            NodePose rest{};             // local rest pose
        };

        struct SkeletonAsset {
            std::string name;
            SkeletonAssetId id{UINT32_MAX};
            std::vector<SkeletonNode> nodes;
            std::unordered_map<std::string, uint32_t> nodeByName;

            uint32_t nodeCount() const { return static_cast<uint32_t>(nodes.size()); }

            uint32_t findNode(const std::string& nodeName) const {
                const auto it = nodeByName.find(nodeName);
                return it == nodeByName.end() ? UINT32_MAX : it->second;
            }
        };

        // ---- baked animation clip ---------------------------------------------------------
        using ClipId = uint32_t;

        struct AnimationClip {
            std::string name;
            SkeletonAssetId skeleton{UINT32_MAX};
            float duration{0.0f};        // seconds
            float frameRate{30.0f};      // baked frames per second
            uint32_t frameCount{0};      // frames [0, frameCount): frame k = pose at k / frameRate (clamped to duration)
            uint32_t nodeCount{0};
            std::vector<NodePose> frames; // frameCount * nodeCount

            // time in seconds, any value is clamped to [0, duration]. out has nodeCount poses.
            void sample(float time, NodePose* out) const {
                const float f = std::clamp(time, 0.0f, duration) * frameRate;
                const uint32_t last = frameCount - 1;
                const uint32_t i0 = std::min(static_cast<uint32_t>(f), last);
                const uint32_t i1 = std::min(i0 + 1, last);
                const float a = f - static_cast<float>(i0);
                const NodePose* p0 = frames.data() + static_cast<size_t>(i0) * nodeCount;
                if(i0 == i1 || a <= 1e-4f) {
                    std::memcpy(out, p0, sizeof(NodePose) * nodeCount);
                    return;
                }
                blendPose(out, p0, frames.data() + static_cast<size_t>(i1) * nodeCount, a, nodeCount);
            }
        };

        // ---- masks: per node blend weight (0 = base, 1 = top) -----------------------------
        using MaskId = uint32_t;
        struct AnimationMask {
            std::string name;
            SkeletonAssetId skeleton{UINT32_MAX};
            std::vector<float> weights;  // nodeCount
        };

        // ---- graph nodes (the variant keeps the hot path branch-light and allocation free) --
        using StateId = uint16_t;
        using ParamId = uint8_t;
        inline constexpr StateId kNoState = 0xFFFF;
        inline constexpr uint32_t MAX_ANIMATION_STATES = 16;
        inline constexpr uint32_t MAX_ANIMATION_PARAMS = 8;

        // plays one clip
        struct ClipNode {
            ClipId clip{UINT32_MAX};
            float speed{1.0f};
            bool looping{true};
        };

        // blends the two clips closest to a runtime parameter; phases are synchronised
        struct BlendSpace1DNode {
            static constexpr uint32_t MAX_POINTS = 5;
            uint32_t count{0};
            ClipId clips[MAX_POINTS]{};
            float points[MAX_POINTS]{};  // ascending
            float speeds[MAX_POINTS]{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
            ParamId param{0};
        };

        // Plays `top` on the masked nodes over `base` (blend in/out over time).
        //   action  : mask = upper body, top = non looping clip, onFinish = base state
        //   static  : blendIn = 0, top looping -> permanent masked blend (e.g. aim upper body)
        struct LayerNode {
            StateId top{kNoState};
            StateId base{kNoState};
            MaskId mask{UINT32_MAX};     // UINT32_MAX = whole skeleton
            float blendIn{0.15f};
            float blendOut{0.15f};
            StateId onFinish{kNoState};  // state to enter when `top` finished (kNoState = stay)
        };

        using AnimationNode = std::variant<ClipNode, BlendSpace1DNode, LayerNode>;

        struct AnimationGraph {
            std::string name;
            SkeletonAssetId skeleton{UINT32_MAX};
            std::vector<AnimationNode> nodes;
            std::vector<std::string> nodeNames;
            std::vector<std::string> paramNames;
            StateId start{0};

            StateId findState(const std::string& stateName) const {
                for(size_t i = 0; i < nodeNames.size(); ++i) {
                    if(nodeNames[i] == stateName) return static_cast<StateId>(i);
                }
                return kNoState;
            }
            ParamId findParam(const std::string& paramName, bool* found = nullptr) const {
                for(size_t i = 0; i < paramNames.size(); ++i) {
                    if(paramNames[i] == paramName) { if(found) *found = true; return static_cast<ParamId>(i); }
                }
                if(found) *found = false;
                return 0;
            }
        };
        using GraphId = uint32_t;

        // ---- the library --------------------------------------------------------------------
        class AnimationLibrary {
        public:
            // skeletons
            SkeletonAsset& addSkeleton(SkeletonAsset&& skeleton) {
                LU_ASSERT(!skeletonByName.contains(skeleton.name), "AnimationLibrary", "addSkeleton",
                    "Skeleton already exists: " + skeleton.name)
                skeleton.id = static_cast<SkeletonAssetId>(skeletons.size());
                skeletonByName[skeleton.name] = skeleton.id;
                skeletons.push_back(std::move(skeleton));
                return skeletons.back();
            }
            const SkeletonAsset& skeleton(SkeletonAssetId id) const { return skeletons[id]; }
            const SkeletonAsset* findSkeleton(const std::string& name) const {
                const auto it = skeletonByName.find(name);
                return it == skeletonByName.end() ? nullptr : &skeletons[it->second];
            }

            // clips
            ClipId addClip(AnimationClip&& clip) {
                LU_ASSERT(!clipByName.contains(clip.name), "AnimationLibrary", "addClip",
                    "Clip already exists: " + clip.name)
                const ClipId id = static_cast<ClipId>(clips.size());
                clipByName[clip.name] = id;
                clips.push_back(std::move(clip));
                return id;
            }
            const AnimationClip& clip(ClipId id) const { return clips[id]; }
            ClipId findClip(const std::string& name) const {
                const auto it = clipByName.find(name);
                return it == clipByName.end() ? UINT32_MAX : it->second;
            }

            // masks
            // weight 1 for `rootNode` (and, optionally, everything below it), 0 elsewhere
            MaskId createMaskFromNode(const std::string& maskName, const std::string& skeletonName,
                                      const std::string& rootNode, bool includeChildren = true,
                                      float weight = 1.0f) {
                const SkeletonAsset* sk = findSkeleton(skeletonName);
                LU_ASSERT(sk, "AnimationLibrary", "createMaskFromNode", "Unknown skeleton: " + skeletonName)
                const uint32_t root = sk->findNode(rootNode);
                LU_ASSERT(root != UINT32_MAX, "AnimationLibrary", "createMaskFromNode",
                    "Unknown node '" + rootNode + "' in skeleton " + skeletonName)
                AnimationMask mask;
                mask.name = maskName;
                mask.skeleton = sk->id;
                mask.weights.assign(sk->nodeCount(), 0.0f);
                for(uint32_t i = root; i < sk->nodeCount(); ++i) {      // parents come first
                    if(i == root) { mask.weights[i] = weight; continue; }
                    if(!includeChildren) continue;
                    const int32_t p = sk->nodes[i].parent;
                    if(p >= 0 && mask.weights[static_cast<uint32_t>(p)] > 0.0f) mask.weights[i] = weight;
                }
                return addMask(std::move(mask));
            }

            MaskId addMask(AnimationMask&& mask) {
                const MaskId id = static_cast<MaskId>(masks.size());
                maskByName[mask.name] = id;
                masks.push_back(std::move(mask));
                return id;
            }
            const AnimationMask& mask(MaskId id) const { return masks[id]; }
            MaskId findMask(const std::string& name) const {
                const auto it = maskByName.find(name);
                return it == maskByName.end() ? UINT32_MAX : it->second;
            }

            // graphs
            GraphId addGraph(AnimationGraph&& graph) {
                LU_ASSERT(!graphByName.contains(graph.name), "AnimationLibrary", "addGraph",
                    "Graph already exists: " + graph.name)
                const GraphId id = static_cast<GraphId>(graphs.size());
                graphByName[graph.name] = id;
                graphs.push_back(std::move(graph));
                return id;
            }
            const AnimationGraph& graph(GraphId id) const { return graphs[id]; }
            GraphId findGraph(const std::string& name) const {
                const auto it = graphByName.find(name);
                return it == graphByName.end() ? UINT32_MAX : it->second;
            }

        private:
            std::vector<SkeletonAsset> skeletons;
            std::vector<AnimationClip> clips;
            std::vector<AnimationMask> masks;
            std::vector<AnimationGraph> graphs;
            std::unordered_map<std::string, SkeletonAssetId> skeletonByName;
            std::unordered_map<std::string, ClipId> clipByName;
            std::unordered_map<std::string, MaskId> maskByName;
            std::unordered_map<std::string, GraphId> graphByName;
        };

        inline AnimationLibrary& GetAnimationLibrary() {
            static AnimationLibrary library;
            return library;
        }
    }
}
