#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 afterimageColor;
out vec4 finalColor;
void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < 0.05) discard;
    finalColor = vec4(afterimageColor.rgb, texel.a * afterimageColor.a) * fragColor * colDiffuse;
}
