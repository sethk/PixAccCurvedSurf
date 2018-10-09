#version 410 core

layout (vertices = 16) out;

void
main()
{
    if (gl_InvocationID == 0)
    {
	gl_TessLevelInner[0] = 9;
	gl_TessLevelInner[1] = 9;
	gl_TessLevelOuter[0] = 9;
	gl_TessLevelOuter[1] = 9;
	gl_TessLevelOuter[2] = 9;
	gl_TessLevelOuter[3] = 9;
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
