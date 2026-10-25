#pragma once

#include "Global.hpp"
#include "Vertex.hpp"

// 2D shapes for Mesh2D (GUI). Register them with App::registerShape2D<Shape>(name, args...).
//
// Conventions (same as Transform2D / Text): units are pixels, x points right, y points down, the origin of the
// geometry is the position of the Transform2D of the parent entity. Rectangles are described by their top-left corner
// and size, round shapes by their center. UVs map the bounding box of the shape to [0,1]x[0,1] (top-left = (0,0)).
// All triangles are clockwise on the screen, so the default pipeline settings (back face culling) work.
namespace Lu{
    namespace Shape2D{

        // Rectangle from `offset` (top-left) to `offset + size`.
        struct Rectangle{
            Rectangle(const glm::vec2 size, const glm::vec2 offset = glm::vec2(0.f), const glm::vec4 color = glm::vec4(1.f));

            const std::vector<Vertex::Vertex2D>& getVertices() const { return vertices; }
            const std::vector<uint32_t>& getIndices() const { return indices; }

        private:
            std::vector<Vertex::Vertex2D> vertices;
            std::vector<uint32_t> indices;
        };

        // Rectangle with rounded corners (real geometry, no shader needed). `radius` is clamped to half of the shorter side,
        // cornerSegments is the number of segments of each corner arc.
        struct RoundedRectangle{
            RoundedRectangle(const glm::vec2 size, const float radius, const uint32_t cornerSegments = 8,
                             const glm::vec2 offset = glm::vec2(0.f), const glm::vec4 color = glm::vec4(1.f));

            const std::vector<Vertex::Vertex2D>& getVertices() const { return vertices; }
            const std::vector<uint32_t>& getIndices() const { return indices; }

        private:
            std::vector<Vertex::Vertex2D> vertices;
            std::vector<uint32_t> indices;
        };

        // Filled circle (triangle fan) around `center`.
        struct Circle{
            Circle(const float radius, const uint32_t segments = 48,
                   const glm::vec2 center = glm::vec2(0.f), const glm::vec4 color = glm::vec4(1.f));

            const std::vector<Vertex::Vertex2D>& getVertices() const { return vertices; }
            const std::vector<uint32_t>& getIndices() const { return indices; }

        private:
            std::vector<Vertex::Vertex2D> vertices;
            std::vector<uint32_t> indices;
        };

        // Ring (annulus) between innerRadius and outerRadius around `center`, e.g. for outlines and radio buttons.
        struct Ring{
            Ring(const float outerRadius, const float innerRadius, const uint32_t segments = 48,
                 const glm::vec2 center = glm::vec2(0.f), const glm::vec4 color = glm::vec4(1.f));

            const std::vector<Vertex::Vertex2D>& getVertices() const { return vertices; }
            const std::vector<uint32_t>& getIndices() const { return indices; }

        private:
            std::vector<Vertex::Vertex2D> vertices;
            std::vector<uint32_t> indices;
        };
    }
}
