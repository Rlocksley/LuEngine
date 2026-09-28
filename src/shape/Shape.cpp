

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
            {{-h + offset.x, -h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {0.0f, 0.0f}},
            // bottom-right
            {{ h + offset.x, -h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {1.0f, 0.0f}},
            // top-right
            {{ h + offset.x,  h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {1.0f, 1.0f}},
            // top-left
            {{-h + offset.x,  h + offset.y, offset.z}, {0.0f, 0.0f, 1.0f}, color, {0.0f, 1.0f}},
        };
    }

    }
}