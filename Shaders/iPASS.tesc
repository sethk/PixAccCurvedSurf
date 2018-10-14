#version 410 core
layout (vertices = 16) out;
patch out mat3 C1;
in float PatchTessLevels[];

void
main()
{
	if (gl_InvocationID == 0)
	{
		gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = gl_TessLevelOuter[3] = PatchTessLevels[5];
		gl_TessLevelOuter[0] = max(PatchTessLevels[8], PatchTessLevels[4]);
		gl_TessLevelOuter[1] = max(PatchTessLevels[1], PatchTessLevels[2]);
		gl_TessLevelOuter[2] = max(PatchTessLevels[7], PatchTessLevels[11]);
		gl_TessLevelOuter[3] = max(PatchTessLevels[14], PatchTessLevels[13]);
		gl_TessLevelInner[0] = gl_TessLevelInner[1] = PatchTessLevels[5];

		C1 = mat3(0.5, 0, 0, 0, 0.5, 0, 0, 0, 0.5);
	}

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
