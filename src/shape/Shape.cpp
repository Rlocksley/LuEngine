#include "Shape.hpp"

namespace Lu{
    namespace Shape{

        Cube::Cube(glm::vec3 halfSize, glm::vec3 offset, glm::vec4 color){
            auto size = halfSize;
            vertices = 
            {
            // Front face
            {{-1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, 0.0f, -1.0f}, color, {0.0f, 0.0f}},
            {{1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, 0.0f, -1.0f}, color, {1.0f, 0.0f}},
            {{1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, 0.0f, -1.0f}, color, {1.0f, 1.0f}},
            {{-1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, 0.0f, -1.0f}, color, {0.0f, 1.0f}},

            // Back face
            {{-1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, 0.0f, 1.0f}, color, {1.0f, 0.0f}},
            {{1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, 0.0f, 1.0f}, color, {0.0f, 0.0f}},
            {{1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, 0.0f, 1.0f}, color, {0.0f, 1.0f}},
            {{-1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, 0.0f, 1.0f}, color, {1.0f, 1.0f}},

            // Left face
            {{-1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {-1.0f, 0.0f, 0.0f}, color, {1.0f, 0.0f}},
            {{-1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {-1.0f, 0.0f, 0.0f}, color, {1.0f, 1.0f}},
            {{-1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {-1.0f, 0.0f, 0.0f}, color, {0.0f, 1.0f}},
            {{-1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {-1.0f, 0.0f, 0.0f}, color, {0.0f, 0.0f}},

            // Right face
            {{1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {1.0f, 0.0f, 0.0f}, color, {0.0f, 0.0f}},
            {{1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {1.0f, 0.0f, 0.0f}, color, {0.0f, 1.0f}},
            {{1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {1.0f, 0.0f, 0.0f}, color, {1.0f, 1.0f}},
            {{1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {1.0f, 0.0f, 0.0f}, color, {1.0f, 0.0f}},

            // Top face
            {{-1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, 1.0f, 0.0f}, color, {0.0f, 1.0f}},
            {{1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, 1.0f, 0.0f}, color, {1.0f, 1.0f}},
            {{1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, 1.0f, 0.0f}, color, {1.0f, 0.0f}},
            {{-1.0f * size[0] + offset[0], 1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, 1.0f, 0.0f}, color, {0.0f, 0.0f}},

            // Bottom face
            {{-1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, -1.0f, 0.0f}, color, {0.0f, 0.0f}},
            {{1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], -1.0f * size[2] + offset[2]}, {0.0f, -1.0f, 0.0f}, color, {1.0f, 0.0f}},
            {{1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, -1.0f, 0.0f}, color, {1.0f, 1.0f}},
            {{-1.0f * size[0] + offset[0], -1.0f * size[1] + offset[1], 1.0f * size[2] + offset[2]}, {0.0f, -1.0f, 0.0f}, color, {0.0f, 1.0f}}
            };
        }

    Sphere::Sphere(float radius, int latitudes, int longitudes, glm::vec3 offset, glm::vec4 color, bool invertNormals){
        GenerateSphereSmooth(radius, latitudes, longitudes, offset, color);
        if (invertNormals) {
            for (auto& vert : vertices)
                vert.normal = -vert.normal;
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                uint32_t tmp = indices[i + 1];
                indices[i + 1] = indices[i + 2];
                indices[i + 2] = tmp;
            }
        }
    }

    
    void Sphere::GenerateSphereSmooth(float radius, int latitudes, int longitudes, glm::vec3 offset, glm::vec4 color)
    {
        if(longitudes < 3)
            longitudes = 3;
        if(latitudes < 2)
            latitudes = 2;

        std::vector<glm::vec3> _vertices;
        std::vector<glm::vec3> _normals;
        std::vector<glm::vec2> _texCoords;
        
        float nx, ny, nz, lengthInv = 1.0f / radius;    // normal
        // Temporary vertex
        struct Vertex
        {
            float x, y, z; // Position
            float u, v;    // Texture coordinates
        };

        float deltaLatitude = PI / latitudes;
        float deltaLongitude = 2 * PI / longitudes;
        float latitudeAngle;
        float longitudeAngle;

        // Compute all vertices first except normals
        for (int i = 0; i <= latitudes; ++i)
        {
            latitudeAngle = PI / 2 - i * deltaLatitude; /* Starting -pi/2 to pi/2 */
            float xy = radius * cosf(latitudeAngle);    /* r * cos(phi) */
            float z = radius * sinf(latitudeAngle);     /* r * sin(phi )*/

            /*
            * We add (latitudes + 1) vertices per longitude because of equator,
            * the North pole and South pole are not counted here, as they overlap.
            * The first and last vertices have same position and normal, but
            * different tex coords.
            */
            for (int j = 0; j <= longitudes; ++j)
            {
                longitudeAngle = j * deltaLongitude;

                Vertex vertex;
                vertex.x = xy * cosf(longitudeAngle);       /* x = r * cos(phi) * cos(theta)  */
                vertex.y = xy * sinf(longitudeAngle);       /* y = r * cos(phi) * sin(theta) */
                vertex.z = z;                               /* z = r * sin(phi) */
                
                // Calculate texture coordinates
                vertex.u = (float)j / longitudes;
                vertex.v = (float)i / latitudes;
                
                _vertices.push_back(glm::vec3(vertex.x, vertex.y, vertex.z));
                _texCoords.push_back(glm::vec2(vertex.u, vertex.v));
               
                // normalized vertex normal
                nx = vertex.x * lengthInv;
                ny = vertex.y * lengthInv;
                nz = vertex.z * lengthInv;
                _normals.push_back(glm::vec3(nx, ny, nz));
            }
        }

        /*
        *  Indices
        *  k1--k1+1
        *  |  / |
        *  | /  |
        *  k2--k2+1
        */
        unsigned int k1, k2;
        for(int i = 0; i < latitudes; ++i)
        {
            k1 = i * (longitudes + 1);
            k2 = k1 + longitudes + 1;
            // 2 Triangles per latitude block excluding the first and last longitudes blocks
            for(int j = 0; j < longitudes; ++j, ++k1, ++k2)
            {
                if (i != 0)
                {
                    indices.push_back(k2);
                    indices.push_back(k1);
                    indices.push_back(k1 + 1);
                }

                if (i != (latitudes - 1))
                {
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                    indices.push_back(k2 + 1);
                }
            }
        }

        
        for(size_t i = 0; i < _vertices.size(); i++)
        {
            Lu::Vertex::Mesh vertex;
            vertex.position = _vertices[i] + offset;
            vertex.normal = _normals[i];
            vertex.texCoord = _texCoords[i];
            vertex.color = color;
            vertices.push_back(vertex);
        }
    }

    Square::Square(float halfSize, glm::vec3 offset, glm::vec4 color) {
        float h = halfSize;
        vertices = {
            // bottom-left
            {{-h + offset.x, -h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {0.0f, 1.0f}},
            // bottom-right
            {{ h + offset.x, -h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {1.0f, 1.0f}},
            // top-right
            {{ h + offset.x,  h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {1.0f, 0.0f}},
            // top-left
            {{-h + offset.x,  h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {0.0f, 0.0f}},
        };
    }

    namespace {
        void appendOrientedTriangle(
            std::vector<Vertex::Mesh>& verts,
            std::vector<uint32_t>& idxs,
            const glm::vec3& p0, const glm::vec2& uv0,
            const glm::vec3& p1, const glm::vec2& uv1,
            const glm::vec3& p2, const glm::vec2& uv2,
            const glm::vec3& n0,
            const glm::vec3& n1,
            const glm::vec3& n2,
            const glm::vec3& faceNormal,
            const glm::vec4& color)
        {
            uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back({p0, n0, color, uv0});
            verts.push_back({p1, n1, color, uv1});
            verts.push_back({p2, n2, color, uv2});
            // Match Cube's VK_FRONT_FACE_CLOCKWISE convention: dot(cross(p1 - p0, p2 - p0), normal) < 0
            glm::vec3 crossProd = glm::cross(p1 - p0, p2 - p0);
            if (glm::dot(crossProd, faceNormal) > 0.0f) {
                idxs.push_back(base + 0);
                idxs.push_back(base + 2);
                idxs.push_back(base + 1);
            } else {
                idxs.push_back(base + 0);
                idxs.push_back(base + 1);
                idxs.push_back(base + 2);
            }
        }

        void appendOrientedQuad(
            std::vector<Vertex::Mesh>& verts,
            std::vector<uint32_t>& idxs,
            const glm::vec3& p0, const glm::vec2& uv0,
            const glm::vec3& p1, const glm::vec2& uv1,
            const glm::vec3& p2, const glm::vec2& uv2,
            const glm::vec3& p3, const glm::vec2& uv3,
            const glm::vec3& normal,
            const glm::vec4& color)
        {
            appendOrientedTriangle(verts, idxs, p0, uv0, p1, uv1, p2, uv2, normal, normal, normal, normal, color);
            appendOrientedTriangle(verts, idxs, p0, uv0, p2, uv2, p3, uv3, normal, normal, normal, normal, color);
        }
    }

    GableRoof::GableRoof(const glm::vec3 halfSize, const glm::vec3 offset, const glm::vec4 color) {
        const float sx = halfSize.x;
        const float sy = halfSize.y;
        const float sz = halfSize.z;

        const glm::vec3 fbl = glm::vec3(-sx, -sy, -sz) + offset;
        const glm::vec3 fbr = glm::vec3( sx, -sy, -sz) + offset;
        const glm::vec3 ft  = glm::vec3(0.0f, sy, -sz) + offset;

        const glm::vec3 bbl = glm::vec3(-sx, -sy,  sz) + offset;
        const glm::vec3 bbr = glm::vec3( sx, -sy,  sz) + offset;
        const glm::vec3 bt  = glm::vec3(0.0f, sy,  sz) + offset;

        const glm::vec3 leftNormal  = glm::normalize(glm::vec3(-2.0f * sy, sx, 0.0f));
        const glm::vec3 rightNormal = glm::normalize(glm::vec3( 2.0f * sy, sx, 0.0f));
        const glm::vec3 frontNormal = glm::vec3(0.0f, 0.0f, -1.0f);
        const glm::vec3 backNormal  = glm::vec3(0.0f, 0.0f,  1.0f);
        const glm::vec3 botNormal   = glm::vec3(0.0f, -1.0f, 0.0f);

        // Left sloped roof face
        appendOrientedQuad(vertices, indices,
            bbl, {0.0f, 0.0f},
            fbl, {1.0f, 0.0f},
            ft,  {1.0f, 1.0f},
            bt,  {0.0f, 1.0f},
            leftNormal, color);

        // Right sloped roof face
        appendOrientedQuad(vertices, indices,
            fbr, {0.0f, 0.0f},
            bbr, {1.0f, 0.0f},
            bt,  {1.0f, 1.0f},
            ft,  {0.0f, 1.0f},
            rightNormal, color);

        // Front gable triangle
        appendOrientedTriangle(vertices, indices,
            fbl, {0.0f, 0.0f},
            fbr, {1.0f, 0.0f},
            ft,  {0.5f, 1.0f},
            frontNormal, frontNormal, frontNormal, frontNormal, color);

        // Back gable triangle
        appendOrientedTriangle(vertices, indices,
            bbr, {0.0f, 0.0f},
            bbl, {1.0f, 0.0f},
            bt,  {0.5f, 1.0f},
            backNormal, backNormal, backNormal, backNormal, color);

        // Bottom quad
        appendOrientedQuad(vertices, indices,
            fbl, {0.0f, 0.0f},
            bbl, {0.0f, 1.0f},
            bbr, {1.0f, 1.0f},
            fbr, {1.0f, 0.0f},
            botNormal, color);
    }

    Pyramid::Pyramid(const glm::vec3 halfSize, const glm::vec3 offset, const glm::vec4 color) {
        const float sx = halfSize.x;
        const float sy = halfSize.y;
        const float sz = halfSize.z;

        const glm::vec3 fbl  = glm::vec3(-sx, -sy, -sz) + offset;
        const glm::vec3 fbr  = glm::vec3( sx, -sy, -sz) + offset;
        const glm::vec3 bbr  = glm::vec3( sx, -sy,  sz) + offset;
        const glm::vec3 bbl  = glm::vec3(-sx, -sy,  sz) + offset;
        const glm::vec3 apex = glm::vec3(0.0f, sy, 0.0f) + offset;

        const glm::vec3 frontN = glm::normalize(glm::vec3(0.0f, sz, -2.0f * sy));
        const glm::vec3 backN  = glm::normalize(glm::vec3(0.0f, sz,  2.0f * sy));
        const glm::vec3 leftN  = glm::normalize(glm::vec3(-2.0f * sy, sx, 0.0f));
        const glm::vec3 rightN = glm::normalize(glm::vec3( 2.0f * sy, sx, 0.0f));
        const glm::vec3 botN   = glm::vec3(0.0f, -1.0f, 0.0f);

        appendOrientedTriangle(vertices, indices, fbl, {0.0f, 0.0f}, fbr, {1.0f, 0.0f}, apex, {0.5f, 1.0f}, frontN, frontN, frontN, frontN, color);
        appendOrientedTriangle(vertices, indices, bbr, {0.0f, 0.0f}, bbl, {1.0f, 0.0f}, apex, {0.5f, 1.0f}, backN,  backN,  backN,  backN,  color);
        appendOrientedTriangle(vertices, indices, bbl, {0.0f, 0.0f}, fbl, {1.0f, 0.0f}, apex, {0.5f, 1.0f}, leftN,  leftN,  leftN,  leftN,  color);
        appendOrientedTriangle(vertices, indices, fbr, {0.0f, 0.0f}, bbr, {1.0f, 0.0f}, apex, {0.5f, 1.0f}, rightN, rightN, rightN, rightN, color);

        appendOrientedQuad(vertices, indices,
            fbl, {0.0f, 0.0f},
            bbl, {0.0f, 1.0f},
            bbr, {1.0f, 1.0f},
            fbr, {1.0f, 0.0f},
            botN, color);
    }

    Cylinder::Cylinder(float radius, float halfHeight, int segments, const glm::vec3 offset, const glm::vec4 color) {
        if (segments < 3) segments = 3;
        const float step = static_cast<float>(2.0 * PI) / static_cast<float>(segments);

        const glm::vec3 topCenter = glm::vec3(0.0f,  halfHeight, 0.0f) + offset;
        const glm::vec3 botCenter = glm::vec3(0.0f, -halfHeight, 0.0f) + offset;
        const glm::vec3 topN(0.0f,  1.0f, 0.0f);
        const glm::vec3 botN(0.0f, -1.0f, 0.0f);

        for (int i = 0; i < segments; ++i) {
            float a0 = static_cast<float>(i) * step;
            float a1 = static_cast<float>(i + 1) * step;
            float u0 = static_cast<float>(i) / static_cast<float>(segments);
            float u1 = static_cast<float>(i + 1) / static_cast<float>(segments);

            glm::vec3 n0(std::cos(a0), 0.0f, std::sin(a0));
            glm::vec3 n1(std::cos(a1), 0.0f, std::sin(a1));
            glm::vec3 faceN = glm::normalize(n0 + n1);

            glm::vec3 b0 = glm::vec3(radius * n0.x, -halfHeight, radius * n0.z) + offset;
            glm::vec3 b1 = glm::vec3(radius * n1.x, -halfHeight, radius * n1.z) + offset;
            glm::vec3 t0 = glm::vec3(radius * n0.x,  halfHeight, radius * n0.z) + offset;
            glm::vec3 t1 = glm::vec3(radius * n1.x,  halfHeight, radius * n1.z) + offset;

            // Side quad (two smooth-shaded triangles)
            appendOrientedTriangle(vertices, indices, b0, {u0, 0.0f}, b1, {u1, 0.0f}, t1, {u1, 1.0f}, n0, n1, n1, faceN, color);
            appendOrientedTriangle(vertices, indices, b0, {u0, 0.0f}, t1, {u1, 1.0f}, t0, {u0, 1.0f}, n0, n1, n0, faceN, color);

            // Top cap slice
            appendOrientedTriangle(vertices, indices,
                topCenter, {0.5f, 0.5f},
                t0, {0.5f + 0.5f * n0.x, 0.5f + 0.5f * n0.z},
                t1, {0.5f + 0.5f * n1.x, 0.5f + 0.5f * n1.z},
                topN, topN, topN, topN, color);

            // Bottom cap slice
            appendOrientedTriangle(vertices, indices,
                botCenter, {0.5f, 0.5f},
                b1, {0.5f + 0.5f * n1.x, 0.5f + 0.5f * n1.z},
                b0, {0.5f + 0.5f * n0.x, 0.5f + 0.5f * n0.z},
                botN, botN, botN, botN, color);
        }
    }

    Cone::Cone(float radius, float halfHeight, int segments, const glm::vec3 offset, const glm::vec4 color) {
        if (segments < 3) segments = 3;
        const float step = static_cast<float>(2.0 * PI) / static_cast<float>(segments);
        const float height = 2.0f * halfHeight;

        const glm::vec3 apex      = glm::vec3(0.0f,  halfHeight, 0.0f) + offset;
        const glm::vec3 botCenter = glm::vec3(0.0f, -halfHeight, 0.0f) + offset;
        const glm::vec3 botN(0.0f, -1.0f, 0.0f);

        for (int i = 0; i < segments; ++i) {
            float a0 = static_cast<float>(i) * step;
            float a1 = static_cast<float>(i + 1) * step;
            float aMid = 0.5f * (a0 + a1);
            float u0 = static_cast<float>(i) / static_cast<float>(segments);
            float u1 = static_cast<float>(i + 1) / static_cast<float>(segments);

            glm::vec3 c0(std::cos(a0), 0.0f, std::sin(a0));
            glm::vec3 c1(std::cos(a1), 0.0f, std::sin(a1));

            glm::vec3 n0 = glm::normalize(glm::vec3(c0.x * height, radius, c0.z * height));
            glm::vec3 n1 = glm::normalize(glm::vec3(c1.x * height, radius, c1.z * height));
            glm::vec3 nApex = glm::normalize(glm::vec3(std::cos(aMid) * height, radius, std::sin(aMid) * height));

            glm::vec3 b0 = glm::vec3(radius * c0.x, -halfHeight, radius * c0.z) + offset;
            glm::vec3 b1 = glm::vec3(radius * c1.x, -halfHeight, radius * c1.z) + offset;

            appendOrientedTriangle(vertices, indices, b0, {u0, 0.0f}, b1, {u1, 0.0f}, apex, {0.5f * (u0 + u1), 1.0f}, n0, n1, nApex, nApex, color);

            appendOrientedTriangle(vertices, indices,
                botCenter, {0.5f, 0.5f},
                b1, {0.5f + 0.5f * c1.x, 0.5f + 0.5f * c1.z},
                b0, {0.5f + 0.5f * c0.x, 0.5f + 0.5f * c0.z},
                botN, botN, botN, botN, color);
        }
    }

    }
}