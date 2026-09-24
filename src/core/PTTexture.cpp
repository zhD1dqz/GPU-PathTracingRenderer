
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "PTTexture.h"
#include "stb_image.h"

namespace PathTraceAlg
{
    Texture::Texture(std::string texName, unsigned char* data, int w, int h, int c) : name(texName)
        , width(w)
        , height(h)
        , components(c)
    {
        texData.resize(width * height * components);
        std::copy(data, data + width * height * components, texData.begin());
    }

    bool Texture::LoadTexture(const std::string& filename)
    {
        name = filename;
        components = 4;
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, NULL, components);
        if (data == nullptr)
            return false;
        texData.resize(width * height * components);
        std::copy(data, data + width * height * components, texData.begin());
        stbi_image_free(data);
        return true;
    }
}

/*作用：图片数据的"包装器"
职责：
- 从文件加载纹理图片
- 存储纹理像素数据
- 提供纹理参数（宽/高/通道数）

关键数据：
├── vector<unsigned char> texData - 纹理像素数据
├── int width, height, components
└── string name - 纹理名称/路径*/