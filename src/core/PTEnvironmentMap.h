#pragma once

#include <vector>
#include "MathUtils.h"
#include "stb_image.h"

namespace PathTraceAlg
{
    class EnvironmentMap
    {
    public:
        EnvironmentMap() : width(0), height(0), img(nullptr), cdf(nullptr) {};
        ~EnvironmentMap() { stbi_image_free(img); delete[] cdf; }

        bool LoadEnvMap(const std::string& filename);
        void BuildCDF();

        int width;
        int height;
        float totalSum;
        float* img;
        float* cdf;
    };
}
