#pragma once

#include "Global.hpp"
#include "Channel.hpp"
#include "Renderer.hpp"
#include "Vertex.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/TransformGpu.hpp"
#include "component/Skeleton.hpp"
#include "AnimationLibrary.hpp"
#include <cmath>
#include <unordered_set>
#include "flecs.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <limits>
#include <sstream>

#include "stb/stb_image.h"

namespace Lu{
    namespace Core{

        class AssetManager{
            Renderer* renderer{nullptr};
            flecs::world* world{nullptr};

            template<class VertexType>
            static void correctTriangleWinding(
                const std::vector<VertexType>& vertices,
                std::vector<uint32_t>& indices) {
                for(size_t i = 0; i + 2 < indices.size(); i += 3) {
                    std::swap(indices[i + 1], indices[i + 2]);
                }
            }
        

            flecs::entity loadMaterialTexture(const std::string& modelName,
                const std::string& filePath, const aiScene& scene,
                const aiMaterial& material, aiTextureType textureType,
                uint32_t textureIndex,
                std::unordered_map<std::string, flecs::entity>& loadedTextures) {
                aiString assimpPath;
                const aiReturn textureResult = material.GetTexture(
                    textureType, 0, &assimpPath);
                if(textureResult != AI_SUCCESS) {
                    return {};
                }

                const std::string texturePath = assimpPath.C_Str();
                const bool isDataTexture = textureType == aiTextureType_NORMALS ||
                    textureType == aiTextureType_DIFFUSE_ROUGHNESS ||
                    textureType == aiTextureType_METALNESS;
                const VkFormat textureFormat = isDataTexture
                    ? VK_FORMAT_R8G8B8A8_UNORM
                    : VK_FORMAT_R8G8B8A8_SRGB;
                const aiTexture* embeddedTexture = scene.GetEmbeddedTexture(texturePath.c_str());
                std::string cacheKey;
                std::string resourcePath;
                if(embeddedTexture) {
                    resourcePath = texturePath;
                } else {
                    std::filesystem::path resolvedPath(texturePath);
                    if(resolvedPath.is_relative()) {
                        resolvedPath = std::filesystem::path(filePath).parent_path() / resolvedPath;
                    }
                    resolvedPath = resolvedPath.lexically_normal();
                    resourcePath = resolvedPath.string();
                }
                cacheKey = resourcePath + (isDataTexture ? "#linear" : "#srgb");

                const auto existingTexture = loadedTextures.find(cacheKey);
                if(existingTexture != loadedTextures.end()) {
                    return existingTexture->second;
                }

                const std::string textureName =
                    modelName + "Texture_" + std::to_string(textureIndex);
                LU_ASSERT(!world->lookup(textureName.c_str()),
                    "AssetManager", "loadModel",
                    "Texture entity already exists: " + textureName)

                const flecs::entity textureEntity = world->entity(textureName.c_str());
                if(embeddedTexture) {
                    std::shared_ptr<uint8_t[]> pixels;
                    uint32_t width = embeddedTexture->mWidth;
                    uint32_t height = embeddedTexture->mHeight;

                    if(height == 0) {
                        LU_ASSERT(width <= static_cast<uint32_t>(std::numeric_limits<int>::max()),
                            "AssetManager", "loadModel",
                            "Embedded texture is too large to decode: " + texturePath)
                        int decodedWidth = 0;
                        int decodedHeight = 0;
                        int channels = 0;
                        uint8_t* decodedPixels = stbi_load_from_memory(
                            reinterpret_cast<const stbi_uc*>(embeddedTexture->pcData),
                            static_cast<int>(width),
                            &decodedWidth,
                            &decodedHeight,
                            &channels,
                            STBI_rgb_alpha);
                        LU_ASSERT(decodedPixels && decodedWidth > 0 && decodedHeight > 0,
                            "AssetManager", "loadModel",
                            "Failed to decode embedded texture: " + texturePath)
                        width = static_cast<uint32_t>(decodedWidth);
                        height = static_cast<uint32_t>(decodedHeight);
                        pixels = std::shared_ptr<uint8_t[]>(decodedPixels,
                            [](uint8_t* data) { stbi_image_free(data); });
                    } else {
                        const size_t pixelCount =
                            static_cast<size_t>(width) * static_cast<size_t>(height);
                        LU_ASSERT(width > 0 && height > 0 &&
                            pixelCount <= std::numeric_limits<size_t>::max() / 4,
                            "AssetManager", "loadModel",
                            "Embedded texture has invalid dimensions: " + texturePath)
                        pixels = std::shared_ptr<uint8_t[]>(new uint8_t[pixelCount * 4]);
                        for(size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
                            const aiTexel& texel = embeddedTexture->pcData[pixelIndex];
                            pixels[pixelIndex * 4] = texel.r;
                            pixels[pixelIndex * 4 + 1] = texel.g;
                            pixels[pixelIndex * 4 + 2] = texel.b;
                            pixels[pixelIndex * 4 + 3] = texel.a;
                        }
                    }
                    TextureCpu cpuTexture(width, height, std::move(pixels));
                    cpuTexture.vkFormat = textureFormat;
                    renderer->createTexture(textureEntity.id(), std::move(cpuTexture));
                } else {
                    TextureCpu cpuTexture(resourcePath);
                    cpuTexture.vkFormat = textureFormat;
                    renderer->createTexture(textureEntity.id(), std::move(cpuTexture));
                }

                loadedTextures.emplace(cacheKey, textureEntity);
                return textureEntity;
            }


