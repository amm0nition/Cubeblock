#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor; 
layout (location = 3) in vec3 aTexCoord; // Contains: U, V, Layer

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out float LayerIndex; // <--- Pass this to Fragment Shader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    
    TexCoord = aTexCoord.xy;
    LayerIndex = aTexCoord.z; // Extract the layer ID
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
