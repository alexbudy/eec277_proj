#version 110

uniform float fade_factor;
uniform sampler2D textures[2];

varying vec2 texcoord;
//layout (binding = 1, offset = 0) uniform atomic_uint atRed;


void main()
{
    float x = texcoord.x;
    float y = texcoord.y;
    //int m = int(mod(x*1000.0, 2.0));

     int i = 0;
     while (i < 1000) {
        float tmp = atan(i);

        i++;
    }  
    //atomicCounterIncrement(atRed);
    gl_FragColor = vec4(0, 1.0, 0, 0.5);

    //if (m==1) 
    //    gl_FragColor = vec4(.0, 1.0, 0, 0.5);               //green
    //else
    //    gl_FragColor = vec4(0, 0, 1.0, 0.5);                //blue
}

int mod(float a, float b) {
    float m=a-floor((a+0.5)/b)*b;
    return int(floor(m+0.5));
}
