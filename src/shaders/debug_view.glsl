#version 330

out vec4 color;
in vec2 TexCoords;

uniform sampler2D debugTexture;
uniform int debugMode;

vec3 HeatMap(float value)
{
    value = clamp(value, 0.0, 1.0);
    return clamp(vec3(1.5) - abs(4.0 * value - vec3(3.0, 2.0, 1.0)),
                 0.0, 1.0);
}

void main()
{
    vec4 sampleValue = texture(debugTexture, TexCoords);
    vec3 result = vec3(0.0);

    if (debugMode == 0) // Depth
    {
        float depth = sampleValue.w;
        result = vec3(depth > 0.0 ? 1.0 - exp(-0.12 * depth) : 0.0);
    }
    else if (debugMode == 1) // World normal
    {
        result = sampleValue.w > 0.5 ? sampleValue.xyz * 0.5 + 0.5 : vec3(0.0);
    }
    else if (debugMode == 2) // Motion vector
    {
        result = vec3(abs(sampleValue.xy) * 20.0, 0.0);
    }
    else if (debugMode == 3) // History weight
        result = HeatMap(sampleValue.r);
    else if (debugMode == 4) // Geometry confidence
        result = HeatMap(sampleValue.g);
    else if (debugMode == 5) // Luminance confidence
        result = HeatMap(sampleValue.b);
    else if (debugMode == 6) // Normalized motion magnitude
        result = HeatMap(sampleValue.a);
    else if (debugMode == 7) // History length
        result = HeatMap(sampleValue.a / 64.0);

    color = vec4(result, 1.0);
}
