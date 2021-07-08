#version 450

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 Color;

void main()
{
	FragColor = vec4(Color, 1.0);
}