

#pragma once

#include <vector>
#include "PTQuad.h"
#include "PTProgram.h"
#include "Vec2.h"
#include "Vec3.h"
#include "Mat4.h"

namespace PathTraceAlg
{
    Program* LoadShaders(const ShaderInclude::ShaderSource& vertShaderObj, const ShaderInclude::ShaderSource& fragShaderObj);

    struct RenderOptions
    {
        RenderOptions()
        {
            renderResolution = iVec2(1280, 720);
            windowResolution = iVec2(1280, 720);
            uniformLightCol = Vec3(0.3f, 0.3f, 0.3f);
            backgroundCol = Vec3(1.0f, 1.0f, 1.0f);
            tileWidth = 100;
            tileHeight = 100;
            maxDepth = 5;
            maxSpp = -1;
            RRDepth = 2;
            texArrayWidth = 2048;
            texArrayHeight = 2048;
            denoiserFrameCnt = 20;
            enableRR = true;
            enableDenoiser = true;
            enableTonemap = true;
            enableAces = false;
            openglNormalMap = true;
            enableEnvMap = false;
            enableUniformLight = false;
            hideEmitters = false;
            enableBackground = false;
            transparentBackground = false;
            independentRenderSize = false;
            enableRoughnessMollification = false;
            enableVolumeMIS = false;
            envMapIntensity = 1.0f;
            envMapRot = 0.0f;
            roughnessMollificationAmt = 0.0f;
            useAdvancedTAA = true;
            simpleAcesFit = false;
            temporalMode = 3;
            temporalSpp = 1;
            temporalSeed = 1;
            historyWeight = 0.9f;
            depthThreshold = 0.02f;
            normalThreshold = 0.85f;
            luminanceTolerance = 2.0f;
            motionDecay = 0.005f;
            spatialFilterStrength = 0.65f;
            exposure = 0.0f;
        }

        iVec2 renderResolution;
        iVec2 windowResolution;
        Vec3 uniformLightCol;
        Vec3 backgroundCol;
        int tileWidth;
        int tileHeight;
        int maxDepth;
        int maxSpp;
        int RRDepth;
        int texArrayWidth;
        int texArrayHeight;
        int denoiserFrameCnt;
        bool enableRR;
        bool enableDenoiser;
        bool enableTonemap;
        bool enableAces;
        bool simpleAcesFit;
        bool openglNormalMap;
        bool enableEnvMap;
        bool enableUniformLight;
        bool hideEmitters;
        bool enableBackground;
        bool transparentBackground;
        bool independentRenderSize;
        bool enableRoughnessMollification;
        bool enableVolumeMIS;
        bool useAdvancedTAA;
        float envMapIntensity;
        float envMapRot;
        float roughnessMollificationAmt;
        // Research modes: 0 none, 1 fixed TAA, 2 geometry rejection,
        // 3 proposed adaptive method, 4 inverse-variance baseline,
        // 5 proposed without luminance, 6 proposed without motion,
        // 7 same-view unbiased accumulation for reference generation.
        int temporalMode;
        int temporalSpp;
        int temporalSeed;
        float historyWeight;
        float depthThreshold;
        float normalThreshold;
        float luminanceTolerance;
        float motionDecay;
        float spatialFilterStrength;
        float exposure;
    };

    class Scene;

    class Renderer
    {
    protected:
        // Core objects
        Scene* scene;
        Quad* quad;
        std::string shadersDirectory;
        bool initialized;

        // Resolution and window settings
        iVec2 renderSize;
        iVec2 windowSize;
        float pixelRatio;

        // OpenGL buffer objects for scene data
        GLuint bvhBuffer;
        GLuint vertexIndicesBuffer;
        GLuint verticesBuffer;
        GLuint normalsBuffer;

        // OpenGL textures for scene data
        GLuint bvhTexture;
        GLuint vertexIndicesTexture;
        GLuint verticesTexture;
        GLuint normalsTexture;
        GLuint materialsTexture;
        GLuint transformsTexture;
        GLuint lightsTexture;
        GLuint textureMapsArrayTexture;
        GLuint environmentMapTexture;
        GLuint environmentMapCDFTexture;

        // Framebuffers
        GLuint pathTraceFramebuffer;
        GLuint pathTraceFramebufferLowRes;
        GLuint accumulationFramebuffer;
        GLuint outputFramebuffer;
        GLuint taaFramebuffer;

