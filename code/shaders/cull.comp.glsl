#version 460

layout (push_constant) uniform PushConstants
{
	uint bLodEnabled;
	uint LodsCount;

	uint bOcclusionCullingEnabled;
	uint ImageWidth;
	uint ImageHeight;
};

layout (set = 0, binding = 0) uniform CameraBuffer
{
	mat4 View;
	mat4 Proj;

	mat4 PrevView;
	mat4 PrevProj;

	vec4 CameraPosition; // w = Near
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

layout (set = 2, binding = 0) uniform sampler2D HiDepthTexture;

vec3 ProjectPoint(vec3 Point)
{
	vec4 V = PrevProj * vec4(Point, 1.0);
	return V.xyz / V.w;
}

bool ProjectSphere(vec3 Center, float Radius, float Near, out vec4 AABB)
{
	if (Center.z + Radius > Near)
		return false;

	float RadiusSq = Radius * Radius;

	vec2 CX = Center.xz;
	float TX = sqrt(dot(CX, CX) - RadiusSq);
	vec2 CosSinX = vec2(TX, Radius) / length(CX);
	vec2 MinX = mat2(CosSinX.x, CosSinX.y, -CosSinX.y, CosSinX.x) * normalize(CX) * TX;
	vec2 MaxX = mat2(CosSinX.x, -CosSinX.y, CosSinX.y, CosSinX.x) * normalize(CX) * TX;
	
	vec2 CY = Center.yz;
	float TY = sqrt(dot(CY, CY) - RadiusSq);
	vec2 CosSinY = vec2(TY, Radius) / length(CY);
	vec2 MinY = mat2(CosSinY.x, CosSinY.y, -CosSinY.y, CosSinY.x) * normalize(CY) * TY;
	vec2 MaxY = mat2(CosSinY.x, -CosSinY.y, CosSinY.y, CosSinY.x) * normalize(CY) * TY;

	vec3 MaxXCameraSpace = vec3(MaxX.x, 0.0, MaxX.y);
	vec3 MinXCameraSpace = vec3(MinX.x, 0.0, MinX.y);
	vec3 MaxYCameraSpace = vec3(0.0, MaxY.x, MaxY.y);
	vec3 MinYCameraSpace = vec3(0.0, MinY.x, MinY.y);

	float Left = ProjectPoint(MinXCameraSpace).x;
	float Right = ProjectPoint(MaxXCameraSpace).x;
	float Bot = ProjectPoint(MinYCameraSpace).y;
	float Top = ProjectPoint(MaxYCameraSpace).y;

	AABB = vec4(Left, Bot, Right, Top) * vec4(0.5, -0.5, 0.5, -0.5) + vec4(0.5);

	return true;
}

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

	if (bVisible && (bOcclusionCullingEnabled != 0))
	{
		float Near = CameraPosition.w;
		vec4 CenterCameraSpace = PrevView * vec4(Center.xyz, 1.0);

		vec4 AABB;
		if (ProjectSphere(CenterCameraSpace.xyz, Radius, Near, AABB))
		{
			float Width = (AABB.z - AABB.x) * ImageWidth;
			float Height = (AABB.w - AABB.y) * ImageHeight;

			float Level = floor(log2(max(Width, Height)));
			// This computes max depth of 2x2 texel quad
			float MaxDepth = textureLod(HiDepthTexture, 0.5*(AABB.xy + AABB.zw), Level).x;

			vec4 MinObjectCameraSpace = PrevView * vec4(Center.xy, Center.z + Radius, 1.0);
			float MinObjectDepth = ProjectPoint(MinObjectCameraSpace.xyz).z;

			// Some objects cull themselves, mb because of some precision issues, so I add this bias
			float Bias = 0.0001;
			bVisible = bVisible && (MinObjectDepth < MaxDepth + Bias);
		}
	}

	if (bVisible)
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