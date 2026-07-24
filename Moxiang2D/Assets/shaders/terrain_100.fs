#version 100

precision mediump float;

varying vec2 fragTexCoord;
varying vec3 fragNormal;
varying vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

void main()
{
    vec4 texel = texture2D(texture0, fragTexCoord);
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(-0.45, 0.85, -0.30));
    float diffuse = 0.78 + max(dot(normal, lightDirection), 0.0) * 0.22;

    gl_FragColor = vec4(
        texel.rgb * colDiffuse.rgb * fragColor.rgb * diffuse,
        texel.a * colDiffuse.a * fragColor.a
    );
}