        // Render textures
        GLuint pathTraceTextureLowRes;
        GLuint pathTraceTexture;
        GLuint accumTexture;
        GLuint tileOutputTexture[2];
        GLuint denoisedTexture;
        GLuint velocityTexture;
        GLuint historyTexture[2];
        GLuint positionTexture;
        GLuint normalTexture;
        GLuint historyPositionTexture[2];
        GLuint historyNormalTexture[2];
        GLuint temporalDiagnosticsTexture;
        int currentHistoryBuffer;

        // Shader programs
        Program* pathTraceShader;
        Program* pathTraceShaderLowRes;
        Program* pathTraceShaderTAA;
        Program* outputShader;
        Program* tonemapShader;
        Program* taaShader;
        Program* debugShader;

        // Rendering state and tracking
        iVec2 tile;
        iVec2 numTiles;
        Vec2 invNumTiles;
        int tileWidth;
        int tileHeight;
        int currentBuffer;
        int frameCounter;
        int sampleCounter;
        int temporalFrameCounter;
        int previousTemporalMode;
        bool temporalHistoryValid;
        int debugView;
        Mat4 previousViewProjectionMatrix;
        Vec3 previousCameraPosition;
        Vec3 previousCameraForward;

        // Denoiser
        Vec3* denoiserInputFramePointer;
        Vec3* frameOutputPointer;
        bool isDenoised;

    public:
        Renderer(Scene* scene, const std::string& shadersDirectory);
        ~Renderer();

        void Resize();
        void ReloadShaders();
        void Render();
        void Present();
        void Update(float deltaTime);
        float GetProgress();
        int GetSampleCount();
        void GetOutputBuffer(unsigned char**, int& width, int& height);
        void ResetTemporalHistory();
        void ExportResearchFrame(const std::string& prefix);
        double GetPathTraceMilliseconds() const { return pathTraceMilliseconds; }
        double GetTemporalMilliseconds() const { return temporalMilliseconds; }
        void SetDebugView(int view) { debugView = view; }
        int GetDebugView() const { return debugView; }

    private:
        // GPU Data Buffers initialization
        void InitializeGPUDataBuffers();
        void InitializeBVHBuffer();
        void InitializeVertexIndicesBuffer();
        void InitializeVerticesBuffer();
        void InitializeNormalsBuffer();
        void InitializeMaterialsTexture();
        void InitializeTransformsTexture();
        void InitializeLightsTexture();
        void InitializeTextureMapsArray();
        void InitializeEnvironmentMapTextures();
        void BindSceneDataTextures();

        // Framebuffers initialization
        void InitializeFramebuffers();
        void InitializeRenderingState();
        void InitializePathTraceFramebuffer();
        void InitializeLowResPreviewFramebuffer();
        void InitializeAccumulationFramebuffer();
        void InitializeVelocityTexture();
        void InitializeTAAFramebuffer();
        void InitializeOutputFramebuffer();
        void InitializeDenoiserResources();

        // Shaders initialization
        void InitializeShaders();
        void LoadShaderSources(ShaderInclude::ShaderSource& vertexShaderSrcObj,
                              ShaderInclude::ShaderSource& pathTraceShaderSrcObj,
                              ShaderInclude::ShaderSource& pathTraceShaderLowResSrcObj,
                              ShaderInclude::ShaderSource& pathTraceShaderTAASrcObj,
                              ShaderInclude::ShaderSource& taaShaderSrcObj,
                              ShaderInclude::ShaderSource& outputShaderSrcObj,
                              ShaderInclude::ShaderSource& tonemapShaderSrcObj);
        void BuildShaderDefines(std::string& pathtraceDefines, std::string& tonemapDefines);
        void InsertShaderDefines(ShaderInclude::ShaderSource& shaderSrc, const std::string& defines);
        void CompileShaderPrograms(ShaderInclude::ShaderSource& vertexShaderSrcObj,
                                   ShaderInclude::ShaderSource& pathTraceShaderSrcObj,
                                   ShaderInclude::ShaderSource& pathTraceShaderLowResSrcObj,
                                   ShaderInclude::ShaderSource& pathTraceShaderTAASrcObj,
                                   ShaderInclude::ShaderSource& taaShaderSrcObj,
                                   ShaderInclude::ShaderSource& outputShaderSrcObj,
                                   ShaderInclude::ShaderSource& tonemapShaderSrcObj);
        void SetupShaderUniforms();
        void SetupPathTraceShaderUniforms(Program* shader);
        void SetupTAAShaderUniforms();
        void WriteTexturePFM(GLuint texture, const std::string& filename, int channels);

        GLuint timingQueries[2];
        double pathTraceMilliseconds;
        double temporalMilliseconds;
    };
}
