#version 450

layout (location = 0) in vec3 LocalPosition;
layout (location = 1) in vec3 LocalNormal;

layout (location = 0) out vec3 Color;

layout (set = 0, binding = 0) uniform CameraBuffer
{
	mat4 View;
	mat4 Proj;
};

layout (push_constant) uniform PushConstants
{
	vec3 Position;
	float Scale;
	vec4 Orientation;
};

vec3 RotateQuaternion(vec3 V, vec4 Q)
{
	return V + 2.0 * cross(Q.xyz, cross(Q.xyz, V) + Q.w * V);
}

void main()
{
	Color = LocalNormal;
	gl_Position = Proj * View * vec4(RotateQuaternion(LocalPosition * Scale, Orientation) + Position, 1.0);
}