            // ------------------------------------------------------------------------------
            //  Skeleton models
            // ------------------------------------------------------------------------------
            static glm::mat4 toGlm(const aiMatrix4x4& m) {
                glm::mat4 r;   // assimp is row major, glm column major: r[column][row]
                r[0][0] = m.a1; r[1][0] = m.a2; r[2][0] = m.a3; r[3][0] = m.a4;
                r[0][1] = m.b1; r[1][1] = m.b2; r[2][1] = m.b3; r[3][1] = m.b4;
                r[0][2] = m.c1; r[1][2] = m.c2; r[2][2] = m.c3; r[3][2] = m.c4;
                r[0][3] = m.d1; r[1][3] = m.d2; r[2][3] = m.d3; r[3][3] = m.d4;
                return r;
            }

            static NodePose toNodePose(const aiMatrix4x4& m) {
                aiVector3D scaling, position;
                aiQuaternion rotation;
                m.Decompose(scaling, rotation, position);
                return NodePose::fromTRS(
                    glm::vec3(position.x, position.y, position.z),
                    glm::quat(rotation.w, rotation.x, rotation.y, rotation.z),
                    glm::vec3(scaling.x, scaling.y, scaling.z));
            }

            // Pose of one node channel at `tick` (assimp ticks).
            static NodePose sampleChannel(const aiNodeAnim& ch, double tick, const NodePose& rest) {
                NodePose pose = rest;
                if(ch.mNumPositionKeys > 0) {
                    uint32_t i = 0;
                    while(i + 1 < ch.mNumPositionKeys && ch.mPositionKeys[i + 1].mTime <= tick) ++i;
                    glm::vec3 v(ch.mPositionKeys[i].mValue.x, ch.mPositionKeys[i].mValue.y, ch.mPositionKeys[i].mValue.z);
                    if(i + 1 < ch.mNumPositionKeys) {
                        const double span = ch.mPositionKeys[i + 1].mTime - ch.mPositionKeys[i].mTime;
                        const float a = span > 1e-9 ? static_cast<float>((tick - ch.mPositionKeys[i].mTime) / span) : 0.0f;
                        const auto& n = ch.mPositionKeys[i + 1].mValue;
                        v = glm::mix(v, glm::vec3(n.x, n.y, n.z), std::clamp(a, 0.0f, 1.0f));
                    }
                    pose.position = glm::vec4(v, 0.0f);
                }
                if(ch.mNumRotationKeys > 0) {
                    uint32_t i = 0;
                    while(i + 1 < ch.mNumRotationKeys && ch.mRotationKeys[i + 1].mTime <= tick) ++i;
                    const auto& q0 = ch.mRotationKeys[i].mValue;
                    glm::quat q(q0.w, q0.x, q0.y, q0.z);
                    if(i + 1 < ch.mNumRotationKeys) {
                        const double span = ch.mRotationKeys[i + 1].mTime - ch.mRotationKeys[i].mTime;
                        const float a = span > 1e-9 ? static_cast<float>((tick - ch.mRotationKeys[i].mTime) / span) : 0.0f;
                        const auto& q1 = ch.mRotationKeys[i + 1].mValue;
                        q = glm::slerp(q, glm::quat(q1.w, q1.x, q1.y, q1.z), std::clamp(a, 0.0f, 1.0f));
                    }
                    pose.rotation = glm::normalize(q);
                }
                if(ch.mNumScalingKeys > 0) {
                    uint32_t i = 0;
                    while(i + 1 < ch.mNumScalingKeys && ch.mScalingKeys[i + 1].mTime <= tick) ++i;
                    glm::vec3 v(ch.mScalingKeys[i].mValue.x, ch.mScalingKeys[i].mValue.y, ch.mScalingKeys[i].mValue.z);
                    if(i + 1 < ch.mNumScalingKeys) {
                        const double span = ch.mScalingKeys[i + 1].mTime - ch.mScalingKeys[i].mTime;
                        const float a = span > 1e-9 ? static_cast<float>((tick - ch.mScalingKeys[i].mTime) / span) : 0.0f;
                        const auto& n = ch.mScalingKeys[i + 1].mValue;
                        v = glm::mix(v, glm::vec3(n.x, n.y, n.z), std::clamp(a, 0.0f, 1.0f));
                    }
                    pose.scale = glm::vec4(v, 0.0f);
                }
                return pose;
            }

