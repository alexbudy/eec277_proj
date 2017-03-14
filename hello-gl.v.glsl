#version 110

attribute vec2 position;

varying vec2 texcoord;

// vertex shader slows pipeline
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    texcoord = position * vec2(0.5) + vec2(0.5);

    int x = 0;  
    while ( x < 10000) {
        //float temp = atan(x);    
        x++;
    }
}
