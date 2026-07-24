#version 100
precision mediump float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec4 afterimageColor;
void main()
{
    vec4 texel = texture2D(texture0, fragTexCoord);
    if (texel.a < 0.05) discard;
    gl_FragColor = vec4(afterimageColor.rgb, texel.a * afterimageColor.a) * fragColor * colDiffuse;
}
