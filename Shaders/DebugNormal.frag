#version 410 core
layout (location = 0) out vec4 FragColor;

in vec3 Normal;

void main()
{
	FragColor = vec4((vec3(1) + normalize(Normal)) * vec3(0.5), 1.0);
}
