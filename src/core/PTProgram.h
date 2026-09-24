
#pragma once

#include <vector>
#include "PTShader.h"

namespace PathTraceAlg
{
    class Program
    {
    private:
        GLuint object;

    public:
        Program(const std::vector<Shader> shaders);
        ~Program();
        void Use();
        void StopUsing();
        GLuint getObject();
    };
}
