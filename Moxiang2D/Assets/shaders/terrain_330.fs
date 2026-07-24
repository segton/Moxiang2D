#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(vec3(-0.45, 0.85, -0.30));
    float diffuse = 0.78 + max(dot(normal, lightDirection), 0.0) * 0.22;

    finalColor = vec4(
        texel.rgb * colDiffuse.rgb * fragColor.rgb * diffuse,
        texel.a * colDiffuse.a * fragColor.a
    );
}
