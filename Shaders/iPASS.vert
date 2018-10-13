#version 410 core

in vec4 Position;
in float TessLevel;
out float PatchTessLevels;

uniform mat4 ModelViewMatrix;

void
main()
{
    gl_Position = ModelViewMatrix * Position;
    PatchTessLevels = TessLevel;
}
