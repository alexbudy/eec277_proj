#version 110

uniform float fade_factor;
uniform sampler2D textures[2];

varying vec2 texcoord;


void main()
{
    float x = texcoord.x;
    float y = texcoord.y;


    float xx = gl_FragCoord.x;
    int m = int(mod(xx, 2.0));

    if (m == 0) {
        gl_FragColor = vec4(0, 1.0, 0.0, 0.5);
    } else {
        gl_FragColor = vec4(1.0, .0, 0, 0.5);
        int i = 0;
        while (i<1) {
            
            i++;
        }
    }
}

int mod(float a, float b) {
    float m=a-floor((a+0.5)/b)*b;
    return int(floor(m+0.5));
}
