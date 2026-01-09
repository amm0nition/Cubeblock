#version 460 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in float LayerIndex;
uniform sampler2DArray textureArray;

struct Light {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
};
uniform Light light;

// Fog Uniforms
uniform vec3 viewPos;
uniform vec3 fogColor;
uniform float fogDensity;

void main()
{
    // Texture
    vec4 texColor = texture(textureArray, vec3(TexCoord, LayerIndex));
    
    // Lighting
    vec3 ambient = light.ambient * texColor.rgb;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texColor.rgb;
    vec3 result = ambient + diffuse;

    // --- EXPONENTIAL SQUARED FOG ---
    float distance = length(viewPos - FragPos);
    
    // The "Squared" part (dist * dist) makes it much smoother
    float fogFactor = 1.0 - exp(-(distance * fogDensity) * (distance * fogDensity));
    
    // Clamp to 0-1
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // Mix
    vec3 finalColor = mix(result, fogColor, fogFactor);

    FragColor = vec4(finalColor, 1.0);
}