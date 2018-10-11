#version 410 core

layout (quads, equal_spacing, ccw) in;
out vec3 Position;
out vec2 TexCoord;
out vec3 Normal;
out vec3 ViewDir;

uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;

void
main()
{
	float u = gl_TessCoord.x;
	float v = gl_TessCoord.y;
	vec4 u4 = vec4(u);
	vec4 uComp4 = vec4(1 - u);
	vec4 v4 = vec4(v);
	vec4 vComp4 = vec4(1 - v);

	vec4 c4[16];
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			c4[i * 4 + j] = gl_in[i * 4 + j].gl_Position;

	vec4 c3[9];
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			c3[i * 3 + j] = vComp4 * (uComp4 * c4[i * 4 + j] + u4 * c4[i * 4 + j + 1]) +
					v4 * (uComp4 * c4[(i + 1) * 4 + j] + u4 * c4[(i + 1) * 4 + j + 1]);

	vec4 c2[4];
	for (int i = 0; i < 2; ++i)
		for (int j = 0; j < 2; ++j)
			c2[i * 2 + j] = vComp4 * (uComp4 * c3[i * 3 + j] + u4 * c3[i * 3 + j + 1]) +
					v4 * (uComp4 * c3[(i + 1) * 3 + j] + u4 * c3[(i + 1) * 3 + j + 1]);

	vec4 uc[2];
	vec4 vc[2];
	for (int i = 0; i < 2; ++i)
	{
		uc[i] = (1 - u) * c2[i * 2] + u * c2[i * 2 + 1];
		vc[i] = (1 - v) * c2[i] + v * c2[2 + i];
	}

	vec4 localPos = (1 - v) * uc[0] + v * uc[1];
	vec4 localNormal = vec4(normalize(cross(uc[1].xyz - uc[0].xyz, vc[1].xyz - vc[0].xyz)), 0);

	gl_Position = ProjectionMatrix * ModelViewMatrix * localPos;
	Position = gl_Position.xyz / gl_Position.w;
	TexCoord = gl_TessCoord.xy;
	Normal = (ModelViewMatrix * localNormal).xyz;
	ViewDir = normalize(-Position);
}
