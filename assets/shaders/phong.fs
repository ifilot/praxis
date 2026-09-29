#version 330 core

in vec3 vertex_direction_eyespace;   // from fragment to camera
in vec3 lightdirection_eyespace;     // from fragment to light
in vec3 normal_eyespace;

uniform vec4  color;                 // base surface color
uniform vec3  light_color;           // light intensity/color
uniform float diffuse_strength;
uniform float ambient_strength;
uniform float specular_strength;
uniform float shininess;             // specular exponent (e.g. 16–128)
uniform float edge_strength;
uniform float edge_power;
uniform int camera_mode;

out vec4 fragColor;

void main()
{
    // --- Normalize inputs (important after interpolation) ---
    vec3 N = normalize(normal_eyespace);
    vec3 L = normalize(lightdirection_eyespace);
    vec3 V = normalize(vertex_direction_eyespace);
    if (camera_mode == 1) {
        V = vec3(0.0, 0.0, 1.0);
    }

    // --- Ambient ---
    vec3 ambient = ambient_strength * light_color;

    // --- Diffuse (Lambert) ---
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = diffuse_strength * NdotL * light_color;

    // --- Blinn-Phong specular (better than reflect()) ---
    vec3 H = normalize(L + V);   // half-vector
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, shininess);

    // Energy-aware specular reduction
    vec3 specular = specular_strength * spec * light_color * (1.0 - color.rgb);

    // Combine lighting
    vec3 result = (ambient + diffuse) * color.rgb + specular;

    // ================= EDGE DARKENING =================
    float rim = 1.0 - max(dot(N, V), 0.0);
    rim = pow(rim, edge_power);

    result *= (1.0 - rim * edge_strength);
    // ==================================================

    // Gamma correction (important!)
    result = pow(result, vec3(1.0/2.2));

    fragColor = vec4(result, color.a);
}
