#version 410 core

layout (location = 0) out vec4 FragColor;

void
main()
{
    int index = gl_PrimitiveID % 16;
    bool red = (index < 4 || index >= 12 || index == 4 || index == 7 || index == 8 || index == 11);

    FragColor = vec4((red) ? 1 : 0, 0, 0, 1);
}
