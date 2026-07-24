#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D lightMap;

out vec4 finalColor;

void main()
{
    vec4 sceneColor =
        texture(
            texture0,
            fragTexCoord
        ) * fragColor;

    vec3 lightColor =
        texture(
            lightMap,
            fragTexCoord
        ).rgb;

    // Apply the light map.
    vec3 color =
        sceneColor.rgb *
        lightColor;

    // Slightly increase overall brightness.
    color *= 1.06;

    // Increase contrast.
    color =
        (color - 0.5) *
        1.10 +
        0.5;

    // Reduce the strong neon saturation slightly.
    float luminance =
        dot(
            color,
            vec3(
                0.2126,
                0.7152,
                0.0722
            )
        );

    color =
        mix(
            vec3(luminance),
            color,
            0.88
        );

    // Slightly warm brighter areas.
    float highlightAmount =
        smoothstep(
            0.45,
            1.0,
            luminance
        );

    color *= mix(
        vec3(1.0),
        vec3(
            1.05,
            1.01,
            0.94
        ),
        highlightAmount * 0.35
    );

    // Slightly cool darker areas.
    float shadowAmount =
        1.0 -
        smoothstep(
            0.10,
            0.55,
            luminance
        );

    color *= mix(
        vec3(1.0),
        vec3(
            0.92,
            0.96,
            1.04
        ),
        shadowAmount * 0.30
    );

    // Darken the outer edges of the screen.
    vec2 centeredUv =
        fragTexCoord -
        vec2(0.5);

    float distanceFromCenter =
        length(
            centeredUv *
            vec2(
                1.0,
                0.82
            )
        );

    float vignette =
        1.0 -
        smoothstep(
            0.30,
            0.72,
            distanceFromCenter
        ) * 0.40;

    color *= vignette;

    finalColor = vec4(
        clamp(
            color,
            0.0,
            1.0
        ),
        sceneColor.a
    );
}