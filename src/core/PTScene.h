

#pragma once

#include <string>
#include <vector>
#include <map>
#include "PTEnvironmentMap.h"
#include "bvh.h"
#include "PTRenderer.h"
#include "PTMesh.h"
#include "PTCamera.h"
#include "bvh_translator.h"
#include "PTTexture.h"
#include "PTMaterial.h"

namespace PathTraceAlg
{
    class Camera;

    enum LightType
    {
        RectLight,
        SphereLight,
        DistantLight
    };

    struct Light
    {
        Vec3 position;
        Vec3 emission;
        Vec3 u;
        Vec3 v;
        float radius;
        float area;
        float type;
    };

    struct Indices
    {
        int x, y, z;
    };

    class Scene
    {
    public:
        Scene() : camera(nullptr), envMap(nullptr), initialized(false), dirty(true) {
            sceneBvh = new RadeonRays::Bvh(10.0f, 64, false);
        }
        ~Scene();

        int AddMesh(const std::string& filename);
        int AddTexture(const std::string& filename);
        int AddMaterial(const Material& material);
        int AddMeshInstance(const MeshInstance& meshInstance);
        int AddLight(const Light& light);

        void AddCamera(Vec3 eye, Vec3 lookat, float fov);
        void AddEnvMap(const std::string& filename);

        void ProcessScene();
        void RebuildInstances();

        // Options
        RenderOptions renderOptions;

        // Meshes
        std::vector<Mesh*> meshes;

        // Scene Mesh Data 
        std::vector<Indices> vertIndices;
        std::vector<Vec4> vertexData; // Vertex + texture Coord (u/s)
        std::vector<Vec4> normalData; // Normal + texture Coord (v/t)
        std::vector<Mat4> transforms;

        // Materials
        std::vector<Material> materials;

        // Instances
        std::vector<MeshInstance> meshInstances;

        // Lights
        std::vector<Light> lights;

        // Environment Map
        EnvironmentMap* envMap;

        // Camera
        Camera* camera;

        // Bvh
        RadeonRays::BvhTranslator bvhTranslator; // Produces a flat bvh array for GPU consumption
        RadeonRays::bbox sceneBounds;

        // Texture Data
        std::vector<Texture*> textures;
        std::vector<unsigned char> textureMapsArray;

        bool initialized;
        bool dirty;

        bool instancesModified = false;
        bool envMapModified = false;

    private:
        RadeonRays::Bvh* sceneBvh;
        void createBLAS();
        void createTLAS();
        void ProcessMeshData();
        void ProcessTransforms();
        void ProcessTextures();
        void CreateDefaultCamera();
        RadeonRays::bbox TransformBoundingBox(const RadeonRays::bbox& bbox, const Mat4& transform);
    };
}