            Component::Material buildMaterial(const std::string& prefabName, const std::string& filePath,
                const aiScene& scene, const aiMesh& sourceMesh,
                std::unordered_map<std::string, flecs::entity>& loadedTextures) {
                Component::Material material{};
                material.roughness = 0.5f;
                material.ambient = glm::vec4(0.08f, 0.08f, 0.08f, 1.0f);
                if(sourceMesh.mMaterialIndex >= scene.mNumMaterials) return material;

                const aiMaterial& src = *scene.mMaterials[sourceMesh.mMaterialIndex];
                aiColor4D diffuse(1.0f, 1.0f, 1.0f, 1.0f);
                if(src.Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                    material.albedo = glm::vec4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
                }
                aiColor4D ambient(0.0f, 0.0f, 0.0f, 1.0f);
                if(src.Get(AI_MATKEY_COLOR_AMBIENT, ambient) == AI_SUCCESS) {
                    material.ambient = glm::vec4(
                        glm::max(glm::vec3(ambient.r, ambient.g, ambient.b), glm::vec3(0.08f)), 1.0f);
                }
                aiColor4D emission(0.0f, 0.0f, 0.0f, 1.0f);
                if(src.Get(AI_MATKEY_COLOR_EMISSIVE, emission) == AI_SUCCESS) {
                    material.emission = glm::vec4(emission.r, emission.g, emission.b, emission.a);
                }
                src.Get("$mat.roughnessFactor", 0, 0, material.roughness);
                src.Get("$mat.metallicFactor", 0, 0, material.metallic);

                auto tex = [&](aiTextureType type) {
                    return loadMaterialTexture(prefabName, filePath, scene, src, type,
                        static_cast<uint32_t>(loadedTextures.size()), loadedTextures);
                };
                flecs::entity albedo = tex(aiTextureType_BASE_COLOR);
                if(!albedo) albedo = tex(aiTextureType_DIFFUSE);
                material.textures[0] = albedo;
                material.textures[1] = tex(aiTextureType_NORMALS);
                material.textures[2] = tex(aiTextureType_DIFFUSE_ROUGHNESS);
                material.textures[3] = tex(aiTextureType_METALNESS);
                material.textures[4] = tex(aiTextureType_EMISSIVE);
                return material;
            }

        public:
            // Imports a skinned model:
            //  * skeleton asset (node hierarchy, inverse bind matrices, rest pose)  -> AnimationLibrary + renderer
            //  * every animation baked to `bakeFrameRate` fps, registered as clip "<name>::<animation>"
            //  * skeleton geometry (Vertex::Skeleton) per mesh
            //  * a prefab "<prefix><name>":  Transform-ready parent with Skeleton + TransformGpu and one
            //    child per mesh with SkeletonMesh + Material.  Meshes without bones are attached to the
            //    node that owns them (nodeIndex), skinned meshes are deformed by their bone weights.
            // Call before App::run(). Create the AnimationGraph afterwards (AnimationGraphBuilder).
            void loadSkeletonModel(const std::string& name, const std::string& filePath,
                const std::string& prefix = "Skeleton::",
                const std::string& pipelineName = "SkeletonPipe::Simple",
                const float bakeFrameRate = 30.0f) {
                LU_ASSERT(renderer && world, "AssetManager", "loadSkeletonModel",
                    "AssetManager has not been initialized by an App.")
                LU_ASSERT(!name.empty() && !filePath.empty() && bakeFrameRate > 0.0f,
                    "AssetManager", "loadSkeletonModel", "Invalid arguments.")

                Assimp::Importer importer;
                const aiScene* scene = importer.ReadFile(
                    filePath,
                    aiProcess_Triangulate |
                    aiProcess_GenSmoothNormals |
                    aiProcess_JoinIdenticalVertices |
                    aiProcess_LimitBoneWeights |
                    aiProcess_ImproveCacheLocality |
                    aiProcess_SortByPType |
                    aiProcess_ConvertToLeftHanded);
                LU_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode,
                    "AssetManager", "loadSkeletonModel",
                    "Failed to import model '" + filePath + "': " + importer.GetErrorString())
                LU_ASSERT(scene->mNumMeshes > 0, "AssetManager", "loadSkeletonModel", "Model contains no meshes: " + filePath)

                const std::string prefabName = prefix + name;
                LU_ASSERT(!world->lookup(prefabName.c_str()), "AssetManager", "loadSkeletonModel",
                    "Prefab already exists: " + prefabName)
                const flecs::entity pipeline = world->lookup(pipelineName.c_str());
                LU_ASSERT(pipeline, "AssetManager", "loadSkeletonModel", "Skeleton pipeline entity not found: " + pipelineName)

                // ---- 1. flatten the node tree (parents before children) --------------------------
                struct FlatNode { const aiNode* node; int parent; };
                std::vector<FlatNode> flat;
                std::function<void(const aiNode*, int)> flatten = [&](const aiNode* node, int parent) {
                    const int index = static_cast<int>(flat.size());
                    flat.push_back({node, parent});
                    for(uint32_t c = 0; c < node->mNumChildren; ++c) flatten(node->mChildren[c], index);
                };
                flatten(scene->mRootNode, -1);

