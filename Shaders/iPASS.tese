#version 410 core
#define METHOD 1

patch in mat3 C1;
layout (quads, equal_spacing, ccw) in;
out vec2 TexCoord;
out vec3 WorldPosition;
out vec3 Normal;

uniform mat4 ProjectionMatrix;

#if METHOD == 2
float
B( int i, float u )
{
    const vec4 bc = vec4( 1, 3, 3, 1 );
    
    return bc[i] * pow( u, i ) * pow( 1.0 - u, 3 - i ); 
}
#endif // METHOD == 2

vec3 BicubicBezier(in vec2 uv, out vec3 normal)
{
	float u = uv.x;
	float v = uv.y;

#if METHOD == 1
	vec3 u3 = vec3(u);
	vec3 uComp3 = vec3(1 - u);
	vec3 v3 = vec3(v);
	vec3 vComp3 = vec3(1 - v);

	vec3 c4[16];
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			c4[i * 4 + j] = gl_in[i * 4 + j].gl_Position.xyz;

	vec3 c3[9];
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			c3[i * 3 + j] = vComp3 * (uComp3 * c4[i * 4 + j] + u3 * c4[i * 4 + j + 1]) +
					v3 * (uComp3 * c4[(i + 1) * 4 + j] + u3 * c4[(i + 1) * 4 + j + 1]);

	vec3 c2[4];
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < 2; ++j)
			c2[i * 2 + j] = vComp3 * (uComp3 * c3[i * 3 + j] + u3 * c3[i * 3 + j + 1]) +
					v3 * (uComp3 * c3[(i + 1) * 3 + j] + u3 * c3[(i + 1) * 3 + j + 1]);

	vec3 uc[2];
	vec3 vc[2];
	for (int i = 0; i < 2; ++i)
	{
		uc[i] = (1 - u) * c2[i * 2] + u * c2[i * 2 + 1];
		vc[i] = (1 - v) * c2[i] + v * c2[2 + i];
	}

	normal = cross(vc[1] - vc[0], uc[1] - uc[0]);
	//mat3 m1 = mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);
	return C1 * (1 - v) * uc[0] + v * uc[1];
#endif // METHOD == 1
#if METHOD == 2
    vec3  pos = vec3( 0.0 );

    for ( int j = 0; j < 4; ++j ) {
        for ( int i = 0; i < 4; ++i ) {
            pos += B( i, u ) * B( j, v ) * gl_in[4*j+i].gl_Position.xyz;
        }
    }

	return pos;
#endif
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

		mat4 Cx = B * Px * B;
		mat4 Cy = B * Py * B;
		mat4 Cz = B * Pz * B;

	vec4 up = vec4(u*u*u, u*u, u, 1);
	vec4 vp = vec4(v*v*v, v*v, v, 1);

	return vec3(dot(Cx * vp, up), dot(Cy * vp, up), dot(Cz * vp, up));
#endif // METHOD == 3
}

void
main()
{
	TexCoord = gl_TessCoord.xy;
	WorldPosition = BicubicBezier(gl_TessCoord.xy, Normal);

	gl_Position = ProjectionMatrix * vec4(WorldPosition, 1);
}
