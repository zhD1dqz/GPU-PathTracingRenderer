
#define PI         3.14159265358979323
#define INV_PI     0.31830988618379067
#define TWO_PI     6.28318530717958648
#define INV_TWO_PI 0.15915494309189533
#define INV_4_PI   0.07957747154594766
#define EPS 0.0003
#define INF 1000000.0

#define QUAD_LIGHT 0
#define SPHERE_LIGHT 1
#define DISTANT_LIGHT 2

#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

#define MEDIUM_NONE 0
#define MEDIUM_ABSORB 1
#define MEDIUM_SCATTER 2
#define MEDIUM_EMISSIVE 3

struct Ray
{
    vec3 origin;
    vec3 direction;
};

struct Medium
{
    int type;
    float density;
    vec3 color;
    float anisotropy;
};

struct Material
{
    vec3 baseColor;
    float opacity;
    int alphaMode;
    float alphaCutoff;
    vec3 emission;
    float anisotropic;
    float metallic;
    float roughness;
    float subsurface;
    float specularTint;
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatRoughness;
    float specTrans;
    float ior;
    float ax;
    float ay;
    Medium medium;
};

struct Camera
{
    vec3 up;
    vec3 right;
    vec3 forward;
    vec3 position;
    float fov;
    float focalDist;
    float aperture;
};

struct Light
{
    vec3 position;
    vec3 emission;
    vec3 u;
    vec3 v;
    float radius;
    float area;
    float type;
};

struct State
{
    int depth;
    float eta;
    float hitDist;

    vec3 fhp;
    vec3 normal;
    vec3 ffnormal;
    vec3 tangent;
    vec3 bitangent;

    bool isEmitter;

    vec2 texCoord;
    int matID;
    Material mat;
    Medium medium;
};

struct ScatterSampleRec
{
    vec3 L;
    vec3 f;
    float pdf;
};

struct LightSampleRec
{
    vec3 normal;
    vec3 emission;
    vec3 direction;
    float dist;
    float pdf;
};

uniform Camera camera;

// Uniform variables
uniform vec3 randomVector;
uniform vec2 resolution;
uniform vec2 tilePerOffset;
uniform vec2 invNumTiles;

uniform vec2 environmentMapResolution;
uniform float environmentMapTotalSum;
uniform float environmentMapIntensity;
uniform float environmentMapRotation;
uniform vec3 uniformLightColor;
uniform int lightCount;
uniform int maxDepth;
uniform int topLevelBVHIndex;
uniform int frameNumber;
uniform float roughnessMollificationAmount;

uniform sampler2D materialsTexture;
uniform sampler2D transformsTexture;
uniform sampler2D lightsTexture;
uniform sampler2DArray textureMapsArrayTexture;

uniform mat4 viewProjectionMatrix;
uniform mat4 previousViewProjectionMatrix;

uniform sampler2D accumTexture;
uniform samplerBuffer bvhBuffer;
uniform isamplerBuffer vertexIndicesTexture;
uniform samplerBuffer verticesTexture;
uniform samplerBuffer normalsTexture;
uniform sampler2D environmentMapTexture;
uniform sampler2D environmentMapCDFTexture;

uvec4 seed;
ivec2 pixel;

void BeginInit(vec2 p, int frame)
{
    pixel = ivec2(p);
    seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

void pcg4d(inout uvec4 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
    v = v ^ (v >> 16u);
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
    pcg4d(seed); return float(seed.x) / float(0xffffffffu);
}

vec3 FaceForward(vec3 a, vec3 b)
{
    return dot(a, b) < 0.0 ? -b : b;
}

float Luminance(vec3 c)
{
    return 0.212671 * c.x + 0.715160 * c.y + 0.072169 * c.z;
}