                // ---- 2. which nodes does the skeleton need? bones, rigid-mesh owners, their ancestors
                std::unordered_set<std::string> boneNames;
                std::vector<bool> meshHasBones(scene->mNumMeshes, false);
                for(uint32_t m = 0; m < scene->mNumMeshes; ++m) {
                    meshHasBones[m] = scene->mMeshes[m]->HasBones();
                    for(uint32_t b = 0; b < scene->mMeshes[m]->mNumBones; ++b) {
                        boneNames.insert(scene->mMeshes[m]->mBones[b]->mName.C_Str());
                    }
                }
                LU_ASSERT(!boneNames.empty(), "AssetManager", "loadSkeletonModel",
                    "Model has no bones (use loadModel for static meshes): " + filePath)

                std::vector<bool> needed(flat.size(), false);
                for(size_t i = 0; i < flat.size(); ++i) {
                    const aiNode* node = flat[i].node;
                    if(boneNames.contains(node->mName.C_Str())) needed[i] = true;
                    for(uint32_t k = 0; k < node->mNumMeshes; ++k) {
                        if(!meshHasBones[node->mMeshes[k]]) needed[i] = true;
                    }
                }
                needed[0] = true;
                for(size_t i = flat.size(); i-- > 0;) {
                    if(needed[i] && flat[i].parent >= 0) needed[static_cast<size_t>(flat[i].parent)] = true;
                }

                // ---- 3. skeleton asset -----------------------------------------------------------
                SkeletonAsset asset;
                asset.name = name;
                std::vector<int> compactOf(flat.size(), -1);
                for(size_t i = 0; i < flat.size(); ++i) {
                    if(!needed[i]) continue;
                    SkeletonNode node;
                    node.name = flat[i].node->mName.C_Str();
                    node.parent = flat[i].parent >= 0 ? compactOf[static_cast<size_t>(flat[i].parent)] : -1;
                    node.rest = toNodePose(flat[i].node->mTransformation);
                    compactOf[i] = static_cast<int>(asset.nodes.size());
                    asset.nodeByName[node.name] = static_cast<uint32_t>(asset.nodes.size());
                    asset.nodes.push_back(std::move(node));
                }
                LU_ASSERT(asset.nodes.size() <= MAX_BONES_PER_SKELETON, "AssetManager", "loadSkeletonModel",
                    "Skeleton '" + name + "' needs " + std::to_string(asset.nodes.size()) +
                    " nodes, MAX_BONES_PER_SKELETON is " + std::to_string(MAX_BONES_PER_SKELETON))
                for(uint32_t m = 0; m < scene->mNumMeshes; ++m) {
                    for(uint32_t b = 0; b < scene->mMeshes[m]->mNumBones; ++b) {
                        const aiBone& bone = *scene->mMeshes[m]->mBones[b];
                        SkeletonNode& node = asset.nodes[asset.nodeByName.at(bone.mName.C_Str())];
                        node.isBone = true;
                        node.inverseBind = toGlm(bone.mOffsetMatrix);
                    }
                }

                // ---- 4. register in the library and the renderer ---------------------------------
                AnimationLibrary& library = GetAnimationLibrary();
                const SkeletonAssetId skeletonId = library.addSkeleton(std::move(asset)).id;
                const SkeletonAsset& registered = library.skeleton(skeletonId);
                const flecs::entity assetEntity = world->entity(("SkeletonAsset::" + name).c_str());
                renderer->createSkeletonAsset(assetEntity.id(), registered);

                // ---- 5. bake animations ------------------------------------------------------------
                const uint32_t nodeCount = registered.nodeCount();
                for(uint32_t a = 0; a < scene->mNumAnimations; ++a) {
                    const aiAnimation& anim = *scene->mAnimations[a];
                    std::string animName = anim.mName.length > 0 ? anim.mName.C_Str() : "Animation" + std::to_string(a);
                    std::string clipName = name + "::" + animName;
                    if(library.findClip(clipName) != UINT32_MAX) clipName += "_" + std::to_string(a);

                    const double tps = anim.mTicksPerSecond > 0.0 ? anim.mTicksPerSecond : 25.0;
                    AnimationClip clip;
                    clip.name = clipName;
                    clip.skeleton = skeletonId;
                    clip.duration = static_cast<float>(anim.mDuration / tps);
                    clip.frameRate = bakeFrameRate;
                    clip.nodeCount = nodeCount;
                    clip.frameCount = static_cast<uint32_t>(std::ceil(clip.duration * bakeFrameRate)) + 1;

                    std::vector<const aiNodeAnim*> channels(nodeCount, nullptr);
                    for(uint32_t c = 0; c < anim.mNumChannels; ++c) {
                        const uint32_t node = registered.findNode(anim.mChannels[c]->mNodeName.C_Str());
                        if(node != UINT32_MAX) channels[node] = anim.mChannels[c];
                    }

                    clip.frames.resize(static_cast<size_t>(clip.frameCount) * nodeCount);
                    for(uint32_t f = 0; f < clip.frameCount; ++f) {
                        const double seconds = std::min(static_cast<double>(f) / bakeFrameRate,
                                                        static_cast<double>(clip.duration));
                        for(uint32_t n = 0; n < nodeCount; ++n) {
                            const NodePose& rest = registered.nodes[n].rest;
                            clip.frames[static_cast<size_t>(f) * nodeCount + n] =
                                channels[n] ? sampleChannel(*channels[n], seconds * tps, rest) : rest;
                        }
                    }
                    library.addClip(std::move(clip));
                }

