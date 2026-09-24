#version 330

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 historyPositionOut;
layout(location = 2) out vec4 historyNormalOut;
layout(location = 3) out vec4 diagnosticsOut;
in vec2 TexCoords;

uniform sampler2D currentColorTexture;
uniform sampler2D velocityTexture;
uniform sampler2D currentPositionTexture;
uniform sampler2D currentNormalTexture;
uniform sampler2D historyTexture;
uniform sampler2D historyPositionTexture;
uniform sampler2D historyNormalTexture;
uniform vec2 resolution;
uniform vec3 previousCameraPosition;
uniform vec3 previousCameraForward;
uniform int temporalMode;
uniform int currentSampleCount;
uniform bool historyValid;
uniform float historyWeight;
uniform float depthThreshold;
uniform float normalThreshold;
uniform float luminanceTolerance;
uniform float motionDecay;
uniform float spatialFilterStrength;

float Luminance(vec3 c) { return dot(c, vec3(0.212671, 0.715160, 0.072169)); }

float GeometryConfidence(vec4 currentPosition, vec4 currentNormal,
                         vec4 historyPosition, vec4 historyNormal)
{
    bool currentHit = currentNormal.w > 0.5;
    bool historyHit = historyNormal.w > 0.5;
    if (currentHit != historyHit) return 0.0;
    if (!currentHit) return 1.0;

    float expectedDepth = dot(currentPosition.xyz - previousCameraPosition,
                              previousCameraForward);
    float storedDepth = dot(historyPosition.xyz - previousCameraPosition,
                            previousCameraForward);
    float depthDifference = abs(expectedDepth - storedDepth) /
                            max(abs(expectedDepth), 1e-3);
    float normalSimilarity = dot(normalize(currentNormal.xyz),
                                 normalize(historyNormal.xyz));
    if (expectedDepth <= 0.0 || depthDifference > depthThreshold ||
        normalSimilarity < normalThreshold) return 0.0;

    // Preserve more valid history inside the hard rejection boundary. The hard
    // tests above still prevent cross-surface ghosting at disocclusions.
    float cd = exp(-1.5 * depthDifference / max(depthThreshold, 1e-5));
    float cn = exp(-1.5 * (1.0 - normalSimilarity) /
                   max(1.0 - normalThreshold, 1e-5));
    return cd * cn;
}

