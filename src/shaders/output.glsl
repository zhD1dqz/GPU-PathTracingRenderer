
#version 330

out vec4 color;
in vec2 TexCoords;

uniform sampler2D imageTexture;

void main()
{
    color = texture(imageTexture, TexCoords);
}