
#version 330

layout(location = 0) out vec4 color;
layout(location = 1) out vec2 velocity;
layout(location = 2) out vec4 positionAndHit;
layout(location = 3) out vec4 normalAndHit;
in vec2 TexCoords;

#include generic/globals.glsl
#include generic/modelintersect.glsl
#include generic/sampling.glsl
#include generic/envmap.glsl
#include generic/rayhit.glsl
#include generic/disney.glsl
#include generic/lambert.glsl
#include generic/pathtrace.glsl

uniform int temporalSamplesPerPixel;

vec4 TraceSample(int sampleIndex, out vec3 firstHitPos,
                 out vec3 firstHitNormal, out bool hitFirst)
{
    BeginInit(gl_FragCoord.xy, frameNumber * 8 + sampleIndex);

    float r1 = 2.0 * rand();
    float r2 = 2.0 * rand();

    vec2 jitter;
    jitter.x = r1 < 1.0 ? sqrt(r1) - 1.0 : 1.0 - sqrt(2.0 - r1);
    jitter.y = r2 < 1.0 ? sqrt(r2) - 1.0 : 1.0 - sqrt(2.0 - r2);

    jitter /= (resolution * 0.5);
    vec2 d = (2.0 * TexCoords - 1.0) + jitter;

    float scale = tan(camera.fov * 0.5);
    d.y *= resolution.y / resolution.x * scale;
    d.x *= scale;
    vec3 rayDir = normalize(d.x * camera.right + d.y * camera.up + camera.forward);

    vec3 focalPoint = camera.focalDist * rayDir;
    float cam_r1 = rand() * TWO_PI;
    float cam_r2 = rand() * camera.aperture;
    vec3 randomAperturePos = (cos(cam_r1) * camera.right + sin(cam_r1) * camera.up) * sqrt(cam_r2);
    vec3 finalRayDir = normalize(focalPoint - randomAperturePos);

    Ray ray = Ray(camera.position + randomAperturePos, finalRayDir);

    return PathTrace(ray, firstHitPos, firstHitNormal, hitFirst);
}

void main(void)
{
    vec4 pixelColor = vec4(0.0);
    vec3 firstHitPos = vec3(0.0);
    vec3 firstHitNormal = vec3(0.0);
    bool hitFirst = false;
    for (int sampleIndex = 0; sampleIndex < 8; ++sampleIndex)
    {
        if (sampleIndex >= temporalSamplesPerPixel) break;
        vec3 samplePosition, sampleNormal;
        bool sampleHit;
        pixelColor += TraceSample(sampleIndex, samplePosition, sampleNormal, sampleHit);
        if (sampleIndex == 0)
        {
            firstHitPos = samplePosition;
            firstHitNormal = sampleNormal;
            hitFirst = sampleHit;
        }
    }
    pixelColor /= float(max(temporalSamplesPerPixel, 1));

    color = pixelColor;
    positionAndHit = vec4(firstHitPos,
                          hitFirst ? length(firstHitPos - camera.position) : 0.0);
    normalAndHit = vec4(firstHitNormal, hitFirst ? 1.0 : 0.0);

    if (hitFirst)
    {
        vec4 currentPos = viewProjectionMatrix * vec4(firstHitPos, 1.0);
        vec4 prevPos = previousViewProjectionMatrix * vec4(firstHitPos, 1.0);
        
        vec2 currentNDC = currentPos.xy / currentPos.w;
        vec2 prevNDC = prevPos.xy / prevPos.w;
        
        // Transform NDC to UV [0, 1]
        vec2 currentUV = currentNDC * 0.5 + 0.5;
        vec2 prevUV = prevNDC * 0.5 + 0.5;

        velocity = currentUV - prevUV;
    }
    else
    {
        velocity = vec2(0.0);
    }
}
