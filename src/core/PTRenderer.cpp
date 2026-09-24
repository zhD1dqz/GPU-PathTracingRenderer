
#include "PTConfig.h"
#include "PTRenderer.h"
#include "PTShaderIncludes.h"
#include "PTScene.h"
#include "OpenImageDenoise/oidn.hpp"
#include <fstream>

namespace PathTraceAlg
{
    Program* LoadShaders(const ShaderInclude::ShaderSource& vertShaderObj, const ShaderInclude::ShaderSource& fragShaderObj)
    {
        std::vector<Shader> shaders;
        shaders.push_back(Shader(vertShaderObj, GL_VERTEX_SHADER));
        shaders.push_back(Shader(fragShaderObj, GL_FRAGMENT_SHADER));
        return new Program(shaders);
    }

    Renderer::Renderer(Scene* scene, const std::string& shadersDirectory)
        : scene(scene)
        , quad(nullptr)
        , initialized(false)
        , bvhBuffer(0)
        , bvhTexture(0)
        , vertexIndicesBuffer(0)
        , vertexIndicesTexture(0)
        , verticesBuffer(0)
        , verticesTexture(0)
        , normalsBuffer(0)
        , normalsTexture(0)
        , materialsTexture(0)
        , transformsTexture(0)
        , lightsTexture(0)
        , textureMapsArrayTexture(0)
        , environmentMapTexture(0)
        , environmentMapCDFTexture(0)
        , pathTraceTextureLowRes(0)
        , pathTraceTexture(0)
        , accumTexture(0)
        , tileOutputTexture()
        , denoisedTexture(0)
        , velocityTexture(0)
        , historyTexture()
        , pathTraceFramebuffer(0)
        , pathTraceFramebufferLowRes(0)
        , accumulationFramebuffer(0)
        , outputFramebuffer(0)
        , taaFramebuffer(0)
        , shadersDirectory(shadersDirectory)
        , pathTraceShader(nullptr)
        , pathTraceShaderLowRes(nullptr)
        , outputShader(nullptr)
        , tonemapShader(nullptr)
        , pathTraceShaderTAA(nullptr)
        , taaShader(nullptr)
        , debugShader(nullptr)
        , positionTexture(0)
        , normalTexture(0)
        , historyPositionTexture()
        , historyNormalTexture()
        , temporalDiagnosticsTexture(0)
        , currentHistoryBuffer(0)
        , temporalFrameCounter(0)
        , previousTemporalMode(-1)
        , temporalHistoryValid(false)
        , debugView(0)
        , denoiserInputFramePointer(nullptr)
        , frameOutputPointer(nullptr)
        , isDenoised(false)
        , pathTraceMilliseconds(0.0)
        , temporalMilliseconds(0.0)
        , timingQueries()
    {
        if (scene == nullptr)
        {
            printf("No Scene Found\n");
            return;
        }

        if (!scene->initialized)
            scene->ProcessScene();

        InitializeGPUDataBuffers();
        quad = new Quad();
        pixelRatio = 0.25f;

        InitializeFramebuffers();
        InitializeShaders();
        glGenQueries(2, timingQueries);
    }

    Renderer::~Renderer()
    {
        delete quad;

        // Delete textures
        glDeleteTextures(1, &bvhTexture);
        glDeleteTextures(1, &vertexIndicesTexture);
        glDeleteTextures(1, &verticesTexture);
        glDeleteTextures(1, &normalsTexture);
        glDeleteTextures(1, &materialsTexture);
        glDeleteTextures(1, &transformsTexture);
        glDeleteTextures(1, &lightsTexture);
        glDeleteTextures(1, &textureMapsArrayTexture);
        glDeleteTextures(1, &environmentMapTexture);
        glDeleteTextures(1, &environmentMapCDFTexture);
        glDeleteTextures(1, &pathTraceTexture);
        glDeleteTextures(1, &pathTraceTextureLowRes);
        glDeleteTextures(1, &accumTexture);
        glDeleteTextures(1, &tileOutputTexture[0]);
        glDeleteTextures(1, &tileOutputTexture[1]);
        glDeleteTextures(1, &denoisedTexture);
        glDeleteTextures(1, &velocityTexture);
        glDeleteTextures(2, historyTexture);
        glDeleteTextures(1, &positionTexture);
        glDeleteTextures(1, &normalTexture);
        glDeleteTextures(2, historyPositionTexture);
        glDeleteTextures(2, historyNormalTexture);
        glDeleteTextures(1, &temporalDiagnosticsTexture);

        // Delete buffers
        glDeleteBuffers(1, &bvhBuffer);
        glDeleteBuffers(1, &vertexIndicesBuffer);
        glDeleteBuffers(1, &verticesBuffer);
        glDeleteBuffers(1, &normalsBuffer);

        // Delete FBOs
        glDeleteFramebuffers(1, &pathTraceFramebuffer);
        glDeleteFramebuffers(1, &pathTraceFramebufferLowRes);
        glDeleteFramebuffers(1, &accumulationFramebuffer);
        glDeleteFramebuffers(1, &outputFramebuffer);
        glDeleteFramebuffers(1, &taaFramebuffer);
        glDeleteQueries(2, timingQueries);

        // Delete shaders
        delete pathTraceShader;
        delete pathTraceShaderLowRes;
        delete outputShader;
        delete tonemapShader;
        delete pathTraceShaderTAA;
        delete taaShader;
        delete debugShader;

        // Delete denoiser data
        delete[] denoiserInputFramePointer;
        delete[] frameOutputPointer;

    }

