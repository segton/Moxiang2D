#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;
uniform float isoVerticalScale;

out vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
    vec2 worldPosition = vertexPosition.xy;

    vec2 isoPosition;

    isoPosition.x =
        (worldPosition.x - worldPosition.y) *
        0.70710678118;

    isoPosition.y =
        (worldPosition.x + worldPosition.y) *
        0.70710678118 *
        isoVerticalScale;

    gl_Position = mvp * vec4(
        isoPosition.x,
        isoPosition.y,
        vertexPosition.z,
        1.0
    );

    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
}