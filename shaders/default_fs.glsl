#version 450 core
#extension GL_ARB_bindless_texture : require

#define PI 3.141592

out vec4 FragColor;

in mat3 tbn;
in vec4 color;
in vec4 weights;
in vec3 tangent;
in vec3 frag_pos;
in vec3 view_vec;
in vec3 normal;
in vec2 tex_coord;
in vec2 extra_tex_coord;
in float handedness;

flat in uint v_DrawID;

uniform vec3 light_dir;
uniform mat4 light_view;
uniform mat4 light_ortho;

layout(bindless_sampler) uniform samplerCube irradiance_map;
layout(bindless_sampler) uniform samplerCube prefilter_map;
layout(bindless_sampler) uniform sampler2D brdf_lut;

#define alpha_mode_opaque(material) ((material.flags & 0xfffffff3u) == 0)
#define alpha_mode_mask(material)   ((material.flags & 0x00000001u) == 1)
#define alpha_mode_blend(material)  ((material.flags & 0x00000002u) == 2)
#define texture_exists(texture_index) ((texture_index) != 0)

uniform sampler2D dir_shadowmap;

struct Material {
    vec4 base_color;
    vec4 base_color_factor;
    vec3 emissive_factor;
    float metalic_factor;
    float alpha_cutoff;
    float roughness_factor;
    int base_color_texture_index;
    int metalic_texture_index;
    int normal_texture_index;
    int occlusion_texture_index;
    int emissive_texture_index;
    uint flags;
};

struct PointLight {
    vec4 color_intensity;
    vec4 pos; 
    vec4 attenuation_factors; // w components not used, just for padding
};

layout(std430, binding = 0) readonly restrict buffer texture_handle_buffer {
    sampler2D texture_handles[];
};

layout(std430, binding = 1) readonly restrict buffer material_buffer {
    Material materials[];
};

layout(std430, binding = 2) readonly restrict buffer command_material_buffer {
    uint command_material[];
};

layout(std140,binding = 3) readonly restrict uniform point_light_buffer {
    uint num_lights;
    PointLight lights[];
};

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

float compute_shadows(vec4 frag_light_space,vec3 normal) {
    vec3 proj_coords = frag_light_space.xyz / frag_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;
    if(proj_coords.z > 1.0)
        return 0.0;

    float bias = max(0.05 * (1.0 - dot(normal, light_dir)), 0.005);  
    float closest_depth = texture(dir_shadowmap, proj_coords.xy).r; 
    float current_depth = proj_coords.z;
    float shadow = current_depth > closest_depth + bias ? 1.0 : 0.0;

    return shadow;
} 

uniform int color_state = 0;

