
#pragma once

#include "PTScene.h"

namespace PathTraceAlg
{
    class Scene;

    bool LoadSceneFromFile(const std::string& filename, Scene* scene, RenderOptions& renderOptions);
}