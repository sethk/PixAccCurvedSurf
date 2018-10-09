#version 410 core

layout (quads, equal_spacing, ccw) in;
out vec4 Color;

uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;

void
main()
{
    vec4 c01 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
    vec4 c23 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, gl_TessCoord.x);
    vec4 c0123 = mix(c01, c23, gl_TessCoord.x);

    vec4 c45 = mix(gl_in[4].gl_Position, gl_in[5].gl_Position, gl_TessCoord.x);
    vec4 c67 = mix(gl_in[6].gl_Position, gl_in[7].gl_Position, gl_TessCoord.x);
    vec4 c4567 = mix(c45, c67, gl_TessCoord.x);

    vec4 c89 = mix(gl_in[8].gl_Position, gl_in[9].gl_Position, gl_TessCoord.x);
    vec4 c10_11 = mix(gl_in[10].gl_Position, gl_in[11].gl_Position, gl_TessCoord.x);
    vec4 c8_9_10_11 = mix(c89, c10_11, gl_TessCoord.x);

    vec4 c12_13 = mix(gl_in[12].gl_Position, gl_in[13].gl_Position, gl_TessCoord.x);
    vec4 c14_15 = mix(gl_in[14].gl_Position, gl_in[15].gl_Position, gl_TessCoord.x);
    vec4 c12_13_14_15 = mix(c12_13, c14_15, gl_TessCoord.x);

    vec4 c0123_4567 = mix(c0123, c4567, gl_TessCoord.y);
    vec4 c8_9_10_11_12_13_14_15 = mix(c8_9_10_11, c12_13_14_15, gl_TessCoord.y);

    vec4 pos = mix(c0123_4567, c8_9_10_11_12_13_14_15, gl_TessCoord.y);
    Color = vec4(pos.x, pos.y, pos.z, 1);

    gl_Position = ProjectionMatrix * ModelViewMatrix * pos;
}