void main() {
    uint current_material_index = command_material[v_DrawID];
    Material current_material = materials[current_material_index];
    vec4 albedo_texture_sample = texture(texture_handles[current_material.base_color_texture_index],tex_coord); 
    float transparency = albedo_texture_sample.a * current_material.base_color_factor.a;
    vec3 albedo_color = albedo_texture_sample.rgb * current_material.base_color_factor.rgb;
    albedo_color = pow(albedo_color,vec3(2.2));

    if(transparency < current_material.alpha_cutoff && alpha_mode_mask(current_material))
        discard;

    vec3 occlusion_color = texture(texture_handles[current_material.occlusion_texture_index],tex_coord).rgb;
    vec3 normal_color    = texture(texture_handles[current_material.normal_texture_index],tex_coord).rgb * 2.0 - 1.0;
    vec3 metalic_color   = texture(texture_handles[current_material.metalic_texture_index],tex_coord).rgb;
    vec3 emissive_color  = texture(texture_handles[current_material.emissive_texture_index],tex_coord).rgb;

    vec3 world_normal = tbn * normal_color;
    vec3 final_normal =  texture_exists(current_material.normal_texture_index) ? world_normal : normal;
    float back_facing = (!gl_FrontFacing ? -1.0 : 1.0); 
    final_normal *= back_facing;

    float metallic = metalic_color.b * current_material.metalic_factor;
    float roughness = metalic_color.g * current_material.roughness_factor;
    float occlusion = metalic_color.r;
    vec3 emissive = texture_exists(current_material.emissive_texture_index) ? emissive_color : vec3(0.0);
    emissive *= current_material.emissive_factor;

    vec3 half_vec = normalize(view_vec + light_dir);

    float alpha = pow(roughness,2.0);
    float k = alpha/2.0;
    vec3 f0 = vec3(0.04);
    f0 = mix(f0,pow(albedo_color,vec3(2.2)),metallic);

    float n_dot_l = max(dot(final_normal,light_dir),0.0);
    float n_dot_v = max(dot(final_normal,view_vec),0.0);
    float n_dot_h = max(dot(final_normal,half_vec),0.0);
    float v_dot_h = max(dot(view_vec,half_vec),0.0);

    float D = pow(alpha,2.0) / (PI * pow((n_dot_h * n_dot_h * (alpha * alpha - 1.0) + 1.0),2.0));
    float G_v = n_dot_v / (n_dot_v * (1.0 - k) + k);
    float G_l = n_dot_l / (n_dot_l * (1.0 - k) + k);
    float G = G_v * G_l;
    vec3 F = f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);

    vec3 ks = F;
    vec3 kd = (vec3(1.0) - ks) * (vec3(1.0) - vec3(metallic));

    vec3 diffuse_brdf = kd * albedo_color / PI;
    vec3 specular_brdf = (D * G * F) / max(4.0 * n_dot_l * n_dot_v,0.01);
    vec3 radiance = vec3(5.0);
    // calculate it for the sun
    vec4 frag_light_space = light_ortho * light_view * vec4(frag_pos,1.0); 
    float shadow = compute_shadows(frag_light_space,final_normal);  
    float visibility = 1.0 - shadow;
    vec3 Lo = (diffuse_brdf + specular_brdf) * radiance * n_dot_l * visibility;
    
    // accumulate it for each point light
    for(int i = 0; i < num_lights; ++i) {
        vec3 L = normalize(lights[i].pos.xyz - frag_pos);
        vec3 H = normalize(view_vec + L);
        float distance = length(lights[i].pos.xyz - frag_pos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lights[i].color_intensity.rgb * attenuation;

        float NDF = DistributionGGX(world_normal, H, roughness);   
        float G   = GeometrySmith(world_normal,view_vec, L, roughness);    
        vec3 F    = fresnelSchlick(max(dot(H, view_vec), 0.0), f0);        
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(world_normal, view_vec), 0.0) * max(dot(world_normal, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	                
            
        float NdotL = max(dot(world_normal, L), 0.0);        

        Lo += (kD * albedo_color.rgb / PI + specular) * radiance * NdotL;
    }   
    
    vec3 F_schlick = fresnelSchlickRoughness(n_dot_v,f0,roughness);
    vec3 kS = F_schlick;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    vec3 irradiance = texture(irradiance_map,final_normal).rgb;
    vec3 indirect_diffuse = albedo_color * irradiance;

    vec3 reflect_vec = reflect(-view_vec,final_normal);
    const float MAX_REFLECTION_LOD = 4;
    vec3 prefilter_color = textureLod(prefilter_map, reflect_vec,  roughness * MAX_REFLECTION_LOD).rgb;  
    vec2 brdf  = texture(brdf_lut, vec2(n_dot_v, roughness)).rg;
    vec3 indirect_specular = prefilter_color * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * indirect_diffuse + indirect_specular) * occlusion;

    const float gamma = 2.2;

    vec3 pbr_color = ambient + Lo;

    pbr_color = pbr_color / (pbr_color + vec3(1.0));
    pbr_color = pow(pbr_color, vec3(1.0/gamma));  

    vec3 final_color = pbr_color;

    if(color_state == 0)
        final_color = pbr_color;
    else if(color_state == 1)
        final_color = world_normal;
    else if(color_state == 2)
        final_color = albedo_color;
    else if(color_state == 3)
        final_color = vec3(occlusion);

    FragColor = vec4(final_color,1.0); 
}
