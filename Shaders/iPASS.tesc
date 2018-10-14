#version 410 core
#define METHOD 3
layout (vertices = 16) out;
in float PatchTessLevels[];

#if METHOD == 3
patch out mat4 Cx, Cy, Cz;
#endif // METHOD == 3

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

		#if METHOD == 3
			const mat4 B = mat4(
					-1, 3,-3, 1,
					3,-6, 3, 0,
					-3, 3, 0, 0,
					1, 0, 0, 0
					);

			mat4 Px, Py, Pz;

			for(int j=0; j!=4; ++j)
				for(int i=0; i!=4; ++i)
				{
					int k = j*4+i;
					Px[j][i] = gl_in[k].gl_Position.x;
					Py[j][i] = gl_in[k].gl_Position.y;
					Pz[j][i] = gl_in[k].gl_Position.z;
				}

			Cx = B * Px * B;
			Cy = B * Py * B;
			Cz = B * Pz * B;
		#endif // METHOD == 3
	}

	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