void main()
{
    vec3 currentColor = texture(currentColorTexture, TexCoords).rgb;
    vec4 currentPosition = texture(currentPositionTexture, TexCoords);
    vec4 currentNormal = texture(currentNormalTexture, TexCoords);
    vec2 velocity = texture(velocityTexture, TexCoords).xy;
    vec2 previousUV = TexCoords - velocity;

    historyPositionOut = currentPosition;
    historyNormalOut = currentNormal;

    bool inBounds = all(greaterThanEqual(previousUV, vec2(0.0))) &&
                    all(lessThanEqual(previousUV, vec2(1.0)));
    if (temporalMode == 0 || !historyValid || !inBounds)
    {
        color = vec4(currentColor, temporalMode == 7 ? float(currentSampleCount) : 1.0);
        diagnosticsOut = vec4(0.0, 0.0, 0.0,
                              min(length(velocity * resolution) / 20.0, 1.0));
        return;
    }

    // Reference mode holds the camera pose fixed and forms an unbiased running
    // average. It intentionally bypasses rejection and neighborhood clipping.
    if (temporalMode == 7)
    {
        vec4 oldColor = texture(historyTexture, TexCoords);
        float oldLength = max(oldColor.a, 1.0);
        float newSamples = float(max(currentSampleCount, 1));
        float referenceWeight = oldLength / (oldLength + newSamples);
        color = vec4(mix(currentColor, oldColor.rgb, referenceWeight),
                     min(oldLength + newSamples, 4096.0));
        diagnosticsOut = vec4(referenceWeight, 1.0, 1.0, 0.0);
        return;
    }

    // Fixed-weight baseline: preserve the original 3x3 neighborhood clamp and
    // avoid executing any proposed geometry/confidence work.
    if (temporalMode == 1)
    {
        vec3 minimumColor = vec3(1e20);
        vec3 maximumColor = vec3(-1e20);
        for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            vec3 neighbor = textureOffset(currentColorTexture, TexCoords,
                                          ivec2(x, y)).rgb;
            minimumColor = min(minimumColor, neighbor);
            maximumColor = max(maximumColor, neighbor);
        }
        vec4 oldColor = texture(historyTexture, previousUV);
        vec3 clampedHistory = clamp(oldColor.rgb, minimumColor, maximumColor);
        color = vec4(mix(currentColor, clampedHistory, historyWeight),
                     min(oldColor.a + 1.0, 64.0));
        diagnosticsOut = vec4(historyWeight, 1.0, 1.0,
                              min(length(velocity * resolution) / 20.0, 1.0));
        return;
    }

    // Geometry-aware gather: validate the four history texels before mixing.
    ivec2 size = textureSize(historyTexture, 0);
    vec2 samplePosition = previousUV * vec2(size) - vec2(0.5);
    ivec2 base = ivec2(floor(samplePosition));
    vec2 fraction = fract(samplePosition);
    vec3 gatheredHistory = vec3(0.0);
    float gatheredLength = 0.0;
    float confidenceSum = 0.0;
    float bilinearSum = 0.0;

    for (int y = 0; y <= 1; ++y)
    for (int x = 0; x <= 1; ++x)
    {
        ivec2 coordinate = base + ivec2(x, y);
        if (any(lessThan(coordinate, ivec2(0))) ||
            any(greaterThanEqual(coordinate, size))) continue;
        float wx = x == 0 ? 1.0 - fraction.x : fraction.x;
        float wy = y == 0 ? 1.0 - fraction.y : fraction.y;
        float bilinearWeight = wx * wy;
        vec4 oldColor = texelFetch(historyTexture, coordinate, 0);
        vec4 oldPosition = texelFetch(historyPositionTexture, coordinate, 0);
        vec4 oldNormal = texelFetch(historyNormalTexture, coordinate, 0);
        float cg = GeometryConfidence(currentPosition, currentNormal,
                                      oldPosition, oldNormal);
        float sampleWeight = bilinearWeight * cg;
        gatheredHistory += oldColor.rgb * sampleWeight;
        gatheredLength += oldColor.a * sampleWeight;
        confidenceSum += sampleWeight;
        bilinearSum += bilinearWeight;
    }

    float geometryConfidence = 1.0;
    if (confidenceSum > 1e-6)
    {
        gatheredHistory /= confidenceSum;
        gatheredLength /= confidenceSum;
        geometryConfidence = clamp(confidenceSum / max(bilinearSum, 1e-6), 0.0, 1.0);
    }
    else
    {
        // No valid history. Continue into the geometry-aware spatial filter
        // instead of returning a raw, noisy 1-SPP sample.
        gatheredHistory = currentColor;
        gatheredLength = 0.0;
        geometryConfidence = 0.0;
    }

    vec3 minimumColor = vec3(1e20);
    vec3 maximumColor = vec3(-1e20);
    vec3 filteredCurrentSum = vec3(0.0);
    float spatialWeightSum = 0.0;
    float sumL = 0.0, sumL2 = 0.0, count = 0.0;
    for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x)
    {
        ivec2 offset = ivec2(x, y);
        vec3 neighborColor = textureOffset(currentColorTexture, TexCoords, offset).rgb;
        vec4 neighborPosition = textureOffset(currentPositionTexture, TexCoords, offset);
        vec4 neighborNormal = textureOffset(currentNormalTexture, TexCoords, offset);
        bool sameSurface = (neighborNormal.w > 0.5) == (currentNormal.w > 0.5);
        if (sameSurface && currentNormal.w > 0.5)
        {
            float nd = dot(normalize(currentNormal.xyz), normalize(neighborNormal.xyz));
            float pd = length(currentPosition.xyz - neighborPosition.xyz) /
                       max(currentPosition.w, 1e-3);
            sameSurface = nd >= normalThreshold && pd <= depthThreshold;
        }
        if (sameSurface)
        {
            float l = Luminance(neighborColor);
            sumL += l; sumL2 += l * l; count += 1.0;
            minimumColor = min(minimumColor, neighborColor);
            maximumColor = max(maximumColor, neighborColor);
            float spatialWeight = exp(-0.5 * float(x * x + y * y));
            filteredCurrentSum += neighborColor * spatialWeight;
            spatialWeightSum += spatialWeight;
        }
    }

    vec3 filteredCurrent = filteredCurrentSum / max(spatialWeightSum, 1e-6);
    float meanL = sumL / max(count, 1.0);
    float variance = max(sumL2 / max(count, 1.0) - meanL * meanL, 0.0);
    float residual = abs(Luminance(gatheredHistory) - meanL) /
                     sqrt(variance + 1e-4);
    float excess = max(residual - luminanceTolerance, 0.0);
    float luminanceConfidence = exp(-2.0 * excess * excess);
    float motionPixels = length(velocity * resolution);
    float motionConfidence = exp(-motionDecay * motionPixels);
    gatheredHistory = clamp(gatheredHistory, minimumColor, maximumColor);

    float weight = historyWeight;
    float historyRamp = min(historyWeight, gatheredLength / (gatheredLength + 1.0));
    if (temporalMode == 2)
        weight *= geometryConfidence;
    else if (temporalMode == 3)
        weight = historyRamp * geometryConfidence * luminanceConfidence * motionConfidence;
    else if (temporalMode == 4)
        weight = historyWeight * geometryConfidence * motionConfidence /
                 (1.0 + 8.0 * variance);
    else if (temporalMode == 5)
        weight = historyRamp * geometryConfidence * motionConfidence;
    else if (temporalMode == 6)
        weight = historyRamp * geometryConfidence * luminanceConfidence;

    weight = clamp(weight, 0.0, historyWeight);
    // When motion or disocclusion makes history unreliable, use a small
    // geometry-aware 3x3 reconstruction of the current frame. This reduces
    // motion noise without blending across depth/normal discontinuities.
    float historyReliability = weight / max(historyWeight, 1e-5);
    float spatialStrength = temporalMode == 3 ?
        spatialFilterStrength * (1.0 - historyReliability) : 0.0;
    vec3 reconstructedCurrent = mix(currentColor, filteredCurrent, spatialStrength);
    float newLength = mix(1.0, min(gatheredLength + 1.0, 64.0),
                          geometryConfidence * luminanceConfidence);
    color = vec4(mix(reconstructedCurrent, gatheredHistory, weight), newLength);
    diagnosticsOut = vec4(weight, geometryConfidence, luminanceConfidence,
                          min(motionPixels / 20.0, 1.0));
}
