#version 100

precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D lightMap;

void main()
{
    vec4 sceneColor =
        texture2D(
            texture0,
            fragTexCoord
        ) * fragColor;

    vec3 lightColor =
        texture2D(
            lightMap,
            fragTexCoord
        ).rgb;

    vec3 color =
        sceneColor.rgb *
        lightColor;

    color *= 1.06;

    color =
        (color - 0.5) *
        1.10 +
        0.5;

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

    gl_FragColor = vec4(
        clamp(
            color,
            0.96,
            1.0
        ),
        sceneColor.a
    );
}