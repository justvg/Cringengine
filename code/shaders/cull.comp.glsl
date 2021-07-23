#version 460

struct SMeshDraw
{
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

layout (set = 0, binding = 0) readonly buffer Draws
{
	SMeshDraw Draw[];
};

layout (set = 0, binding = 1) writeonly buffer DrawCommands
{
	SMeshDrawCommand DrawCommand[];
};

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
void main()
{
	uint GroupIndex = gl_WorkGroupID.x;
	uint ThreadIndex  = gl_LocalInvocationID.x;
	uint Index = GroupIndex * 32 + ThreadIndex;

	DrawCommand[Index].IndexCount = Draw[Index].IndexCount;
    DrawCommand[Index].InstanceCount = 1;
    DrawCommand[Index].FirstIndex = Draw[Index].FirstIndex;
    DrawCommand[Index].VertexOffset = Draw[Index].VertexOffset;
    DrawCommand[Index].FirstInstance = Draw[Index].FirstInstance;
}