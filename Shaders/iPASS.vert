#version 410 core

in vec4 Position;
in float TessLevel;
out float PatchTessLevels;

void
main()
{
    gl_Position = Position;
    PatchTessLevels = TessLevel;
}
