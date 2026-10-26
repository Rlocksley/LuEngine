#pragma once

#include "Global.hpp"
#include "Vertex.hpp"
#define PI 3.14159265359

namespace Lu{
    namespace Shape{

    struct Cube{

                Cube(const glm::vec3 halfSize, const glm::vec3 offset = glm::vec3(0,0,0), const glm::vec4 color = glm::vec4(1,1,1,1));

            private:
                std::vector<Vertex::Mesh> vertices;
            
                std::vector<uint32_t> indices = {
                    // Front face
                    0, 1, 2, 2, 3, 0,
                    // Back face
                    4, 7, 6, 6, 5, 4,
                    // Left face
                    8, 10, 11, 9, 10, 8,
                    // Right face
                    12, 14, 13, 15, 14, 12,
                    // Top face
                    16, 17, 18, 18, 19, 16,
                    // Bottom face
                    20, 23, 22, 22, 21, 20
                };

            public:
                const std::vector<Vertex::Mesh>& getVertices(){
                    return vertices;
                }

                const std::vector<uint32_t>& getIndices(){
                    return indices;
                }
    };


    struct Sphere{
        private:
        std::vector<Vertex::Mesh> vertices;
        std::vector<uint32_t> indices;

        public:
        Sphere(float radius, int latitudes, int longitudes, glm::vec3 offset = glm::vec3(0,0,0), glm::vec4 color = glm::vec4(1,1,1,1), bool invertNormals = false);
        private:
        
        void GenerateSphereSmooth(float radius, int latitudes, int longitudes, glm::vec3 offset, glm::vec4 color);
    public:

        const std::vector<Vertex::Mesh>& getVertices(){
            return vertices;
        }

        const std::vector<uint32_t>& getIndices(){
            return indices;
        }

    };

    // Flat XY-plane quad facing +Z, intended for billboard particle systems.
    // Vertices span [-halfSize, +halfSize] in X and Y with full [0,1]x[0,1] UVs.
    struct Square {
        Square(float halfSize = 0.5f, glm::vec3 offset = glm::vec3(0.0f), glm::vec4 color = glm::vec4(1.0f));

    private:
        std::vector<Vertex::Mesh> vertices;

        std::vector<uint32_t> indices = {
            0, 1, 2,
            2, 3, 0
        };

    public:
        const std::vector<Vertex::Mesh>& getVertices() { return vertices; }
        const std::vector<uint32_t>& getIndices()      { return indices;  }
    };

    // Pitched triangular-prism roof (ridge along Z axis at +halfSize.y, base at -halfSize.y).
    struct GableRoof {
        GableRoof(const glm::vec3 halfSize, const glm::vec3 offset = glm::vec3(0.0f), const glm::vec4 color = glm::vec4(1.0f));

    private:
        std::vector<Vertex::Mesh> vertices;
        std::vector<uint32_t> indices;

    public:
        const std::vector<Vertex::Mesh>& getVertices() { return vertices; }
        const std::vector<uint32_t>& getIndices()      { return indices;  }
    };

    // Four-sided pyramid / hip roof (apex at (0, +halfSize.y, 0), base at -halfSize.y).
    struct Pyramid {
        Pyramid(const glm::vec3 halfSize, const glm::vec3 offset = glm::vec3(0.0f), const glm::vec4 color = glm::vec4(1.0f));

    private:
        std::vector<Vertex::Mesh> vertices;
        std::vector<uint32_t> indices;

    public:
        const std::vector<Vertex::Mesh>& getVertices() { return vertices; }
        const std::vector<uint32_t>& getIndices()      { return indices;  }
    };

    // Vertical cylinder along Y axis spanning [-halfHeight, +halfHeight].
    struct Cylinder {
        Cylinder(float radius, float halfHeight, int segments = 24, const glm::vec3 offset = glm::vec3(0.0f), const glm::vec4 color = glm::vec4(1.0f));

    private:
        std::vector<Vertex::Mesh> vertices;
        std::vector<uint32_t> indices;

    public:
        const std::vector<Vertex::Mesh>& getVertices() { return vertices; }
        const std::vector<uint32_t>& getIndices()      { return indices;  }
    };

    // Vertical cone along Y axis with base at -halfHeight and apex at +halfHeight.
    struct Cone {
        Cone(float radius, float halfHeight, int segments = 24, const glm::vec3 offset = glm::vec3(0.0f), const glm::vec4 color = glm::vec4(1.0f));

    private:
        std::vector<Vertex::Mesh> vertices;
        std::vector<uint32_t> indices;

    public:
        const std::vector<Vertex::Mesh>& getVertices() { return vertices; }
        const std::vector<uint32_t>& getIndices()      { return indices;  }
    };
    
    }
}