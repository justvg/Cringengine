#version 460

layout (location = 0) in vec3 LocalPosition;
layout (location = 1) in vec3 LocalNormal;

layout (location = 0) out vec3 Color;

layout (set = 0, binding = 0) uniform CameraBuffer
{
	mat4 View;
	mat4 Proj;
};

struct SObjectData
{
	vec3 Position;
	float Scale;
	vec4 Orientation;
};

layout (set = 1, binding = 0) readonly buffer ObjectsData
{
	SObjectData ObjectData[];
};

vec3 RotateQuaternion(vec3 V, vec4 Q)
{
	return V + 2.0 * cross(Q.xyz, cross(Q.xyz, V) + Q.w * V);
}

void main()
{
	Color = LocalNormal;

	vec3 P = ObjectData[gl_BaseInstance].Position;
	float S = ObjectData[gl_BaseInstance].Scale;
	vec4 O = ObjectData[gl_BaseInstance].Orientation;

	gl_Position = Proj * View * vec4(RotateQuaternion(LocalPosition * S, O) + P, 1.0);
}