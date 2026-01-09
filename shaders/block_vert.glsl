#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;    // Kept if you use vertex colors, otherwise optional
layout (location = 3) in vec2 aTexCoord; // Standard UV
layout (location = 4) in float aTexIndex;// <--- NEW: The Texture ID (0, 1, 2...)

out vec3 FragPos;
out vec3 Normal;
out vec3 TexCoord; // <--- CHANGED: Now a vec3 to hold the index

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Normal matrix calculation (expensive, but fine for now)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal = normalMatrix * aNormal;

    gl_Position = projection * view * vec4(FragPos, 1.0);
    
    // Combine the 2D UV and the Index into one 3D coordinate
    TexCoord = vec3(aTexCoord, aTexIndex); 
}
