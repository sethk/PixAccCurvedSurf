uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;

in vec3 Position;

out gl_PerVertex
{
    vec4 gl_Position;
};

void
main(void)
{
    gl_Position = ProjectionMatrix * ModelViewMatrix * vec4(Position, 1);
}
