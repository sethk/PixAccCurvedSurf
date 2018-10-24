layout (quads, equal_spacing, ccw) in;
patch in BicubicPatch Patch;
out vec2 TexCoord;
out vec3 WorldPosition;
out vec3 Normal;

uniform mat4 ProjectionMatrix;

void
main()
{
	TexCoord = gl_TessCoord.xy;
	WorldPosition = BicubicBezier(gl_TessCoord.xy, Normal, Patch);
	gl_Position = ProjectionMatrix * vec4(WorldPosition, 1);
}
