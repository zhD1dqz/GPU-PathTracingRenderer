
#pragma once

#include "PTConfig.h"

namespace PathTraceAlg
{
    class Program;

    class Quad
    {
    public:
        Quad();
        void Draw(Program*);

    private:
        GLuint vao;
        GLuint vbo;
    };
}