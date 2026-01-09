#version 460 core

out vec4 FragColor;

struct Light
{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
};

// We don't really need a struct for Material anymore since 
// the texture is an array, but we can keep the concept simple:
uniform sampler2DArray blockTextureArray; // <--- CHANGED: The array of all block images

in vec3 TexCoord; // (u, v, layer_index)
in vec3 Normal;  
in vec3 FragPos;  

uniform Light light;

void main()
{
    // 1. Sample the color from the specific layer in the array
    vec4 texColor = texture(blockTextureArray, TexCoord);

    // Ambient
    vec3 ambient = light.ambient * vec3(texColor);
  
    // Diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texColor);  
    
    vec3 result = ambient + diffuse;
    FragColor = vec4(result, 1.0);
}
    