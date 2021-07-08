#version 450

layout (location = 0) in vec3 LocalPosition;
layout (location = 1) in vec3 LocalNormal;

layout (location = 0) out vec3 Color;

layout (set = 0, binding = 0) uniform CameraBuffer
{
	mat4 View;
	mat4 Proj;
};

void main()
{
	Color = LocalNormal;
	gl_Position = Proj * View * vec4(LocalPosition, 1.0);
}