    void Renderer::InitializeGPUDataBuffers()
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        InitializeBVHBuffer();
        InitializeVertexIndicesBuffer();
        InitializeVerticesBuffer();
        InitializeNormalsBuffer();
        InitializeMaterialsTexture();
        InitializeTransformsTexture();
        InitializeLightsTexture();
        InitializeTextureMapsArray();
        InitializeEnvironmentMapTextures();
        BindSceneDataTextures();
    }

    void Renderer::InitializeBVHBuffer()
    {
        glGenBuffers(1, &bvhBuffer);
        glBindBuffer(GL_TEXTURE_BUFFER, bvhBuffer);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(RadeonRays::BvhTranslator::Node) * scene->bvhTranslator.nodes.size(), &scene->bvhTranslator.nodes[0], GL_STATIC_DRAW);
        glGenTextures(1, &bvhTexture);
        glBindTexture(GL_TEXTURE_BUFFER, bvhTexture);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, bvhBuffer);
    }

    void Renderer::InitializeVertexIndicesBuffer()
    {
        glGenBuffers(1, &vertexIndicesBuffer);
        glBindBuffer(GL_TEXTURE_BUFFER, vertexIndicesBuffer);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(Indices) * scene->vertIndices.size(), &scene->vertIndices[0], GL_STATIC_DRAW);
        glGenTextures(1, &vertexIndicesTexture);
        glBindTexture(GL_TEXTURE_BUFFER, vertexIndicesTexture);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32I, vertexIndicesBuffer);
    }

    void Renderer::InitializeVerticesBuffer()
    {
        glGenBuffers(1, &verticesBuffer);
        glBindBuffer(GL_TEXTURE_BUFFER, verticesBuffer);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec4) * scene->vertexData.size(), &scene->vertexData[0], GL_STATIC_DRAW);
        glGenTextures(1, &verticesTexture);
        glBindTexture(GL_TEXTURE_BUFFER, verticesTexture);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, verticesBuffer);
    }

    void Renderer::InitializeNormalsBuffer()
    {
        glGenBuffers(1, &normalsBuffer);
        glBindBuffer(GL_TEXTURE_BUFFER, normalsBuffer);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec4) * scene->normalData.size(), &scene->normalData[0], GL_STATIC_DRAW);
        glGenTextures(1, &normalsTexture);
        glBindTexture(GL_TEXTURE_BUFFER, normalsTexture);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, normalsBuffer);
    }

    void Renderer::InitializeMaterialsTexture()
    {
        glGenTextures(1, &materialsTexture);
        glBindTexture(GL_TEXTURE_2D, materialsTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * scene->materials.size(), 1, 0, GL_RGBA, GL_FLOAT, &scene->materials[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Renderer::InitializeTransformsTexture()
    {
        glGenTextures(1, &transformsTexture);
        glBindTexture(GL_TEXTURE_2D, transformsTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Mat4) / sizeof(Vec4)) * scene->transforms.size(), 1, 0, GL_RGBA, GL_FLOAT, &scene->transforms[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Renderer::InitializeLightsTexture()
    {
        if (!scene->lights.empty())
        {
            glGenTextures(1, &lightsTexture);
            glBindTexture(GL_TEXTURE_2D, lightsTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, (sizeof(Light) / sizeof(Vec3)) * scene->lights.size(), 1, 0, GL_RGB, GL_FLOAT, &scene->lights[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void Renderer::InitializeTextureMapsArray()
    {
        if (!scene->textures.empty())
        {
            glGenTextures(1, &textureMapsArrayTexture);
            glBindTexture(GL_TEXTURE_2D_ARRAY, textureMapsArrayTexture);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, scene->renderOptions.texArrayWidth, scene->renderOptions.texArrayHeight, scene->textures.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &scene->textureMapsArray[0]);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        }
    }

    void Renderer::InitializeEnvironmentMapTextures()
    {
        if (scene->envMap != nullptr)
        {
            glGenTextures(1, &environmentMapTexture);
            glBindTexture(GL_TEXTURE_2D, environmentMapTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, scene->envMap->width, scene->envMap->height, 0, GL_RGB, GL_FLOAT, scene->envMap->img);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            glGenTextures(1, &environmentMapCDFTexture);
            glBindTexture(GL_TEXTURE_2D, environmentMapCDFTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene->envMap->width, scene->envMap->height, 0, GL_RED, GL_FLOAT, scene->envMap->cdf);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void Renderer::BindSceneDataTextures()
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_BUFFER, bvhTexture);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_BUFFER, vertexIndicesTexture);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_BUFFER, verticesTexture);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_BUFFER, normalsTexture);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, materialsTexture);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, transformsTexture);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, lightsTexture);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureMapsArrayTexture);
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, environmentMapTexture);
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, environmentMapCDFTexture);
    }

    void Renderer::Resize()
    {
        // Delete
        glDeleteTextures(1, &pathTraceTexture);
        glDeleteTextures(1, &pathTraceTextureLowRes);
        glDeleteTextures(1, &accumTexture);
        glDeleteTextures(1, &tileOutputTexture[0]);
        glDeleteTextures(1, &tileOutputTexture[1]);
        glDeleteTextures(1, &denoisedTexture);
        glDeleteTextures(1, &velocityTexture);
        glDeleteTextures(2, historyTexture);
        glDeleteTextures(1, &positionTexture);
        glDeleteTextures(1, &normalTexture);
        glDeleteTextures(2, historyPositionTexture);
        glDeleteTextures(2, historyNormalTexture);
        glDeleteTextures(1, &temporalDiagnosticsTexture);

        glDeleteFramebuffers(1, &pathTraceFramebuffer);
        glDeleteFramebuffers(1, &pathTraceFramebufferLowRes);
        glDeleteFramebuffers(1, &accumulationFramebuffer);
        glDeleteFramebuffers(1, &outputFramebuffer);
        glDeleteFramebuffers(1, &taaFramebuffer);
        delete[] denoiserInputFramePointer;
        delete[] frameOutputPointer;

        delete pathTraceShader;
        delete pathTraceShaderLowRes;
        delete outputShader;
        delete tonemapShader;
        delete pathTraceShaderTAA;
        delete taaShader;
        delete debugShader;

        InitializeFramebuffers();
        InitializeShaders();
    }

    void Renderer::InitializeFramebuffers()
    {
        InitializeRenderingState();
        InitializePathTraceFramebuffer();
        InitializeLowResPreviewFramebuffer();
        InitializeAccumulationFramebuffer();
        InitializeVelocityTexture();
        InitializeTAAFramebuffer();
        InitializeOutputFramebuffer();
        InitializeDenoiserResources();
        ResetTemporalHistory();
    }

    void Renderer::InitializeRenderingState()
    {
        sampleCounter = 1;
        currentBuffer = 0;
        frameCounter = 1;

        renderSize = scene->renderOptions.renderResolution;
        windowSize = scene->renderOptions.windowResolution;

        tileWidth = scene->renderOptions.tileWidth;
        tileHeight = scene->renderOptions.tileHeight;

        invNumTiles.x = (float)tileWidth / renderSize.x;
        invNumTiles.y = (float)tileHeight / renderSize.y;

        numTiles.x = ceil((float)renderSize.x / tileWidth);
        numTiles.y = ceil((float)renderSize.y / tileHeight);

        tile.x = -1;
        tile.y = numTiles.y - 1;
    }

    void Renderer::InitializePathTraceFramebuffer()
    {
        glGenFramebuffers(1, &pathTraceFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, pathTraceFramebuffer);

        glGenTextures(1, &pathTraceTexture);
        glBindTexture(GL_TEXTURE_2D, pathTraceTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tileWidth, tileHeight, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pathTraceTexture, 0);
    }

    void Renderer::InitializeLowResPreviewFramebuffer()
    {
        glGenFramebuffers(1, &pathTraceFramebufferLowRes);
        glBindFramebuffer(GL_FRAMEBUFFER, pathTraceFramebufferLowRes);

        glGenTextures(1, &pathTraceTextureLowRes);
        glBindTexture(GL_TEXTURE_2D, pathTraceTextureLowRes);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, windowSize.x * pixelRatio, windowSize.y * pixelRatio, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pathTraceTextureLowRes, 0);
    }

    void Renderer::InitializeAccumulationFramebuffer()
    {
        glGenFramebuffers(1, &accumulationFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, accumulationFramebuffer);

        glGenTextures(1, &accumTexture);
        glBindTexture(GL_TEXTURE_2D, accumTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTexture, 0);
    }

    void Renderer::InitializeVelocityTexture()
    {
        glGenTextures(1, &velocityTexture);
        glBindTexture(GL_TEXTURE_2D, velocityTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, renderSize.x, renderSize.y, 0, GL_RG, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, velocityTexture, 0);

        glGenTextures(1, &positionTexture);
        glBindTexture(GL_TEXTURE_2D, positionTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, positionTexture, 0);

        glGenTextures(1, &normalTexture);
        glBindTexture(GL_TEXTURE_2D, normalTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, normalTexture, 0);
    }

    void Renderer::InitializeTAAFramebuffer()
    {
        glGenFramebuffers(1, &taaFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, taaFramebuffer);

        for (int i = 0; i < 2; i++)
        {
            glGenTextures(1, &historyTexture[i]);
            glBindTexture(GL_TEXTURE_2D, historyTexture[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            glGenTextures(1, &historyPositionTexture[i]);
            glBindTexture(GL_TEXTURE_2D, historyPositionTexture[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glGenTextures(1, &historyNormalTexture[i]);
            glBindTexture(GL_TEXTURE_2D, historyNormalTexture[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glGenTextures(1, &temporalDiagnosticsTexture);
        glBindTexture(GL_TEXTURE_2D, temporalDiagnosticsTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        currentHistoryBuffer = 0;
    }

    void Renderer::InitializeOutputFramebuffer()
    {
        glGenFramebuffers(1, &outputFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);

        glGenTextures(1, &tileOutputTexture[0]);
        glBindTexture(GL_TEXTURE_2D, tileOutputTexture[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenTextures(1, &tileOutputTexture[1]);
        glBindTexture(GL_TEXTURE_2D, tileOutputTexture[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderSize.x, renderSize.y, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileOutputTexture[currentBuffer], 0);
    }

    void Renderer::InitializeDenoiserResources()
    {
        denoiserInputFramePointer = new Vec3[renderSize.x * renderSize.y];
        frameOutputPointer = new Vec3[renderSize.x * renderSize.y];

        glGenTextures(1, &denoisedTexture);
        glBindTexture(GL_TEXTURE_2D, denoisedTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, renderSize.x, renderSize.y, 0, GL_RGB, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Renderer::ReloadShaders()
    {
        // Delete shaders
        delete pathTraceShader;
        delete pathTraceShaderLowRes;
        delete outputShader;
        delete tonemapShader;
        delete pathTraceShaderTAA;
        delete taaShader;
        delete debugShader;
        ResetTemporalHistory();

        InitializeShaders();
    }

    void Renderer::InitializeShaders()
    {
        ShaderInclude::ShaderSource vertexShaderSrcObj;
        ShaderInclude::ShaderSource pathTraceShaderSrcObj;
        ShaderInclude::ShaderSource pathTraceShaderLowResSrcObj;
        ShaderInclude::ShaderSource pathTraceShaderTAASrcObj;
        ShaderInclude::ShaderSource taaShaderSrcObj;
        ShaderInclude::ShaderSource outputShaderSrcObj;
        ShaderInclude::ShaderSource tonemapShaderSrcObj;

        LoadShaderSources(vertexShaderSrcObj, pathTraceShaderSrcObj, pathTraceShaderLowResSrcObj,
                         pathTraceShaderTAASrcObj, taaShaderSrcObj, outputShaderSrcObj, tonemapShaderSrcObj);

        std::string pathtraceDefines = "";
        std::string tonemapDefines = "";
        BuildShaderDefines(pathtraceDefines, tonemapDefines);

        if (pathtraceDefines.size() > 0)
        {
            InsertShaderDefines(pathTraceShaderSrcObj, pathtraceDefines);
            InsertShaderDefines(pathTraceShaderLowResSrcObj, pathtraceDefines);
            InsertShaderDefines(pathTraceShaderTAASrcObj, pathtraceDefines);
        }

        if (tonemapDefines.size() > 0)
        {
            InsertShaderDefines(tonemapShaderSrcObj, tonemapDefines);
        }

        CompileShaderPrograms(vertexShaderSrcObj, pathTraceShaderSrcObj, pathTraceShaderLowResSrcObj,
                              pathTraceShaderTAASrcObj, taaShaderSrcObj, outputShaderSrcObj, tonemapShaderSrcObj);

        ShaderInclude::ShaderSource debugShaderSrcObj =
            ShaderInclude::load(shadersDirectory + "debug_view.glsl");
        debugShader = LoadShaders(vertexShaderSrcObj, debugShaderSrcObj);

        SetupShaderUniforms();
        debugShader->Use();
        glUniform1i(glGetUniformLocation(debugShader->getObject(), "debugTexture"), 0);
        debugShader->StopUsing();
    }

    void Renderer::LoadShaderSources(ShaderInclude::ShaderSource& vertexShaderSrcObj,
                                      ShaderInclude::ShaderSource& pathTraceShaderSrcObj,
                                      ShaderInclude::ShaderSource& pathTraceShaderLowResSrcObj,
                                      ShaderInclude::ShaderSource& pathTraceShaderTAASrcObj,
                                      ShaderInclude::ShaderSource& taaShaderSrcObj,
                                      ShaderInclude::ShaderSource& outputShaderSrcObj,
                                      ShaderInclude::ShaderSource& tonemapShaderSrcObj)
    {
        vertexShaderSrcObj = ShaderInclude::load(shadersDirectory + "generic/vertex.glsl");
        pathTraceShaderSrcObj = ShaderInclude::load(shadersDirectory + "tile.glsl");
        pathTraceShaderLowResSrcObj = ShaderInclude::load(shadersDirectory + "preview.glsl");
        pathTraceShaderTAASrcObj = ShaderInclude::load(shadersDirectory + "preview_taa.glsl");
        taaShaderSrcObj = ShaderInclude::load(shadersDirectory + "taa.glsl");
        outputShaderSrcObj = ShaderInclude::load(shadersDirectory + "output.glsl");
        tonemapShaderSrcObj = ShaderInclude::load(shadersDirectory + "tonemapping.glsl");
    }

    void Renderer::BuildShaderDefines(std::string& pathtraceDefines, std::string& tonemapDefines)
    {
        if (scene->renderOptions.enableEnvMap && scene->envMap != nullptr)
            pathtraceDefines += "#define OPT_ENVMAP\n";

        if (!scene->lights.empty())
            pathtraceDefines += "#define OPT_LIGHTS\n";

        if (scene->renderOptions.enableRR)
        {
            pathtraceDefines += "#define OPT_RR\n";
            pathtraceDefines += "#define OPT_RR_DEPTH " + std::to_string(scene->renderOptions.RRDepth) + "\n";
        }

        if (scene->renderOptions.enableUniformLight)
            pathtraceDefines += "#define OPT_UNIFORM_LIGHT\n";

        if (scene->renderOptions.openglNormalMap)
            pathtraceDefines += "#define OPT_OPENGL_NORMALMAP\n";

        if (scene->renderOptions.hideEmitters)
            pathtraceDefines += "#define OPT_HIDE_EMITTERS\n";

        if (scene->renderOptions.enableBackground)
        {
            pathtraceDefines += "#define OPT_BACKGROUND\n";
            tonemapDefines += "#define OPT_BACKGROUND\n";
        }

        if (scene->renderOptions.transparentBackground)
        {
            pathtraceDefines += "#define OPT_TRANSPARENT_BACKGROUND\n";
            tonemapDefines += "#define OPT_TRANSPARENT_BACKGROUND\n";
        }

        for (int i = 0; i < scene->materials.size(); i++)
        {
            if ((int)scene->materials[i].alphaMode != AlphaMode::Opaque)
            {
                pathtraceDefines += "#define OPT_ALPHA_TEST\n";
                break;
            }
        }

        if (scene->renderOptions.enableRoughnessMollification)
            pathtraceDefines += "#define OPT_ROUGHNESS_MOLLIFICATION\n";

        for (int i = 0; i < scene->materials.size(); i++)
        {
            if ((int)scene->materials[i].mediumType != MediumType::None)
            {
                pathtraceDefines += "#define OPT_MEDIUM\n";
                break;
            }
        }

        if (scene->renderOptions.enableVolumeMIS)
            pathtraceDefines += "#define OPT_VOL_MIS\n";
    }

    void Renderer::InsertShaderDefines(ShaderInclude::ShaderSource& shaderSrc, const std::string& defines)
    {
        size_t idx = shaderSrc.src.find("#version");
        if (idx != -1)
            idx = shaderSrc.src.find("\n", idx);
        else
            idx = 0;
        shaderSrc.src.insert(idx + 1, defines);
    }

    void Renderer::CompileShaderPrograms(ShaderInclude::ShaderSource& vertexShaderSrcObj,
                                         ShaderInclude::ShaderSource& pathTraceShaderSrcObj,
                                         ShaderInclude::ShaderSource& pathTraceShaderLowResSrcObj,
                                         ShaderInclude::ShaderSource& pathTraceShaderTAASrcObj,
                                         ShaderInclude::ShaderSource& taaShaderSrcObj,
                                         ShaderInclude::ShaderSource& outputShaderSrcObj,
                                         ShaderInclude::ShaderSource& tonemapShaderSrcObj)
    {
        pathTraceShader = LoadShaders(vertexShaderSrcObj, pathTraceShaderSrcObj);
        pathTraceShaderLowRes = LoadShaders(vertexShaderSrcObj, pathTraceShaderLowResSrcObj);
        pathTraceShaderTAA = LoadShaders(vertexShaderSrcObj, pathTraceShaderTAASrcObj);
        taaShader = LoadShaders(vertexShaderSrcObj, taaShaderSrcObj);
        outputShader = LoadShaders(vertexShaderSrcObj, outputShaderSrcObj);
        tonemapShader = LoadShaders(vertexShaderSrcObj, tonemapShaderSrcObj);
    }

    void Renderer::SetupShaderUniforms()
    {
        SetupPathTraceShaderUniforms(pathTraceShader);
        SetupPathTraceShaderUniforms(pathTraceShaderLowRes);
        SetupPathTraceShaderUniforms(pathTraceShaderTAA);
        SetupTAAShaderUniforms();
    }

    void Renderer::SetupPathTraceShaderUniforms(Program* shader)
    {
        shader->Use();
        GLuint shaderObject = shader->getObject();

        if (scene->envMap)
        {
            glUniform2f(glGetUniformLocation(shaderObject, "environmentMapResolution"), (float)scene->envMap->width, (float)scene->envMap->height);
            glUniform1f(glGetUniformLocation(shaderObject, "environmentMapTotalSum"), scene->envMap->totalSum);
        }

        glUniform1i(glGetUniformLocation(shaderObject, "topLevelBVHIndex"), scene->bvhTranslator.topLevelIndex);
        glUniform2f(glGetUniformLocation(shaderObject, "resolution"), float(renderSize.x), float(renderSize.y));
        if (shader == pathTraceShader)
            glUniform2f(glGetUniformLocation(shaderObject, "invNumTiles"), invNumTiles.x, invNumTiles.y);
        glUniform1i(glGetUniformLocation(shaderObject, "lightCount"), scene->lights.size());
        glUniform1i(glGetUniformLocation(shaderObject, "accumTexture"), 0);
        glUniform1i(glGetUniformLocation(shaderObject, "bvhBuffer"), 1);
        glUniform1i(glGetUniformLocation(shaderObject, "vertexIndicesTexture"), 2);
        glUniform1i(glGetUniformLocation(shaderObject, "verticesTexture"), 3);
        glUniform1i(glGetUniformLocation(shaderObject, "normalsTexture"), 4);
        glUniform1i(glGetUniformLocation(shaderObject, "materialsTexture"), 5);
        glUniform1i(glGetUniformLocation(shaderObject, "transformsTexture"), 6);
        glUniform1i(glGetUniformLocation(shaderObject, "lightsTexture"), 7);
        glUniform1i(glGetUniformLocation(shaderObject, "textureMapsArrayTexture"), 8);
        glUniform1i(glGetUniformLocation(shaderObject, "environmentMapTexture"), 9);
        glUniform1i(glGetUniformLocation(shaderObject, "environmentMapCDFTexture"), 10);
        shader->StopUsing();
    }

    void Renderer::SetupTAAShaderUniforms()
    {
        taaShader->Use();
        GLuint shaderObject = taaShader->getObject();
        glUniform2f(glGetUniformLocation(shaderObject, "resolution"), float(renderSize.x), float(renderSize.y));
        glUniform1i(glGetUniformLocation(shaderObject, "currentColorTexture"), 0);
        glUniform1i(glGetUniformLocation(shaderObject, "velocityTexture"), 1);
        glUniform1i(glGetUniformLocation(shaderObject, "historyTexture"), 2);
        glUniform1i(glGetUniformLocation(shaderObject, "currentPositionTexture"), 3);
        glUniform1i(glGetUniformLocation(shaderObject, "currentNormalTexture"), 4);
        glUniform1i(glGetUniformLocation(shaderObject, "historyPositionTexture"), 5);
        glUniform1i(glGetUniformLocation(shaderObject, "historyNormalTexture"), 6);
        taaShader->StopUsing();
    }

    void Renderer::Render()
    {
        // If maxSpp was reached then stop rendering. 
        if (!scene->dirty && scene->renderOptions.maxSpp != -1 && sampleCounter >= scene->renderOptions.maxSpp)
            return;

        if (previousTemporalMode != scene->renderOptions.temporalMode)
            ResetTemporalHistory();

        // The temporal pass reuses texture units 1-6. Restore all scene data
        // bindings before every path-tracing draw.
        BindSceneDataTextures();
        glActiveTexture(GL_TEXTURE0);

        if (scene->dirty)
        {
            pathTraceShaderTAA->Use();
            GLuint pathShaderObject = pathTraceShaderTAA->getObject();
            int temporalSpp = std::max(1, std::min(scene->renderOptions.temporalSpp, 8));
            int normalizedSeed = ((scene->renderOptions.temporalSeed % 1000) + 1000) % 1000;
            int deterministicFrame = normalizedSeed * 100003 + temporalFrameCounter;
            glUniform1i(glGetUniformLocation(pathShaderObject, "temporalSamplesPerPixel"), temporalSpp);
            glUniform1i(glGetUniformLocation(pathShaderObject, "frameNumber"), deterministicFrame);
            pathTraceShaderTAA->StopUsing();

            // Render a full-resolution low-SPP path-traced frame and its G-buffer.
            glBindFramebuffer(GL_FRAMEBUFFER, accumulationFramebuffer);
            GLenum pathDrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                         GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(4, pathDrawBuffers);
            glViewport(0, 0, renderSize.x, renderSize.y);
            glBeginQuery(GL_TIME_ELAPSED, timingQueries[0]);
            quad->Draw(pathTraceShaderTAA);
            glEndQuery(GL_TIME_ELAPSED);

            // Reproject and reconstruct temporal history.
            glBindFramebuffer(GL_FRAMEBUFFER, taaFramebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, historyTexture[currentHistoryBuffer], 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, historyPositionTexture[currentHistoryBuffer], 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, historyNormalTexture[currentHistoryBuffer], 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, temporalDiagnosticsTexture, 0);
            GLenum temporalDrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                             GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(4, temporalDrawBuffers);
            glViewport(0, 0, renderSize.x, renderSize.y);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, accumTexture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, velocityTexture);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, historyTexture[1 - currentHistoryBuffer]);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, positionTexture);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, normalTexture);
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, historyPositionTexture[1 - currentHistoryBuffer]);
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, historyNormalTexture[1 - currentHistoryBuffer]);

            taaShader->Use();
            GLuint taaObject = taaShader->getObject();
            glUniform1i(glGetUniformLocation(taaObject, "temporalMode"), scene->renderOptions.temporalMode);
            glUniform1i(glGetUniformLocation(taaObject, "currentSampleCount"), temporalSpp);
            glUniform1i(glGetUniformLocation(taaObject, "historyValid"), temporalHistoryValid ? 1 : 0);
            glUniform1f(glGetUniformLocation(taaObject, "historyWeight"), scene->renderOptions.historyWeight);
            glUniform1f(glGetUniformLocation(taaObject, "depthThreshold"), scene->renderOptions.depthThreshold);
            glUniform1f(glGetUniformLocation(taaObject, "normalThreshold"), scene->renderOptions.normalThreshold);
            glUniform1f(glGetUniformLocation(taaObject, "luminanceTolerance"), scene->renderOptions.luminanceTolerance);
            glUniform1f(glGetUniformLocation(taaObject, "motionDecay"), scene->renderOptions.motionDecay);
            glUniform1f(glGetUniformLocation(taaObject, "spatialFilterStrength"), scene->renderOptions.spatialFilterStrength);
            glUniform3f(glGetUniformLocation(taaObject, "previousCameraPosition"),
                        previousCameraPosition.x, previousCameraPosition.y, previousCameraPosition.z);
            glUniform3f(glGetUniformLocation(taaObject, "previousCameraForward"),
                        previousCameraForward.x, previousCameraForward.y, previousCameraForward.z);
            glBeginQuery(GL_TIME_ELAPSED, timingQueries[1]);
            quad->Draw(taaShader);
            glEndQuery(GL_TIME_ELAPSED);

            currentHistoryBuffer = 1 - currentHistoryBuffer;
            temporalHistoryValid = true;
            previousTemporalMode = scene->renderOptions.temporalMode;
            temporalFrameCounter++;
            previousCameraPosition = scene->camera->position;
            previousCameraForward = scene->camera->forward;

            GLuint64 elapsedNanoseconds = 0;
            glGetQueryObjectui64v(timingQueries[0], GL_QUERY_RESULT, &elapsedNanoseconds);
            pathTraceMilliseconds = elapsedNanoseconds / 1000000.0;
            glGetQueryObjectui64v(timingQueries[1], GL_QUERY_RESULT, &elapsedNanoseconds);
            temporalMilliseconds = elapsedNanoseconds / 1000000.0;

            scene->instancesModified = false;
            scene->dirty = false;
            scene->envMapModified = false;
        }
        else
        {
            // Renders to pathTraceTexture while using previously accumulated samples from accumTexture
            // Rendering is done a tile per frame, so if a 500x500 image is rendered with a tileWidth and tileHeight of 250 then, all tiles (for a single sample) 
            // get rendered after 4 frames
            glBindFramebuffer(GL_FRAMEBUFFER, pathTraceFramebuffer);
            glViewport(0, 0, tileWidth, tileHeight);
            glBindTexture(GL_TEXTURE_2D, accumTexture);
            quad->Draw(pathTraceShader);

            // pathTraceTexture is copied to accumTexture and re-used as input for the first step.
            glBindFramebuffer(GL_FRAMEBUFFER, accumulationFramebuffer);
            GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, drawBuffers);
            glViewport(tileWidth * tile.x, tileHeight * tile.y, tileWidth, tileHeight);
            glBindTexture(GL_TEXTURE_2D, pathTraceTexture);
            quad->Draw(outputShader);

            // Here we render to tileOutputTexture[currentBuffer] but display tileOutputTexture[1-currentBuffer] until all tiles are done rendering
            // When all tiles are rendered, we flip the bound texture and start rendering to the other one
            glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileOutputTexture[currentBuffer], 0);
            glViewport(0, 0, renderSize.x, renderSize.y);
            glBindTexture(GL_TEXTURE_2D, accumTexture);
            quad->Draw(tonemapShader);
        }
    }

    void Renderer::Present()
    {
        glActiveTexture(GL_TEXTURE0);

        if (debugView != 0)
        {
            GLuint texture = 0;
            int shaderMode = 0;
            bool useTonemapping = false;
            switch (debugView)
            {
            case 1: texture = accumTexture; useTonemapping = true; break;
            case 2: texture = historyTexture[1 - currentHistoryBuffer]; useTonemapping = true; break;
            case 3: texture = positionTexture; shaderMode = 0; break;
            case 4: texture = normalTexture; shaderMode = 1; break;
            case 5: texture = velocityTexture; shaderMode = 2; break;
            case 6: texture = temporalDiagnosticsTexture; shaderMode = 3; break;
            case 7: texture = temporalDiagnosticsTexture; shaderMode = 4; break;
            case 8: texture = temporalDiagnosticsTexture; shaderMode = 5; break;
            case 9: texture = historyTexture[1 - currentHistoryBuffer]; shaderMode = 7; break;
            case 10: texture = temporalDiagnosticsTexture; shaderMode = 6; break;
            default: texture = historyTexture[1 - currentHistoryBuffer]; useTonemapping = true; break;
            }

            glBindTexture(GL_TEXTURE_2D, texture);
            if (useTonemapping)
                quad->Draw(tonemapShader);
            else
            {
                debugShader->Use();
                glUniform1i(glGetUniformLocation(debugShader->getObject(), "debugMode"), shaderMode);
                quad->Draw(debugShader);
            }
            return;
        }

        // For the first sample or if the camera is moving, we do not have an image ready with all the tiles rendered, so we display a low res preview.
        if (scene->dirty || sampleCounter == 1)
        {
            glBindTexture(GL_TEXTURE_2D, historyTexture[1 - currentHistoryBuffer]);
            quad->Draw(tonemapShader);
        }
        else
        {
            if (scene->renderOptions.enableDenoiser && isDenoised)
                glBindTexture(GL_TEXTURE_2D, denoisedTexture);
            else
                glBindTexture(GL_TEXTURE_2D, tileOutputTexture[1 - currentBuffer]);

            quad->Draw(outputShader);
        }
    }

    float Renderer::GetProgress()
    {
        int maxSpp = scene->renderOptions.maxSpp;
        return maxSpp <= 0 ? 0.0f : sampleCounter * 100.0f / maxSpp;
    }

    void Renderer::GetOutputBuffer(unsigned char** data, int& width, int& height)
    {
        width = renderSize.x;
        height = renderSize.y;

        *data = new unsigned char[width * height * 4];

        glActiveTexture(GL_TEXTURE0);

        if (temporalHistoryValid && sampleCounter == 1)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   tileOutputTexture[currentBuffer], 0);
            glViewport(0, 0, renderSize.x, renderSize.y);
            glBindTexture(GL_TEXTURE_2D, historyTexture[1 - currentHistoryBuffer]);
            quad->Draw(tonemapShader);
            glBindTexture(GL_TEXTURE_2D, tileOutputTexture[currentBuffer]);
        }
        else if (scene->renderOptions.enableDenoiser && isDenoised)
            glBindTexture(GL_TEXTURE_2D, denoisedTexture);
        else
            glBindTexture(GL_TEXTURE_2D, tileOutputTexture[1 - currentBuffer]);

        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, *data);
    }

    void Renderer::ResetTemporalHistory()
    {
        temporalHistoryValid = false;
        temporalFrameCounter = 0;
        currentHistoryBuffer = 0;
        previousTemporalMode = scene ? scene->renderOptions.temporalMode : -1;
        pathTraceMilliseconds = 0.0;
        temporalMilliseconds = 0.0;
        if (scene && scene->camera)
        {
            previousCameraPosition = scene->camera->position;
            previousCameraForward = scene->camera->forward;
        }
    }

    void Renderer::WriteTexturePFM(GLuint texture, const std::string& filename, int channels)
    {
        (void)channels;
        std::vector<float> rgba(renderSize.x * renderSize.y * 4);
        std::vector<float> rgb(renderSize.x * renderSize.y * 3);

        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, rgba.data());
        for (int pixel = 0; pixel < renderSize.x * renderSize.y; ++pixel)
        {
            rgb[pixel * 3 + 0] = rgba[pixel * 4 + 0];
            rgb[pixel * 3 + 1] = rgba[pixel * 4 + 1];
            rgb[pixel * 3 + 2] = rgba[pixel * 4 + 2];
        }

        std::ofstream file(filename, std::ios::binary);
        if (!file)
        {
            std::cerr << "Unable to write research frame: " << filename << std::endl;
            return;
        }
        // Negative scale denotes little-endian samples. OpenGL and PFM both use
        // a bottom-left image origin, so the row order is preserved.
        file << "PF\n" << renderSize.x << " " << renderSize.y << "\n-1.0\n";
        file.write(reinterpret_cast<const char*>(rgb.data()),
                   static_cast<std::streamsize>(rgb.size() * sizeof(float)));
    }

    void Renderer::ExportResearchFrame(const std::string& prefix)
    {
        if (!temporalHistoryValid)
            return;

        int completedHistory = 1 - currentHistoryBuffer;
        WriteTexturePFM(historyTexture[completedHistory], prefix + "_color.pfm", 4);
        WriteTexturePFM(historyPositionTexture[completedHistory], prefix + "_position.pfm", 4);
        WriteTexturePFM(historyNormalTexture[completedHistory], prefix + "_normal.pfm", 4);
        WriteTexturePFM(velocityTexture, prefix + "_motion.pfm", 2);
        WriteTexturePFM(temporalDiagnosticsTexture, prefix + "_diagnostics.pfm", 4);
    }

    int Renderer::GetSampleCount()
    {
        return sampleCounter;
    }

    void Renderer::Update(float deltaTime)
    {
        // If maxSpp was reached then stop updates
        if (!scene->dirty && scene->renderOptions.maxSpp != -1 && sampleCounter >= scene->renderOptions.maxSpp)
            return;

        // Update data for instances
        if (scene->instancesModified)
        {
            // Update transforms
            glBindTexture(GL_TEXTURE_2D, transformsTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Mat4) / sizeof(Vec4)) * scene->transforms.size(), 1, 0, GL_RGBA, GL_FLOAT, &scene->transforms[0]);

            // Update materials
            glBindTexture(GL_TEXTURE_2D, materialsTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * scene->materials.size(), 1, 0, GL_RGBA, GL_FLOAT, &scene->materials[0]);

            // Update top level BVH
            int index = scene->bvhTranslator.topLevelIndex;
            int offset = sizeof(RadeonRays::BvhTranslator::Node) * index;
            int size = sizeof(RadeonRays::BvhTranslator::Node) * (scene->bvhTranslator.nodes.size() - index);
            glBindBuffer(GL_TEXTURE_BUFFER, bvhBuffer);
            glBufferSubData(GL_TEXTURE_BUFFER, offset, size, &scene->bvhTranslator.nodes[index]);
        }

        // Recreate texture for envmaps
        if (scene->envMapModified)
        {
            // Create texture for environment map
            if (scene->envMap != nullptr)
            {
                glBindTexture(GL_TEXTURE_2D, environmentMapTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, scene->envMap->width, scene->envMap->height, 0, GL_RGB, GL_FLOAT, scene->envMap->img);

                glBindTexture(GL_TEXTURE_2D, environmentMapCDFTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene->envMap->width, scene->envMap->height, 0, GL_RED, GL_FLOAT, scene->envMap->cdf);

                GLuint shaderObject;
                pathTraceShader->Use();
                shaderObject = pathTraceShader->getObject();
                glUniform2f(glGetUniformLocation(shaderObject, "environmentMapResolution"), (float)scene->envMap->width, (float)scene->envMap->height);
                glUniform1f(glGetUniformLocation(shaderObject, "environmentMapTotalSum"), scene->envMap->totalSum);
                pathTraceShader->StopUsing();

                pathTraceShaderLowRes->Use();
                shaderObject = pathTraceShaderLowRes->getObject();
                glUniform2f(glGetUniformLocation(shaderObject, "environmentMapResolution"), (float)scene->envMap->width, (float)scene->envMap->height);
                glUniform1f(glGetUniformLocation(shaderObject, "environmentMapTotalSum"), scene->envMap->totalSum);
                pathTraceShaderLowRes->StopUsing();
            }
        }

        // Denoise image if requested
        if (scene->renderOptions.enableDenoiser && sampleCounter > 1)
        {
            if (!isDenoised || (frameCounter % (scene->renderOptions.denoiserFrameCnt * (numTiles.x * numTiles.y)) == 0))
            {
                glBindTexture(GL_TEXTURE_2D, tileOutputTexture[1 - currentBuffer]);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, denoiserInputFramePointer);

                // Create an Intel Open Image Denoise device
                oidn::DeviceRef device = oidn::newDevice();
                device.commit();

                // Create a denoising filter
                oidn::FilterRef filter = device.newFilter("RT"); // generic ray tracing filter
                filter.setImage("color", denoiserInputFramePointer, oidn::Format::Float3, renderSize.x, renderSize.y, 0, 0, 0);
                filter.setImage("output", frameOutputPointer, oidn::Format::Float3, renderSize.x, renderSize.y, 0, 0, 0);
                filter.set("hdr", false);
                filter.commit();

                // Filter the image
                filter.execute();

                const char* errorMessage;
                if (device.getError(errorMessage) != oidn::Error::None)
                    std::cout << "Error: " << errorMessage << std::endl;

                // Copy the denoised data to denoisedTexture
                glBindTexture(GL_TEXTURE_2D, denoisedTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, renderSize.x, renderSize.y, 0, GL_RGB, GL_FLOAT, frameOutputPointer);

                isDenoised = true;
            }
        }
        else
            isDenoised = false;

        // If scene was modified then clear out image for re-rendering
        if (scene->dirty)
        {
            tile.x = -1;
            tile.y = numTiles.y - 1;
            sampleCounter = 1;
            isDenoised = false;
            frameCounter = 1;

            // Clear out the accumulated texture for rendering a new image
            glBindFramebuffer(GL_FRAMEBUFFER, accumulationFramebuffer);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else // Update render state
        {
            frameCounter++;
            tile.x++;
            if (tile.x >= numTiles.x)
            {
                tile.x = 0;
                tile.y--;
                if (tile.y < 0)
                {
                    tile.x = 0;
                    tile.y = numTiles.y - 1;
                    sampleCounter++;
                    currentBuffer = 1 - currentBuffer;
                }
            }
        }

        // Update uniforms

        // Calculate ViewProj
        float view[16];
        float proj[16];
        scene->camera->ComputeViewProjectionMatrix(view, proj, (float)renderSize.x / renderSize.y);

        Mat4 viewMat, projMat;
        for (int i = 0; i < 16; i++) {
            viewMat.data[i / 4][i % 4] = view[i];
            projMat.data[i / 4][i % 4] = proj[i];
        }
        Mat4 viewProj = viewMat * projMat;

        GLuint shaderObject;
        pathTraceShader->Use();
        shaderObject = pathTraceShader->getObject();
        glUniform3f(glGetUniformLocation(shaderObject, "camera.position"), scene->camera->position.x, scene->camera->position.y, scene->camera->position.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.right"), scene->camera->right.x, scene->camera->right.y, scene->camera->right.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.up"), scene->camera->up.x, scene->camera->up.y, scene->camera->up.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.forward"), scene->camera->forward.x, scene->camera->forward.y, scene->camera->forward.z);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.fov"), scene->camera->fov);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.focalDist"), scene->camera->focalDist);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.aperture"), scene->camera->aperture);
        glUniform1i(glGetUniformLocation(shaderObject, "enableEnvMap"), scene->envMap == nullptr ? false : scene->renderOptions.enableEnvMap);
        glUniform1f(glGetUniformLocation(shaderObject, "environmentMapIntensity"), scene->renderOptions.envMapIntensity);
        glUniform1f(glGetUniformLocation(shaderObject, "environmentMapRotation"), scene->renderOptions.envMapRot / 360.0f);
        glUniform1i(glGetUniformLocation(shaderObject, "maxDepth"), scene->renderOptions.maxDepth);
        glUniform2f(glGetUniformLocation(shaderObject, "tilePerOffset"), (float)tile.x * invNumTiles.x, (float)tile.y * invNumTiles.y);
        glUniform3f(glGetUniformLocation(shaderObject, "uniformLightColor"), scene->renderOptions.uniformLightCol.x, scene->renderOptions.uniformLightCol.y, scene->renderOptions.uniformLightCol.z);
        glUniform1f(glGetUniformLocation(shaderObject, "roughnessMollificationAmount"), scene->renderOptions.roughnessMollificationAmt);
        glUniform1i(glGetUniformLocation(shaderObject, "frameNumber"), frameCounter);   
        pathTraceShader->StopUsing();

        pathTraceShaderLowRes->Use();
        shaderObject = pathTraceShaderLowRes->getObject();
        glUniform3f(glGetUniformLocation(shaderObject, "camera.position"), scene->camera->position.x, scene->camera->position.y, scene->camera->position.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.right"), scene->camera->right.x, scene->camera->right.y, scene->camera->right.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.up"), scene->camera->up.x, scene->camera->up.y, scene->camera->up.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.forward"), scene->camera->forward.x, scene->camera->forward.y, scene->camera->forward.z);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.fov"), scene->camera->fov);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.focalDist"), scene->camera->focalDist);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.aperture"), scene->camera->aperture);
        glUniform1i(glGetUniformLocation(shaderObject, "enableEnvMap"), scene->envMap == nullptr ? false : scene->renderOptions.enableEnvMap);
        glUniform1f(glGetUniformLocation(shaderObject, "environmentMapIntensity"), scene->renderOptions.envMapIntensity);
        glUniform1f(glGetUniformLocation(shaderObject, "environmentMapRotation"), scene->renderOptions.envMapRot / 360.0f);
        glUniform1i(glGetUniformLocation(shaderObject, "maxDepth"), scene->renderOptions.maxDepth);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.position"), scene->camera->position.x, scene->camera->position.y, scene->camera->position.z);
        glUniform3f(glGetUniformLocation(shaderObject, "uniformLightColor"), scene->renderOptions.uniformLightCol.x, scene->renderOptions.uniformLightCol.y, scene->renderOptions.uniformLightCol.z);
        glUniform1f(glGetUniformLocation(shaderObject, "roughnessMollificationAmount"), scene->renderOptions.roughnessMollificationAmt);
        pathTraceShaderLowRes->StopUsing();

        pathTraceShaderTAA->Use();
        shaderObject = pathTraceShaderTAA->getObject();
        glUniform3f(glGetUniformLocation(shaderObject, "camera.position"), scene->camera->position.x, scene->camera->position.y, scene->camera->position.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.right"), scene->camera->right.x, scene->camera->right.y, scene->camera->right.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.up"), scene->camera->up.x, scene->camera->up.y, scene->camera->up.z);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.forward"), scene->camera->forward.x, scene->camera->forward.y, scene->camera->forward.z);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.fov"), scene->camera->fov);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.focalDist"), scene->camera->focalDist);
        glUniform1f(glGetUniformLocation(shaderObject, "camera.aperture"), scene->camera->aperture);
        glUniform1i(glGetUniformLocation(shaderObject, "enableEnvMap"), scene->envMap == nullptr ? false : scene->renderOptions.enableEnvMap);
        glUniform1f(glGetUniformLocation(shaderObject, "environmentMapIntensity"), scene->renderOptions.envMapIntensity);
        glUniform1f(glGetUniformLocation(shaderObject, "environmentMapRotation"), scene->renderOptions.envMapRot / 360.0f);
        glUniform1i(glGetUniformLocation(shaderObject, "maxDepth"), scene->renderOptions.maxDepth);
        glUniform3f(glGetUniformLocation(shaderObject, "camera.position"), scene->camera->position.x, scene->camera->position.y, scene->camera->position.z);
        glUniform3f(glGetUniformLocation(shaderObject, "uniformLightColor"), scene->renderOptions.uniformLightCol.x, scene->renderOptions.uniformLightCol.y, scene->renderOptions.uniformLightCol.z);
        glUniform1f(glGetUniformLocation(shaderObject, "roughnessMollificationAmount"), scene->renderOptions.roughnessMollificationAmt);
        glUniformMatrix4fv(glGetUniformLocation(shaderObject, "viewProjectionMatrix"), 1, GL_FALSE, &viewProj.data[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderObject, "previousViewProjectionMatrix"), 1, GL_FALSE, &previousViewProjectionMatrix.data[0][0]);
        pathTraceShaderTAA->StopUsing();

        previousViewProjectionMatrix = viewProj;

        tonemapShader->Use();
        shaderObject = tonemapShader->getObject();
        glUniform1f(glGetUniformLocation(shaderObject, "inverseSampleCounter"), 1.0f / (sampleCounter));
        glUniform1i(glGetUniformLocation(shaderObject, "enableTonemap"), scene->renderOptions.enableTonemap);
        glUniform1i(glGetUniformLocation(shaderObject, "enableAces"), scene->renderOptions.enableAces);
        glUniform1i(glGetUniformLocation(shaderObject, "simpleAcesFit"), scene->renderOptions.simpleAcesFit);
        glUniform3f(glGetUniformLocation(shaderObject, "backgroundColor"), scene->renderOptions.backgroundCol.x, scene->renderOptions.backgroundCol.y, scene->renderOptions.backgroundCol.z);
        glUniform1f(glGetUniformLocation(shaderObject, "exposure"), scene->renderOptions.exposure);
        tonemapShader->StopUsing();
    }
}


/*作用：GPU渲染的"控制中心"
职责：
- 管理所有OpenGL资源（纹理、缓冲区、帧缓冲）
- 编译和管理着色器程序
- 实现分块渐进式渲染管线
- 集成TAA和降噪后处理
- 处理CPU-GPU数据传输

关键资源：
├── GLuint纹理系列：bvhTexture、verticesTexture等
├── GLuint帧缓冲：pathTraceFramebuffer、accumulationFramebuffer
├── Program指针：pathTraceShader、tonemapShader等
└── Quad* quad - 全屏四边形渲染器

关键函数：
├── Render()      - 执行一帧渲染
├── Update()      - 更新渲染状态
├── Present()     - 显示到屏幕
└── Resize()      - 处理窗口大小变化*/
