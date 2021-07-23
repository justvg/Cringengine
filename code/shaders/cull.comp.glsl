#version 460

layout (set = 0, binding = 0) uniform CameraBuffer
{
	mat4 View;
	mat4 Proj;
	
	vec4 Frustum[6];
};

struct SMeshDraw
{
	vec3 SphereCenter;
	float SphereRadius;

	vec3 Position;
	float Scale;
	vec4 Orientation;

	uint IndexCount;
	uint FirstIndex;
	uint VertexOffset;
	uint FirstInstance;
};

struct SMeshDrawCommand
{
	uint IndexCount;
    uint InstanceCount;
    uint FirstIndex;
    uint VertexOffset;
    uint FirstInstance;
};

layout (set = 1, binding = 0) readonly buffer Draws
{
	SMeshDraw Draw[];
};

layout (set = 1, binding = 1) writeonly buffer DrawCommands
{
	SMeshDrawCommand DrawCommand[];
};

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
	uint GroupIndex = gl_WorkGroupID.x;
	uint ThreadIndex  = gl_LocalInvocationID.x;
	uint Index = GroupIndex * 32 + ThreadIndex;

	float Scale = Draw[Index].Scale ;
	vec4 Position = vec4(Draw[Index].Position + Scale * Draw[Index].SphereCenter, -1);
	float Radius = Scale * Draw[Index].SphereRadius;

	bool visible = true;
	for (uint I = 0; I < 6; I++)
		visible = visible && (dot(Position, Frustum[I]) >= -Radius);

	DrawCommand[Index].IndexCount = Draw[Index].IndexCount;
    DrawCommand[Index].InstanceCount = visible ? 1 : 0;
    DrawCommand[Index].FirstIndex = Draw[Index].FirstIndex;
    DrawCommand[Index].VertexOffset = Draw[Index].VertexOffset;
    DrawCommand[Index].FirstInstance = Draw[Index].FirstInstance;
}