                // ---- 6. geometry + prefab -------------------------------------------------------------
                const flecs::entity prefab = world->entity(prefabName.c_str())
                    .add(flecs::Prefab)
                    .add<Component::TransformGpu>()
                    .set<Component::Skeleton>({assetEntity});

                std::unordered_map<std::string, flecs::entity> loadedTextures;
                std::vector<flecs::entity> geometryOf(scene->mNumMeshes);

                auto createGeometry = [&](uint32_t meshIndex) -> flecs::entity {
                    if(geometryOf[meshIndex]) return geometryOf[meshIndex];
                    const aiMesh& src = *scene->mMeshes[meshIndex];
                    const std::string meshName = src.mName.length > 0 ? src.mName.C_Str() : "Mesh";
                    const std::string geometryName = prefabName + "Geometry_" + meshName + "_" + std::to_string(meshIndex);
                    LU_ASSERT(!world->lookup(geometryName.c_str()), "AssetManager", "loadSkeletonModel",
                        "Geometry entity already exists: " + geometryName)

                    struct Influence { uint32_t bone; float weight; };
                    std::vector<std::array<Influence, 4>> influences(src.mNumVertices, std::array<Influence, 4>{});
                    std::vector<uint8_t> influenceCount(src.mNumVertices, 0);
                    for(uint32_t b = 0; b < src.mNumBones; ++b) {
                        const aiBone& bone = *src.mBones[b];
                        const uint32_t node = registered.findNode(bone.mName.C_Str());
                        for(uint32_t w = 0; w < bone.mNumWeights; ++w) {
                            const uint32_t v = bone.mWeights[w].mVertexId;
                            const float weight = bone.mWeights[w].mWeight;
                            auto& list = influences[v];
                            uint8_t& count = influenceCount[v];
                            if(count < 4) {
                                list[count++] = {node, weight};
                            } else {      // keep the 4 strongest
                                uint32_t weakest = 0;
                                for(uint32_t k = 1; k < 4; ++k) if(list[k].weight < list[weakest].weight) weakest = k;
                                if(weight > list[weakest].weight) list[weakest] = {node, weight};
                            }
                        }
                    }

                    std::vector<Vertex::Skeleton> vertices;
                    vertices.reserve(src.mNumVertices);
                    for(uint32_t v = 0; v < src.mNumVertices; ++v) {
                        const aiVector3D& p = src.mVertices[v];
                        const aiVector3D n = src.HasNormals() ? src.mNormals[v] : aiVector3D(0.0f, 1.0f, 0.0f);
                        const aiColor4D c = src.HasVertexColors(0) ? src.mColors[0][v] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
                        const aiVector3D t = src.HasTextureCoords(0) ? src.mTextureCoords[0][v] : aiVector3D(0.0f);
                        Vertex::Skeleton vertex{
                            glm::vec3(p.x, p.y, p.z), glm::vec3(n.x, n.y, n.z),
                            glm::vec4(c.r, c.g, c.b, c.a), glm::vec2(t.x, t.y)};
                        float sum = 0.0f;
                        for(uint32_t k = 0; k < influenceCount[v]; ++k) sum += influences[v][k].weight;
                        for(uint32_t k = 0; k < influenceCount[v]; ++k) {
                            vertex.bones[static_cast<int>(k)] = static_cast<int>(influences[v][k].bone);
                            vertex.weights[static_cast<int>(k)] = sum > 1e-6f ? influences[v][k].weight / sum : 0.0f;
                        }
                        vertices.push_back(vertex);
                    }

                    std::vector<uint32_t> indices;
                    indices.reserve(static_cast<size_t>(src.mNumFaces) * 3);
                    for(uint32_t f = 0; f < src.mNumFaces; ++f) {
                        LU_ASSERT(src.mFaces[f].mNumIndices == 3, "AssetManager", "loadSkeletonModel",
                            "Assimp did not triangulate mesh: " + meshName)
                        for(uint32_t k = 0; k < 3; ++k) indices.push_back(src.mFaces[f].mIndices[k]);
                    }
                    LU_ASSERT(!vertices.empty() && !indices.empty(), "AssetManager", "loadSkeletonModel",
                        "Imported mesh has no triangle geometry: " + meshName)
                    correctTriangleWinding(vertices, indices);

                    const flecs::entity geometry = world->entity(geometryName.c_str());
                    renderer->createSkeletonGeometry(geometry.id(), vertices, indices);
                    geometryOf[meshIndex] = geometry;
                    return geometry;
                };

