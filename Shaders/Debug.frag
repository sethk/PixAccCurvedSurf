layout (location = 0) out vec4 FragColor;

#if SHOW_ERROR
	in vec2 TexCoord;
	in vec3 WorldPosition;
	in BicubicPatch DebugPatch;

	uniform ivec2 ViewportSize;
	uniform mat4 ProjectionMatrix;
#endif // SHOW_ERROR
in vec3 Normal;

void
main()
{
	#if SHOW_ERROR
		vec3 normal;
		vec3 limitPos = BicubicBezier(TexCoord, normal, DebugPatch);
		vec4 limitScreenPos = ProjectionMatrix * vec4(limitPos, 1);
		limitPos = limitScreenPos.xyz / limitScreenPos.w;

		vec4 approxScreenPos = ProjectionMatrix * vec4(WorldPosition, 1);
		vec3 approxPos = approxScreenPos.xyz / approxScreenPos.w;

		vec2 halfViewport = ViewportSize * 0.5;

		float error = distance(limitPos.xy * halfViewport, approxPos.xy * halfViewport);
		if (error < 0.1)
			FragColor = vec4(0.4, 0.4, 0.4, 1);
		else if (error < 0.5)
			FragColor = vec4(0, 0, 1, 1);
		else if (error < 1.0)
			FragColor = vec4(0, 1, 0, 1);
		else
			FragColor = vec4(1, 0, 0, 1);
	#endif // SHOW_ERROR

	#if SHOW_NORMAL
		FragColor = vec4((vec3(1) + normalize(Normal)) * vec3(0.5), 1.0);
	#endif // SHOW_NORMAL
}
