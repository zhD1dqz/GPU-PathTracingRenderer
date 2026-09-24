
#pragma once

namespace PathTraceAlg
{
    struct iVec2
    {
    public:
        iVec2() { x = 0, y = 0; };
        iVec2(int x, int y) { this->x = x; this->y = y; };

        int x, y;
    };

    struct Vec2
    {
    public:
        Vec2() { x = 0, y = 0; };
        Vec2(float x, float y) { this->x = x; this->y = y; };

        float x, y;
    };
}

/*Vec2.h：
├── struct iVec2 - 整数二维向量（用于像素坐标）
└── struct Vec2  - 浮点二维向量（用于纹理坐标）

Vec3.h：最重要的向量类
├── 存储三维坐标、方向、颜色
├── 完整数学运算：+ - * / dot cross normalize
└── 静态工具函数：Min/Max/Clamp/Pow等

Vec4.h：
├── 四维向量（齐次坐标、RGBA颜色）
└── 主要用于顶点数据打包*/