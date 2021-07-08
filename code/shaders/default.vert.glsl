#version 450

layout (location = 0) in vec3 LocalPosition;
layout (location = 1) in vec3 LocalNormal;

void main()
{
	gl_Position = vec4(LocalPosition, 1.0);
}