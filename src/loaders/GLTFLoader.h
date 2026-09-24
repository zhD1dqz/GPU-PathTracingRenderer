
#pragma once

#include "PTScene.h"

namespace PathTraceAlg
{
    class Scene;

    bool LoadGLTF(const std::string& filename, Scene* scene, RenderOptions& renderOptions, Mat4 xform, bool binary);
}