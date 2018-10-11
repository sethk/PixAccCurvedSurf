#version 410 core

varying vec3 Position;
varying vec3 Normal;

void main()
{
	gl_FragColor = vec4((vec3(1) + Normal) * vec3(0.5), 1.0);
}
