#pragma once

#include "Global.hpp"
#include "Channel.hpp"
#include "Renderer.hpp"
#include "Vertex.hpp"
#include "component/Material.hpp"
#include "component/Mesh.hpp"
#include "component/TransformGpu.hpp"
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

        public:
            void set(Renderer* renderer, flecs::world* world){
                this->renderer = renderer;
                this->world = world;
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
