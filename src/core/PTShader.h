

#pragma once

#include <string>
#include "PTShaderIncludes.h"
#include "PTConfig.h"

namespace PathTraceAlg
{
    class Shader
    {
    private:
        GLuint object;
    public:
        Shader(const ShaderInclude::ShaderSource& sourceObj, GLuint shaderType);
        GLuint getObject() const;
    };
}