
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <string>
#include "PTEnvironmentMap.h"

namespace PathTraceAlg
{
    float Luminance(float r, float g, float b)
    {
        return 0.212671f * r + 0.715160f * g + 0.072169f * b;
    }

    void EnvironmentMap::BuildCDF()
    {
        // Gather weights for CDF
        float* weights = new float[width * height];
        for (int v = 0; v < height; v++)
        {
            for (int u = 0; u < width; u++)
            {
                int imgIdx = v * width * 3 + u * 3;
                weights[u + v * width] = Luminance(img[imgIdx + 0], img[imgIdx + 1], img[imgIdx + 2]);
            }
        }

        // Build CDF
        cdf = new float[width * height];
        cdf[0] = weights[0];
        for (int i = 1; i < width * height; i++)
            cdf[i] = cdf[i - 1] + weights[i];

        totalSum = cdf[width * height - 1];

        delete[] weights;
    }

    bool EnvironmentMap::LoadEnvMap(const std::string& filename)
    {
        img = stbi_loadf(filename.c_str(), &width, &height, NULL, 3);

        if (img == nullptr)
            return false;

        BuildCDF();

        return true;
    }
}


/*作用：天空盒的"智能采样器"
职责：
- 加载HDR环境贴图
- 构建重要性采样的CDF
- 提供环境光采样接口

关键数据：
├── float* img  - HDR图像数据
├── float* cdf  - 累积分布函数（用于重要性采样）
├── float totalSum - 总亮度
└── int width, height*/