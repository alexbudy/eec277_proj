#version 110

uniform float fade_factor;
uniform sampler2D textures[2];

varying vec2 texcoord;

// frag shader does not slow pipeline
void main()
{
    int x = 0;

    while ( x < 10) {
        float temp = atan(x);
        x++;
    }    

    gl_FragColor = vec4(texcoord[0], .0, 0, 0.5);
    //gl_FragColor = mix(
    //    texture2D(textures[0], texcoord),
     //   texture2D(textures[1], texcoord),
      //  fade_factor
   // );
}
