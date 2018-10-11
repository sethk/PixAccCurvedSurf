#version 410 core
layout (location = 0) out vec4 FragColor;

varying vec3 Position;
varying vec3 ViewDir;
varying vec3 Normal;

const vec3 LightPos = vec3(1);
const vec3 LightColor = vec3(1);
const float LightPower = 40.0;
const vec3 AmbientColor = vec3(0.1, 0.0, 0.0);
const vec3 DiffuseColor = vec3(0.5, 0.0, 0.0);
const vec3 SpecularColor = vec3(1);
const float Shininess = 16.0;
const float ScreenGamma = 2.2;

void
main()
{
	vec3 normal = normalize(Normal);
	vec3 lightDir = LightPos - Position;
	float distance = length(lightDir);
	float distanceSq = distance * distance;
	lightDir = normalize(lightDir);

	float lambertian = max(dot(lightDir, Normal), 0);
	float specular = 0;

	if (lambertian > 0)
	{
		vec3 viewDir = normalize(ViewDir);

		// (lightDir + viewDir) / 2
		vec3 halfDir = normalize(lightDir + viewDir);
		float specAngle = max(dot(halfDir, normal), 0);
		specular = pow(specAngle, Shininess);
	}

	vec3 lightScale = vec3(LightPower / distanceSq);
	vec3 linearColor = AmbientColor +
			DiffuseColor * lambertian * LightColor * lightScale +
			SpecularColor * specular * LightColor * lightScale;
	vec3 sRGBColor = pow(linearColor, vec3(1.0 / ScreenGamma));

	FragColor = vec4(sRGBColor, 1);
}
