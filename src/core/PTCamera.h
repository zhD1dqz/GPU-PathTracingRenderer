#pragma once

#include <float.h>
#include "MathUtils.h"
#include "Vec3.h"

namespace PathTraceAlg
{
    class Camera
    {
    public:
        Camera(Vec3 eye, Vec3 lookat, float fov);
        Camera(const Camera& other);
        Camera& operator = (const Camera& other);

        void OffsetOrientation(float dx, float dy);
        void Strafe(float dx, float dy);
        void SetRadius(float dr);
        void Zoom(float wheelSteps);
        void SetLookAt(const Vec3& eye, const Vec3& lookat);
        Vec3 GetPivot() const { return pivot; }
        void ComputeViewProjectionMatrix(float* view, float* projection, float ratio);
        void SetFov(float val);

        Vec3 position;
        Vec3 up;
        Vec3 right;
        Vec3 forward;

        float focalDist;
        float aperture;
        float fov;
        bool isMoving;

    private:
        void UpdateCamera();

        Vec3 worldUp;
        Vec3 pivot;

        float pitch;
        float radius;
        float yaw;
    };
}
