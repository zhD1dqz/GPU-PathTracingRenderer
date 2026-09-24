
#define TINYOBJLOADER_IMPLEMENTATION

#include <iostream>
#include "tiny_obj_loader.h"
#include "PTMesh.h"

namespace PathTraceAlg
{
    float sphericalTheta(const Vec3& v)
    {
        return acosf(Math::Clamp(v.y, -1.f, 1.f));
    }

    float sphericalPhi(const Vec3& v)
    {
        float p = atan2f(v.z, v.x);
        return (p < 0.f) ? p + 2.f * PI : p;
    }

    bool Mesh::LoadFromFile(const std::string& filename)
    {
        name = filename;
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str(), 0, true);

        if (!ret)
        {
            printf("Unable to load model\n");
            return false;
        }

        // Loop over shapes
        for (size_t s = 0; s < shapes.size(); s++)
        {
            // Loop over faces(polygon)
            size_t index_offset = 0;

            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
            {
                // Loop over vertices in the face.
                for (size_t v = 0; v < 3; v++)
                {
                    // access to vertex
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                    tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                    tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                    tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                    tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                    tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                    tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                    tinyobj::real_t tx, ty;

                    if (!attrib.texcoords.empty())
                    {
                        tx = attrib.texcoords[2 * idx.texcoord_index + 0];
                        ty = 1.0 - attrib.texcoords[2 * idx.texcoord_index + 1];
                    }
                    else
                    {
                        if (v == 0)
                            tx = ty = 0;
                        else if (v == 1)
                            tx = 0, ty = 1;
                        else
                            tx = ty = 1;
                    }

                    vertexData.push_back(Vec4(vx, vy, vz, tx));
                    normalData.push_back(Vec4(nx, ny, nz, ty));
                }

                index_offset += 3;
            }
        }

        /*Vec3 center = Vec3(0.0, 0.0, 0.0);

        for (int i = 0; i < vertexData.size(); i++)
            center = center + Vec3(vertexData[i]);
        center = center * (1.0 / vertexData.size());

        for (int i = 0; i < vertexData.size(); i++)
        {
            Vec3 diff = Vec3(vertexData[i]) - center;
            diff = Vec3::Normalize(diff);
            vertexData[i].w = sphericalTheta(diff) * (1.0 / PI);
            normalData[i].w = sphericalPhi(diff) * (1.0 / (2.0 * PI));
        }*/

        return true;
    }

    void Mesh::BuildBVH()
    {
        const int numTris = vertexData.size() / 3;
        std::vector<RadeonRays::bbox> bounds(numTris);

#pragma omp parallel for
        for (int i = 0; i < numTris; ++i)
        {
            const Vec3 v1 = Vec3(vertexData[i * 3 + 0]);
            const Vec3 v2 = Vec3(vertexData[i * 3 + 1]);
            const Vec3 v3 = Vec3(vertexData[i * 3 + 2]);

            bounds[i].grow(v1);
            bounds[i].grow(v2);
            bounds[i].grow(v3);
        }

        bvh->Build(&bounds[0], numTris);
    }
}

/*作用：3D模型的"容器"
职责：
- 从OBJ文件加载网格数据
- 存储顶点、法线、UV数据
- 构建网格级别的BVH（BLAS）

关键数据：
├── vector<Vec4> vertexData - 顶点位置+UV.x
├── vector<Vec4> normalData - 法线方向+UV.y
└── RadeonRays::Bvh* bvh   - 网格的BVH

关键函数：
├── LoadFromFile() - 加载OBJ文件
└── BuildBVH()    - 构建网格BVH*/