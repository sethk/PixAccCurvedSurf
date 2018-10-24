struct BicubicPatch
{
	#if METHOD == 2
		mat4 Cx, Cy, Cz;
	#else
		vec3 c[16];
	#endif // METHOD == 2
};

vec3
BicubicBezier(in vec2 uv, out vec3 normal, const in BicubicPatch p)
{
	#if METHOD == 1
		vec3 u3 = vec3(uv.x);
		vec3 uComp3 = vec3(1 - uv.x);
		vec3 v3 = vec3(uv.y);
		vec3 vComp3 = vec3(1 - uv.y);

		vec3 c3[9];
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				c3[i * 3 + j] = vComp3 * (uComp3 * p.c[i * 4 + j] + u3 * p.c[i * 4 + j + 1]) +
						v3 * (uComp3 * p.c[(i + 1) * 4 + j] + u3 * p.c[(i + 1) * 4 + j + 1]);

		vec3 c2[4];
		for (int i = 0; i < 2; ++i)
			for (int j = 0; j < 2; ++j)
				c2[i * 2 + j] = vComp3 * (uComp3 * c3[i * 3 + j] + u3 * c3[i * 3 + j + 1]) +
						v3 * (uComp3 * c3[(i + 1) * 3 + j] + u3 * c3[(i + 1) * 3 + j + 1]);

		vec3 uc[2];
		vec3 vc[2];
		for (int i = 0; i < 2; ++i)
		{
			uc[i] = (1 - uv.x) * c2[i * 2] + uv.x * c2[i * 2 + 1];
			vc[i] = (1 - uv.y) * c2[i] + uv.y * c2[2 + i];
		}

		normal = cross(vc[1] - vc[0], uc[1] - uc[0]);
		return (1 - uv.y) * uc[0] + uv.y * uc[1];
	#endif // METHOD == 1

	#if METHOD == 2
		vec4 up = vec4(uv.x*uv.x*uv.x, uv.x*uv.x, uv.x, 1);
		vec4 vp = vec4(uv.y*uv.y*uv.y, uv.y*uv.y, uv.y, 1);

		return vec3(dot(p.Cx * vp, up), dot(p.Cy * vp, up), dot(p.Cz * vp, up));
	#endif // METHOD == 2

	#if METHOD == 3
		vec2 N0 = -3 * (1 - uv) * (1 - uv);
		vec2 N1 =  3 * (1 - uv) * (1 - uv) - 6 * uv * (1 - uv);
		vec2 N2 =  6 *      uv  * (1 - uv) - 3 * uv * uv;
		vec2 N3 =  3 *      uv  * uv;

		vec2 P0 =      (1 - uv) * (1 - uv) * (1 - uv);
		vec2 P1 =  3 * (1 - uv) * (1 - uv) *      uv;
		vec2 P2 =  3 *      uv  *      uv  * (1 - uv);
		vec2 P3 =           uv  *      uv  *      uv;

		vec3 tan1 = (N0.x * p.c[ 0] + N1.x * p.c[ 1] + N2.x * p.c[ 2] + N3.x * p.c[ 3]) * P0.y +
					(N0.x * p.c[ 4] + N1.x * p.c[ 5] + N2.x * p.c[ 6] + N3.x * p.c[ 7]) * P1.y +
					(N0.x * p.c[ 8] + N1.x * p.c[ 9] + N2.x * p.c[10] + N3.x * p.c[11]) * P2.y +
					(N0.x * p.c[12] + N1.x * p.c[13] + N2.x * p.c[14] + N3.x * p.c[15]) * P3.y;
		vec3 tan2 = (P0.x * p.c[ 0] + P1.x * p.c[ 1] + P2.x * p.c[ 2] + P3.x * p.c[ 3]) * N0.y +
					(P0.x * p.c[ 4] + P1.x * p.c[ 5] + P2.x * p.c[ 6] + P3.x * p.c[ 7]) * N1.y +
					(P0.x * p.c[ 8] + P1.x * p.c[ 9] + P2.x * p.c[10] + P3.x * p.c[11]) * N2.y +
					(P0.x * p.c[12] + P1.x * p.c[13] + P2.x * p.c[14] + P3.x * p.c[15]) * N3.y;
		normal = cross(tan1, tan2);

		return  (P0.x * p.c[ 0] + P1.x * p.c[ 1] + P2.x * p.c[ 2] + P3.x * p.c[ 3]) * P0.y +
				(P0.x * p.c[ 4] + P1.x * p.c[ 5] + P2.x * p.c[ 6] + P3.x * p.c[ 7]) * P1.y +
				(P0.x * p.c[ 8] + P1.x * p.c[ 9] + P2.x * p.c[10] + P3.x * p.c[11]) * P2.y +
				(P0.x * p.c[12] + P1.x * p.c[13] + P2.x * p.c[14] + P3.x * p.c[15]) * P3.y;
	#endif // METHOD == 3
}
