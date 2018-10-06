#version 410 core
uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;

in vec3 Position;
in vec4 Color;

out gl_PerVertex
{
    vec4 gl_Position;
};

void
main(void)
{
    gl_Position = ProjectionMatrix * ModelViewMatrix * vec4(Position, 1);
}
