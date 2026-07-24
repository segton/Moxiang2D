#version 100

precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

void main()
{
    vec4 textureColor = texture2D(
        texture0,
        fragTexCoord
    );

    gl_FragColor =
        textureColor *
        colDiffuse *
        fragColor;
}