                auto createChild = [&](uint32_t meshIndex, uint32_t nodeIndex, const std::string& suffix) {
                    const flecs::entity geometry = createGeometry(meshIndex);
                    const Component::Material material = buildMaterial(prefabName, filePath, *scene,
                        *scene->mMeshes[meshIndex], loadedTextures);
                    const std::string childName = prefabName + "::" + suffix;
                    LU_ASSERT(!world->lookup(childName.c_str()), "AssetManager", "loadSkeletonModel",
                        "Entity already exists: " + childName)
                    world->entity(childName.c_str())
                        .child_of(prefab)
                        .set<Component::SkeletonMesh>({geometry, pipeline, nodeIndex})
                        .set<Component::Material>(material);
                };

                std::vector<bool> placed(scene->mNumMeshes, false);
                for(size_t i = 0; i < flat.size(); ++i) {
                    const aiNode* node = flat[i].node;
                    for(uint32_t k = 0; k < node->mNumMeshes; ++k) {
                        const uint32_t m = node->mMeshes[k];
                        const std::string suffix = std::string(scene->mMeshes[m]->mName.C_Str()) + "_" + std::to_string(i) + "_" + std::to_string(m);
                        if(meshHasBones[m]) {
                            if(placed[m]) continue;                         // skinned: one entity per mesh
                            placed[m] = true;
                            createChild(m, UINT32_MAX, suffix);
                        } else {                                            // rigid: attach to the owning node
                            placed[m] = true;
                            createChild(m, static_cast<uint32_t>(compactOf[i]), suffix);
                        }
                    }
                }
                for(uint32_t m = 0; m < scene->mNumMeshes; ++m) {           // meshes no node references
                    if(!placed[m]) createChild(m, meshHasBones[m] ? UINT32_MAX : 0u, std::string(scene->mMeshes[m]->mName.C_Str()) + "_free_" + std::to_string(m));
                }
            }

            // Node index of a skeleton asset by name (for SkeletonMesh::nodeIndex attachments).
            static uint32_t skeletonNode(const std::string& skeletonName, const std::string& nodeName) {
                const SkeletonAsset* sk = GetAnimationLibrary().findSkeleton(skeletonName);
                LU_ASSERT(sk, "AssetManager", "skeletonNode", "Unknown skeleton: " + skeletonName)
                const uint32_t node = sk->findNode(nodeName);
                LU_ASSERT(node != UINT32_MAX, "AssetManager", "skeletonNode", "Unknown node: " + nodeName)
                return node;
            }

            void set(Renderer* renderer, flecs::world* world){
                this->renderer = renderer;
                this->world = world;
            }

            // Registration helpers for modules (flecs modules only receive the world, not the App).
            // Like App::registerMeshPipe / App::registerMesh they are NOT thread safe: call them while
            // the modules are imported, i.e. before App::run() starts the ECS and render threads.
            flecs::entity registerMeshPipe(const GraphicsPipelineConfig& config) {
                LU_ASSERT(renderer && world,
                    "AssetManager", "registerMeshPipe",
                    "AssetManager has not been initialized by an App.")
                LU_ASSERT(!config.name.empty() && config.capacity > 0,
                    "AssetManager", "registerMeshPipe",
                    "GraphicsPipelineConfig needs a name and a capacity bigger 0.")
                const flecs::entity pipe = world->entity(config.name.c_str());
                renderer->createMeshPipe(pipe.id(), config);
                return pipe;
            }

            flecs::entity registerMeshGeometry(const std::string& name,
                const std::vector<Vertex::Mesh>& vertices, const std::vector<uint32_t>& indices) {
                LU_ASSERT(renderer && world,
                    "AssetManager", "registerMeshGeometry",
                    "AssetManager has not been initialized by an App.")
                LU_ASSERT(!name.empty(),
                    "AssetManager", "registerMeshGeometry", "name must not be empty.")
                const flecs::entity geometry = world->entity(name.c_str());
                renderer->createMeshGeometry(geometry.id(), vertices, indices);
                return geometry;
            }

            void loadModel(const std::string& name, const std::string& filePath,
                const std::string& prefix = "Mesh::",
                const std::string& pipelineName = "MeshPipe::Simple") {
                LU_ASSERT(renderer && world,
                    "AssetManager", "loadModel",
                    "AssetManager has not been initialized by an App.")
                LU_ASSERT(!name.empty() && !filePath.empty(),
                    "AssetManager", "loadModel",
                    "Model name and file path must not be empty.")

                Assimp::Importer importer;
                const aiScene* scene = importer.ReadFile(
                    filePath,
                    aiProcess_Triangulate |
                    aiProcess_GenSmoothNormals |
                    aiProcess_JoinIdenticalVertices |
                    aiProcess_ImproveCacheLocality |
                    aiProcess_SortByPType |
                    aiProcess_ConvertToLeftHanded);
                LU_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) &&
                    scene->mRootNode,
                    "AssetManager", "loadModel",
                    "Failed to import model '" + filePath + "': " + importer.GetErrorString())
                LU_ASSERT(scene->mNumMeshes > 0,
                    "AssetManager", "loadModel",
                    "Model contains no meshes: " + filePath)

