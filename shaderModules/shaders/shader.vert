#version 450 

layout (location = 0) out vec3 fragColor;    // Declare an output from the vertex shader 
// Every vertex produces a vec3
// That value will be handed over to the rasterizer
// Rasterizer will interpolate it
// The fragment shader will receive it

vec2 positions[3] = vec2[] ( 
  vec2(0.0, -0.5),
  vec2(0.5, 0.5),
  vec2(-0.5, 0.5)
); // location

vec3 colors[3] = vec3[] (
  vec3(1.0, 0.0, 0.0),
  vec3(0.0, 1.0, 0.0),
  vec3(0.0, 0.0, 1.0)
); // per vertex attribute data

void main() { // runs once per vertex invocation
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); 
  fragColor = colors[gl_VertexIndex];
}

// gl_VertexIndex is a built-in integer provided by the GPU
// Tells which run of the vertex shader this is
// 1st run -> 0th vertex
// 2nd run -> 1st vertex
// 3rd run -> 2nd vertex