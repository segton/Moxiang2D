#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float flashAmount;

void main()
{
    vec4 textureColor =
        texture(
            texture0,
            fragTexCoord
        );

    vec4 color =
        textureColor *
        fragColor *
        colDiffuse;

    if (color.a < 0.05)
    {
        discard;
    }

    float flash =
        clamp(
            flashAmount,
            0.0,
            1.0
        );

    color.rgb =
        mix(
            color.rgb,
            vec3(1.0),
            flash
        );

    finalColor =
        color;
}