                const std::string prefabName = prefix + name;
                LU_ASSERT(!world->lookup(prefabName.c_str()),
                    "AssetManager", "loadModel",
                    "Model prefab entity already exists: " + prefabName)
                const flecs::entity pipeline = world->lookup(pipelineName.c_str());
                LU_ASSERT(pipeline,
                    "AssetManager", "loadModel",
                    "Mesh pipeline entity not found: " + pipelineName)

                struct MeshOccurrence {
                    const aiMesh* mesh;
                    aiMatrix4x4 transform;
                };
                std::vector<MeshOccurrence> meshOccurrences;
                std::vector<bool> referencedMeshes(scene->mNumMeshes, false);
                std::function<void(const aiNode*, const aiMatrix4x4&)> collectMeshes;
                collectMeshes = [&](const aiNode* node, const aiMatrix4x4& parentTransform) {
                    const aiMatrix4x4 nodeTransform = parentTransform * node->mTransformation;
                    for(uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
                        const uint32_t sceneMeshIndex = node->mMeshes[meshIndex];
                        LU_ASSERT(sceneMeshIndex < scene->mNumMeshes,
                            "AssetManager", "loadModel",
                            "Node references a mesh outside the imported scene.")
                        meshOccurrences.push_back({
                            scene->mMeshes[sceneMeshIndex],
                            nodeTransform
                        });
                        referencedMeshes[sceneMeshIndex] = true;
                    }
                    for(uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
                        collectMeshes(node->mChildren[childIndex], nodeTransform);
                    }
                };
                collectMeshes(scene->mRootNode, aiMatrix4x4());
                for(uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
                    if(!referencedMeshes[meshIndex]) {
                        meshOccurrences.push_back({
                            scene->mMeshes[meshIndex],
                            aiMatrix4x4()
                        });
                    }
                }
                LU_ASSERT(!meshOccurrences.empty(),
                    "AssetManager", "loadModel",
                    "Model contains no mesh instances: " + filePath)

                const flecs::entity prefab = world->entity(prefabName.c_str())
                    .add(flecs::Prefab)
                    .add<Component::TransformGpu>();

