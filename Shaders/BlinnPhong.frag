#version 410 core
layout (location = 0) out vec4 FragColor;

in vec2 TexCoord;
in vec3 WorldPosition;
in vec3 Normal;

uniform float AmbientIntensity;
uniform sampler2D Texture;
uniform int TextureRepeat;
uniform vec3 LightPosition;
uniform float LightIntensity;
uniform float Shininess;

void
main()
{
	vec3 normal = (gl_FrontFacing) ? normalize(Normal) : normalize(-Normal);
	vec3 lightDir = normalize(LightPosition - WorldPosition);
	vec3 viewDir = normalize(-WorldPosition);

	float diffuse = max(dot(lightDir, normal), 0);
	vec3 diffuseColor = texture(Texture, TexCoord * TextureRepeat).rgb;

	vec3 halfDir = normalize(lightDir + viewDir);
	float specAngle = max(dot(halfDir, normal), 0);
	specAngle = (diffuse != 0) ? specAngle : 0;
	float specular = pow(specAngle, Shininess);

	vec3 color = vec3(0);
	color+= diffuseColor * (AmbientIntensity + diffuse * LightIntensity);
	color+= specular * LightIntensity;

	FragColor = vec4(color, 1);
}
