#version 460

layout (push_constant) uniform PushConstants
{
	uint bLodEnabled;
	uint LodsCount;
};

layout (set = 0, binding = 0) uniform CameraBuffer
{
	mat4 View;
	mat4 Proj;

	vec4 CameraPosition;	
	vec4 Frustum[6];
};

struct SMeshDraw
{
	vec3 SphereCenter;
	float SphereRadius;

	vec3 Position;
	float Scale;
	vec4 Orientation;

	uint IndexCount[7];
	uint FirstIndex[7];
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

layout (set = 1, binding = 2) buffer DrawCounter
{
	uint DrawCount;
};

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
	uint Index = gl_GlobalInvocationID.x;

	float Scale = Draw[Index].Scale ;
	vec4 Center = vec4(Draw[Index].Position + Scale * Draw[Index].SphereCenter, -1);
	float Radius = Scale * Draw[Index].SphereRadius;

	bool bVisible = true;
	for (uint I = 0; I < 6; I++)
		bVisible = bVisible && (dot(Center, Frustum[I]) >= -Radius);

	if(bVisible)
	{
		uint CommandIndex = atomicAdd(DrawCount, 1);

		float Distance = length(Center.xyz - CameraPosition.xyz) - Radius;
		float LodDistance = log2(max(Distance, 1.0));
		int LodIndex = bLodEnabled > 0 ? clamp(int(LodDistance) - 1, 0, int(LodsCount) - 1) : 0;

		DrawCommand[CommandIndex].IndexCount = Draw[Index].IndexCount[LodIndex];
		DrawCommand[CommandIndex].InstanceCount = 1;
		DrawCommand[CommandIndex].FirstIndex = Draw[Index].FirstIndex[LodIndex];
		DrawCommand[CommandIndex].VertexOffset = Draw[Index].VertexOffset;
		DrawCommand[CommandIndex].FirstInstance = Draw[Index].FirstInstance;
	}
}