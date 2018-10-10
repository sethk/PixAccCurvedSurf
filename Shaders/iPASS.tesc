#version 410 core
layout (vertices = 16) out;

in float PatchTessLevels[];

void
main()
{
    if (gl_InvocationID == 0)
    {
	gl_TessLevelInner[0] = PatchTessLevels[0];
	gl_TessLevelInner[1] = PatchTessLevels[1];
	gl_TessLevelOuter[0] = PatchTessLevels[2];
	gl_TessLevelOuter[1] = PatchTessLevels[3];
	gl_TessLevelOuter[2] = PatchTessLevels[4];
	gl_TessLevelOuter[3] = PatchTessLevels[5];
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
