#include "Shape2D.hpp"

#include <algorithm>
#include <cmath>

namespace Lu{
    namespace Shape2D{

        namespace{
            constexpr float TWO_PI = 6.28318530717958647692f;
            constexpr float HALF_PI = 1.57079632679489661923f;

            // uv of a point inside the bounding box [boxMin, boxMin + boxSize]
            glm::vec2 boxUv(const glm::vec2 point, const glm::vec2 boxMin, const glm::vec2 boxSize){
                return (point - boxMin) / boxSize;
            }
        }

        Rectangle::Rectangle(const glm::vec2 size, const glm::vec2 offset, const glm::vec4 color){
            vertices = {
                {offset,                                  {0.f, 0.f}, color},   // top-left
                {offset + glm::vec2(size.x, 0.f),         {1.f, 0.f}, color},   // top-right
                {offset + size,                           {1.f, 1.f}, color},   // bottom-right
                {offset + glm::vec2(0.f, size.y),         {0.f, 1.f}, color}    // bottom-left
            };
            indices = {0, 1, 2, 2, 3, 0};
        }

        RoundedRectangle::RoundedRectangle(const glm::vec2 size, const float radius, const uint32_t cornerSegments,
                                           const glm::vec2 offset, const glm::vec4 color){
            const float r = std::clamp(radius, 0.f, 0.5f * std::min(size.x, size.y));
            const uint32_t segments = std::max(cornerSegments, 1u);

            // outline, clockwise on the screen, starting at the top-left corner: every corner is an arc of `segments + 1` points
            // (a corner with r == 0 degenerates to a single repeated point, which is removed)
            const glm::vec2 centers[4] = {
                offset + glm::vec2(r, r),                         // top-left
                offset + glm::vec2(size.x - r, r),                // top-right
                offset + glm::vec2(size.x - r, size.y - r),       // bottom-right
                offset + glm::vec2(r, size.y - r)                 // bottom-left
            };
            const float startAngle[4] = {2.f * HALF_PI, 3.f * HALF_PI, 0.f, HALF_PI};   // angle of the arc start, y down

            std::vector<glm::vec2> outline;
            for(uint32_t corner = 0; corner < 4; ++corner){
                for(uint32_t i = 0; i <= segments; ++i){
                    const float angle = startAngle[corner] + HALF_PI * static_cast<float>(i) / static_cast<float>(segments);
                    const glm::vec2 point = centers[corner] + r * glm::vec2(std::cos(angle), std::sin(angle));
                    if(!outline.empty() && glm::distance(outline.back(), point) < 1e-4f) continue;
                    outline.push_back(point);
                }
            }
            if(outline.size() > 1 && glm::distance(outline.front(), outline.back()) < 1e-4f) outline.pop_back();

            // triangle fan around the center of the rectangle
            const glm::vec2 center = offset + 0.5f * size;
            vertices.push_back({center, boxUv(center, offset, size), color});
            for(const glm::vec2& point : outline){
                vertices.push_back({point, boxUv(point, offset, size), color});
            }
            const uint32_t count = static_cast<uint32_t>(outline.size());
            for(uint32_t i = 0; i < count; ++i){
                indices.push_back(0);
                indices.push_back(1 + i);
                indices.push_back(1 + (i + 1) % count);
            }
        }

        Circle::Circle(const float radius, const uint32_t segmentCount, const glm::vec2 center, const glm::vec4 color){
            const uint32_t segments = std::max(segmentCount, 3u);
            const glm::vec2 boxMin = center - glm::vec2(radius);
            const glm::vec2 boxSize = glm::vec2(2.f * radius);

            vertices.push_back({center, boxUv(center, boxMin, boxSize), color});
            for(uint32_t i = 0; i < segments; ++i){
                const float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(segments);
                const glm::vec2 point = center + radius * glm::vec2(std::cos(angle), std::sin(angle));
                vertices.push_back({point, boxUv(point, boxMin, boxSize), color});
            }
            for(uint32_t i = 0; i < segments; ++i){
                indices.push_back(0);
                indices.push_back(1 + i);
                indices.push_back(1 + (i + 1) % segments);
            }
        }

        Ring::Ring(const float outerRadius, const float innerRadius, const uint32_t segmentCount,
                   const glm::vec2 center, const glm::vec4 color){
            const uint32_t segments = std::max(segmentCount, 3u);
            const float inner = std::clamp(innerRadius, 0.f, outerRadius);
            const glm::vec2 boxMin = center - glm::vec2(outerRadius);
            const glm::vec2 boxSize = glm::vec2(2.f * outerRadius);

            for(uint32_t i = 0; i < segments; ++i){
                const float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(segments);
                const glm::vec2 direction(std::cos(angle), std::sin(angle));
                const glm::vec2 outerPoint = center + outerRadius * direction;
                const glm::vec2 innerPoint = center + inner * direction;
                vertices.push_back({outerPoint, boxUv(outerPoint, boxMin, boxSize), color});    // 2 * i
                vertices.push_back({innerPoint, boxUv(innerPoint, boxMin, boxSize), color});    // 2 * i + 1
            }
            for(uint32_t i = 0; i < segments; ++i){
                const uint32_t outerA = 2 * i, innerA = 2 * i + 1;
                const uint32_t outerB = 2 * ((i + 1) % segments), innerB = 2 * ((i + 1) % segments) + 1;
                indices.insert(indices.end(), {outerA, outerB, innerB});
                indices.insert(indices.end(), {innerB, innerA, outerA});
            }
        }
    }
}
