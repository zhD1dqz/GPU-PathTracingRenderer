
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include <iostream>
#include <vector>
#include "stb_image_resize.h"
#include "stb_image.h"
#include "PTScene.h"
#include "PTCamera.h"

namespace PathTraceAlg
{
    Scene::~Scene()
    {
        for (Mesh* mesh : meshes)
            delete mesh;
        meshes.clear();

        for (Texture* texture : textures)
            delete texture;
        textures.clear();

        delete camera;
        delete sceneBvh;
        delete envMap;
    }

    void Scene::AddCamera(Vec3 pos, Vec3 lookAt, float fov)
    {
        delete camera;
        camera = new Camera(pos, lookAt, fov);
    }

    int Scene::AddMesh(const std::string& filename)
    {
        // Check if mesh was already loaded
        for (int i = 0; i < meshes.size(); i++)
        {
            if (meshes[i]->name == filename)
                return i;
        }

        printf("Loading model %s\n", filename.c_str());
        Mesh* mesh = new Mesh;
        
        if (!mesh->LoadFromFile(filename))
        {
            printf("Unable to load model %s\n", filename.c_str());
            delete mesh;
            return -1;
        }

        int id = meshes.size();
        meshes.push_back(mesh);
        return id;
    }

    int Scene::AddTexture(const std::string& filename)
    {
        // Check if texture was already loaded
        for (int i = 0; i < textures.size(); i++)
        {
            if (textures[i]->name == filename)
                return i;
        }

        printf("Loading texture %s\n", filename.c_str());
        Texture* texture = new Texture;

        if (!texture->LoadTexture(filename))
        {
            printf("Unable to load texture %s\n", filename.c_str());
            delete texture;
            return -1;
        }

        int id = textures.size();
        textures.push_back(texture);
        return id;
    }

    int Scene::AddMaterial(const Material& material)
    {
        int id = materials.size();
        materials.push_back(material);
        return id;
    }

    void Scene::AddEnvMap(const std::string& filename)
    {
        if (envMap)
        {
            delete envMap;
            envMap = nullptr;
        }

        envMap = new EnvironmentMap;
        if (!envMap->LoadEnvMap(filename.c_str()))
        {
            printf("Unable to load HDR\n");
            delete envMap;
            envMap = nullptr;
            return;
        }

        printf("env %s file load\n", filename.c_str());
        envMapModified = true;
        dirty = true;
    }

    int Scene::AddMeshInstance(const MeshInstance& meshInstance)
    {
        int id = meshInstances.size();
        meshInstances.push_back(meshInstance);
        return id;
    }

    int Scene::AddLight(const Light& light)
    {
        int id = lights.size();
        lights.push_back(light);
        return id;
    }

    RadeonRays::bbox Scene::TransformBoundingBox(const RadeonRays::bbox& bbox, const Mat4& transform)
    {
        Vec3 minBound = bbox.pmin;
        Vec3 maxBound = bbox.pmax;

        Vec3 right = Vec3(transform.data[0][0], transform.data[0][1], transform.data[0][2]);
        Vec3 up = Vec3(transform.data[1][0], transform.data[1][1], transform.data[1][2]);
        Vec3 forward = Vec3(transform.data[2][0], transform.data[2][1], transform.data[2][2]);
        Vec3 translation = Vec3(transform.data[3][0], transform.data[3][1], transform.data[3][2]);

        Vec3 xa = right * minBound.x;
        Vec3 xb = right * maxBound.x;
        Vec3 ya = up * minBound.y;
        Vec3 yb = up * maxBound.y;
        Vec3 za = forward * minBound.z;
        Vec3 zb = forward * maxBound.z;

        minBound = Vec3::Min(xa, xb) + Vec3::Min(ya, yb) + Vec3::Min(za, zb) + translation;
        maxBound = Vec3::Max(xa, xb) + Vec3::Max(ya, yb) + Vec3::Max(za, zb) + translation;

        RadeonRays::bbox transformedBbox;
        transformedBbox.pmin = minBound;
        transformedBbox.pmax = maxBound;
        return transformedBbox;
    }

    void Scene::createTLAS()
    {
        std::vector<RadeonRays::bbox> bounds;
        bounds.resize(meshInstances.size());

        for (int i = 0; i < meshInstances.size(); i++)
        {
            const MeshInstance& instance = meshInstances[i];
            RadeonRays::bbox originalBbox = meshes[instance.meshID]->bvh->Bounds();
            bounds[i] = TransformBoundingBox(originalBbox, instance.transform);
        }

        sceneBvh->Build(&bounds[0], bounds.size());
        sceneBounds = sceneBvh->Bounds();
    }