                std::unordered_map<std::string, flecs::entity> loadedTextures;
                for(size_t occurrenceIndex = 0;
                    occurrenceIndex < meshOccurrences.size(); ++occurrenceIndex) {
                    const MeshOccurrence& occurrence = meshOccurrences[occurrenceIndex];
                    const aiMesh& sourceMesh = *occurrence.mesh;
                    const std::string assimpMeshName = sourceMesh.mName.length > 0
                        ? sourceMesh.mName.C_Str()
                        : "Mesh";
                    std::ostringstream suffix;
                    suffix << assimpMeshName << "_" << occurrenceIndex;

                    const std::string geometryName =
                        prefabName + "Geometry_" + suffix.str();
                    LU_ASSERT(!world->lookup(geometryName.c_str()),
                        "AssetManager", "loadModel",
                        "Mesh geometry entity already exists: " + geometryName)

                    std::vector<Vertex::Mesh> vertices;
                    vertices.reserve(sourceMesh.mNumVertices);
                    aiMatrix3x3 normalTransform(occurrence.transform);
                    normalTransform.Inverse().Transpose();
                    for(uint32_t vertexIndex = 0;
                        vertexIndex < sourceMesh.mNumVertices; ++vertexIndex) {
                        const aiVector3D position =
                            occurrence.transform * sourceMesh.mVertices[vertexIndex];
                        aiVector3D normal(0.0f, 1.0f, 0.0f);
                        if(sourceMesh.HasNormals()) {
                            normal = normalTransform * sourceMesh.mNormals[vertexIndex];
                            normal.Normalize();
                        }

                        const aiColor4D color = sourceMesh.HasVertexColors(0)
                            ? sourceMesh.mColors[0][vertexIndex]
                            : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
                        const aiVector3D texCoord = sourceMesh.HasTextureCoords(0)
                            ? sourceMesh.mTextureCoords[0][vertexIndex]
                            : aiVector3D(0.0f, 0.0f, 0.0f);
                        vertices.push_back({
                            glm::vec3(position.x, position.y, position.z),
                            glm::vec3(normal.x, normal.y, normal.z),
                            glm::vec4(color.r, color.g, color.b, color.a),
                            glm::vec2(texCoord.x, texCoord.y)
                        });
                    }

                    std::vector<uint32_t> indices;
                    indices.reserve(static_cast<size_t>(sourceMesh.mNumFaces) * 3);
                    for(uint32_t faceIndex = 0; faceIndex < sourceMesh.mNumFaces; ++faceIndex) {
                        const aiFace& face = sourceMesh.mFaces[faceIndex];
                        LU_ASSERT(face.mNumIndices == 3,
                            "AssetManager", "loadModel",
                            "Assimp did not triangulate mesh: " + assimpMeshName)
                        for(uint32_t index = 0; index < face.mNumIndices; ++index) {
                            indices.push_back(face.mIndices[index]);
                        }
                    }
                    LU_ASSERT(!vertices.empty() && !indices.empty(),
                        "AssetManager", "loadModel",
                        "Imported mesh has no triangle geometry: " + assimpMeshName)
                    correctTriangleWinding(vertices, indices);

                    const flecs::entity geometryEntity = world->entity(geometryName.c_str());
                    renderer->createMeshGeometry(
                        geometryEntity.id(), vertices, indices);

                    Component::Material material{};
                    material.roughness = 0.5f;
                    material.ambient = glm::vec4(0.08f, 0.08f, 0.08f, 1.0f);
                    if(sourceMesh.mMaterialIndex < scene->mNumMaterials) {
                        const aiMaterial& sourceMaterial =
                            *scene->mMaterials[sourceMesh.mMaterialIndex];
                        aiColor4D diffuse(1.0f, 1.0f, 1.0f, 1.0f);
                        if(sourceMaterial.Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                            material.albedo = glm::vec4(
                                diffuse.r, diffuse.g, diffuse.b, diffuse.a);
                        }
                        aiColor4D ambient(0.0f, 0.0f, 0.0f, 1.0f);
                        if(sourceMaterial.Get(AI_MATKEY_COLOR_AMBIENT, ambient) == AI_SUCCESS) {
                            material.ambient = glm::vec4(
                                glm::max(glm::vec3(ambient.r, ambient.g, ambient.b),
                                    glm::vec3(0.08f)),
                                1.0f);
                        }
                        aiColor4D emission(0.0f, 0.0f, 0.0f, 1.0f);
                        if(sourceMaterial.Get(AI_MATKEY_COLOR_EMISSIVE, emission) == AI_SUCCESS) {
                            material.emission = glm::vec4(
                                emission.r, emission.g, emission.b, emission.a);
                        }
                        sourceMaterial.Get("$mat.roughnessFactor", 0, 0, material.roughness);
                        sourceMaterial.Get("$mat.metallicFactor", 0, 0, material.metallic);

                        const flecs::entity albedoTexture = loadMaterialTexture(
                            prefabName,
                            filePath,
                            *scene,
                            sourceMaterial,
                            aiTextureType_BASE_COLOR,
                            static_cast<uint32_t>(loadedTextures.size()),
                            loadedTextures);
                        flecs::entity diffuseTexture;
                        if(!albedoTexture) {
                            diffuseTexture = loadMaterialTexture(
                                prefabName,
                                filePath,
                                *scene,
                                sourceMaterial,
                                aiTextureType_DIFFUSE,
                                static_cast<uint32_t>(loadedTextures.size()),
                                loadedTextures);
                        }
                        material.textures[0] = albedoTexture
                            ? albedoTexture
                            : diffuseTexture;

                        // Keep the CPU/GPU slots in material-property order:
                        // albedo, normal, roughness, metallic, emission.
                        material.textures[1] = loadMaterialTexture(
                            prefabName, filePath, *scene, sourceMaterial,
                            aiTextureType_NORMALS,
                            static_cast<uint32_t>(loadedTextures.size()), loadedTextures);
                        material.textures[2] = loadMaterialTexture(
                            prefabName, filePath, *scene, sourceMaterial,
                            aiTextureType_DIFFUSE_ROUGHNESS,
                            static_cast<uint32_t>(loadedTextures.size()), loadedTextures);
                        material.textures[3] = loadMaterialTexture(
                            prefabName, filePath, *scene, sourceMaterial,
                            aiTextureType_METALNESS,
                            static_cast<uint32_t>(loadedTextures.size()), loadedTextures);
                        material.textures[4] = loadMaterialTexture(
                            prefabName, filePath, *scene, sourceMaterial,
                            aiTextureType_EMISSIVE,
                            static_cast<uint32_t>(loadedTextures.size()), loadedTextures);
                    }

                    const std::string childName =
                        prefabName + "::" + suffix.str();
                    LU_ASSERT(!world->lookup(childName.c_str()),
                        "AssetManager", "loadModel",
                        "Model mesh entity already exists: " + childName)
                    world->entity(childName.c_str())
                        .child_of(prefab)
                        .set<Component::Mesh>({
                            .mesh = geometryEntity,
                            .pipeline = pipeline
                        })
                        .set<Component::Material>(material);
                }
            }
        };

        inline AssetManager& GetAssetManager(){
            static AssetManager assetManager;
            return assetManager;
        }
    }
}