    void Scene::createBLAS()
    {
        // Loop through all meshes and build BVHs
#pragma omp parallel for
        for (int i = 0; i < meshes.size(); i++)
        {
            meshes[i]->BuildBVH();
        }
    }

    void Scene::RebuildInstances()
    {
        delete sceneBvh;
        sceneBvh = new RadeonRays::Bvh(10.0f, 64, false);

        createTLAS();
        bvhTranslator.UpdateTLAS(sceneBvh, meshInstances);
        ProcessTransforms();

        instancesModified = true;
        dirty = true;
    }

    void Scene::ProcessScene()
    {
        createBLAS();
        createTLAS();
        bvhTranslator.Process(sceneBvh, meshes, meshInstances);

        ProcessMeshData();
        ProcessTransforms();
        ProcessTextures();
        CreateDefaultCamera();

        initialized = true;
    }

    void Scene::ProcessMeshData()
    {
        int verticesCnt = 0;
        for (int i = 0; i < meshes.size(); i++)
        {
            // Copy indices from BVH and not from Mesh. 
            // Required if splitBVH is used as a triangle can be shared by leaf nodes
            int numIndices = meshes[i]->bvh->GetNumIndices();
            const int* triIndices = meshes[i]->bvh->GetIndices();

            for (int j = 0; j < numIndices; j++)
            {
                int index = triIndices[j];
                int v1 = (index * 3 + 0) + verticesCnt;
                int v2 = (index * 3 + 1) + verticesCnt;
                int v3 = (index * 3 + 2) + verticesCnt;

                vertIndices.push_back(Indices{ v1, v2, v3 });
            }

            vertexData.insert(vertexData.end(), meshes[i]->vertexData.begin(), meshes[i]->vertexData.end());
            normalData.insert(normalData.end(), meshes[i]->normalData.begin(), meshes[i]->normalData.end());

            verticesCnt += meshes[i]->vertexData.size();
        }
    }

    void Scene::ProcessTransforms()
    {
        transforms.resize(meshInstances.size());
        for (int i = 0; i < meshInstances.size(); i++)
        {
            transforms[i] = meshInstances[i].transform;
        }
    }

    void Scene::ProcessTextures()
    {
        if (textures.empty())
            return;

        printf("load glb tex\n");

        int reqWidth = renderOptions.texArrayWidth;
        int reqHeight = renderOptions.texArrayHeight;
        int texBytes = reqWidth * reqHeight * 4;
        textureMapsArray.resize(texBytes * textures.size());

#pragma omp parallel for
        for (int i = 0; i < textures.size(); i++)
        {
            int texWidth = textures[i]->width;
            int texHeight = textures[i]->height;

            // Resize textures to fit 2D texture array
            if (texWidth != reqWidth || texHeight != reqHeight)
            {
                unsigned char* resizedTex = new unsigned char[texBytes];
                stbir_resize_uint8(&textures[i]->texData[0], texWidth, texHeight, 0, resizedTex, reqWidth, reqHeight, 0, 4);
                std::copy(resizedTex, resizedTex + texBytes, &textureMapsArray[i * texBytes]);
                delete[] resizedTex;
            }
            else
            {
                std::copy(textures[i]->texData.begin(), textures[i]->texData.end(), &textureMapsArray[i * texBytes]);
            }
        }
    }

    void Scene::CreateDefaultCamera()
    {
        if (camera)
            return;

        RadeonRays::bbox bounds = sceneBvh->Bounds();
        Vec3 extents = bounds.extents();
        Vec3 center = bounds.center();
        float distance = Vec3::Length(extents) * 2.0f;
        AddCamera(Vec3(center.x, center.y, center.z + distance), center, 45.0f);
    }
}


/*作用：整个渲染器的"数据库"
职责：
- 存储所有场景数据（网格、材质、灯光、相机）
- 构建和管理BVH加速结构
- 处理场景变换和实例
- 纹理管理和压缩
- 提供数据上传到GPU的接口

核心数据结构：
├── vector<Mesh*> meshes      - 所有网格数据
├── vector<Material> materials - 所有材质定义
├── vector<Light> lights      - 所有光源
├── vector<MeshInstance> meshInstances - 网格实例
├── Camera* camera           - 相机
└── EnvironmentMap* envMap   - 环境贴图

关键函数：
├── ProcessScene()     - 场景预处理（构建BVH等）
├── AddMesh()          - 添加网格
├── AddMaterial()      - 添加材质
├── AddLight()         - 添加光源
└── RebuildInstances() - 重建实例数据*/