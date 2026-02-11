#include "cglm/mat4.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define VISUAL_STUDIO
#undef VISUAL_STUDIO
#if defined(VISUAL_STUDIO) 
#define asset_path(str) "../../../assets/" str
#define path(str) "../../../" str
#else
#define asset_path(str) "../assets/" str
#define path(str) "../" str
#endif

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "Camera.h"
#include "Window.h"
#include "Global.h"
#include "Arena.h"
#include "Vector.h"

typedef struct {
    vec3 min;
    vec3 max;
} AABB;

void str_concat(const char* s1,const char* s2,char* dest) {
    u32 len1 = strlen(s1);
    u32 len2 = strlen(s2);
    memcpy(dest,s1,len1);
    memcpy(dest + len1,s2,len2);
    dest[len1 + len2] = 0;
}

void dir_parent(const char* src,char* dest) {
    i32 i = strlen(src)-1;    
    if(src[i] == '/') --i; 
    while(src[i] != '/' && i-- >= 0);
    memcpy(dest,src,i);
    dest[i] = 0;
}

static vec4 white = {1.0f,1.0f,1.0f,1.0f};

typedef struct {
    u32 count;
    u32 instanceCount;
    u32 firstIndex;
    u32 baseVertex;
    u32 baseInstance;
} DrawElementsIndirectCommand;

typedef struct {
    vec4 color;
    vec4 tangent;
    vec4 weights;
    vec3 position;
    vec3 normal;
    vec2 uv0;
    vec2 uv1;
    u64 joint_ids;
}Vertex;

typedef struct {
    vec4 color_intensity;
    vec4 pos; 
    vec4 attenuation_factors; // vec4 due to paddings
}PointLight;

#define set_alpha_mode_opaque(material) material.flags &= 0xfffffffc
#define set_alpha_mode_mask(material) material.flags = (material.flags & 0xfffffffc) | 0xfffffffd
#define set_alpha_mode_blend(material) material.flags = (material.flags & 0xfffffffc) | 0xfffffffe

#define alpha_mode_opaque(material) !(material.flags & 0)
#define alpha_mode_mask(material) (material.flags & 0x00000001)
#define alpha_mode_blend(material) (material.flags & 0x00000002)

typedef struct {
    vec4 base_color;
    vec4 base_color_factor;
    vec3 emissive_factor;
    float metalic_factor;
    float alpha_cutoff;
    float roughness_factor;
    int32_t base_color_texture_index;
    int32_t metalic_texture_index;
    int32_t normal_texture_index;
    int32_t occlusion_texture_index;
    int32_t emissive_texture_index;
    uint32_t flags;
}Material;

typedef struct {
    vector(Vertex) vertex_vector;
    vector(uint32_t) index_vector;
    vector(DrawElementsIndirectCommand) culled_backface_indirect_command_vector;
    vector(DrawElementsIndirectCommand) non_culled_backface_indirect_command_vector;
    vector(GLuint64) texture_handle_vector;
    vector(Material) material_vector;
    vector(uint32_t) culled_command_material_index_vector;
    vector(uint32_t) non_culled_command_material_index_vector;
    vector(PointLight) point_light_vector;
    AABB aabb;
    u32 vertex_array;
    u32 vertex_buffer;
    u32 index_buffer;
    u32 culled_backface_indirect_command_buffer;
    u32 non_culled_backface_indirect_command_buffer;
    u32 texture_handles_buffer;
    u32 material_buffer;
    u32 culled_command_material_index_buffer;
    u32 non_culled_command_material_index_buffer;
    u32 point_light_buffer;
}Scene;

GLuint64 bindless_texture_from_file(const char* img_path,const cgltf_sampler* sampler) { 
    int width,height,channels;
    unsigned char* data = stbi_load(img_path,&width,&height,&channels,0);
    GLenum format = GL_RGBA;

    if (channels == 1)
        format = GL_RED;
    else if (channels == 2)
        format = GL_RG;
    else if(channels == 3)
        format = GL_RGB;
    else if(channels == 4)
        format = GL_RGBA;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler ? sampler->wrap_s : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler ? sampler->wrap_t : GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler ? sampler->min_filter : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler ? sampler->mag_filter : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    GLuint64 handle = glGetTextureHandleARB(tex);
    glMakeTextureHandleResidentARB(handle);
    return handle;
}

GLuint64 bindless_texture_from_mem(const unsigned char* img_buffer,size_t len,const cgltf_sampler* sampler) { 
    int width,height,channels;
    unsigned char* data = stbi_load_from_memory(img_buffer,len,&width,&height,&channels,0);
    GLenum format = GL_RGBA;

    if(channels == 1)
        format = GL_RED;
    else if(channels == 3)
        format = GL_RGB;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler ? sampler->wrap_s : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler ? sampler->wrap_t : GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler ? sampler->min_filter : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler ? sampler->mag_filter : GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    GLuint64 handle = glGetTextureHandleARB(tex);
    glMakeTextureHandleResidentARB(handle);
    return handle;
}

GLuint bindless_missing_texture() {
    unsigned char data[] = { 255, 255, 255, 255};

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    GLuint64 handle = glGetTextureHandleARB(tex);
    glMakeTextureHandleResidentARB(handle);
    return handle;
}

void compute_dir_light_ortho(mat4 view,mat4 proj,mat4 light_view,mat4 ortho) {
    vec4 corners[8] = { 
        {-1.0f, -1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, -1.0f, 1.0f},
        { 1.0f, -1.0f, 1.0f, 1.0f}, { 1.0f, -1.0f, -1.0f, 1.0f}, { 1.0f, 1.0f, 1.0f, 1.0f}, { 1.0f, 1.0f, -1.0f, 1.0f} 
    };

    mat4 inverse_view_proj;
    glm_mat4_mul(proj,view,inverse_view_proj);
    glm_mat4_inv(inverse_view_proj,inverse_view_proj); 
    
    // transform view frustum into world space
    for(int i = 0; i < 8; i++) {
        glm_mat4_mulv(inverse_view_proj,corners[i],corners[i]);
        glm_vec4_scale(corners[i],1.0f/corners[i][3],corners[i]);
    }

    vec4 view_space_frustum_in_ws[8]; 
    memcpy(view_space_frustum_in_ws,corners,8 * 4 * sizeof(float));

    vec3 origin = {0.0,0.0,0.0};
    vec3 up = {0.0,1.0,0.0};

    AABB aabb;
    glm_vec3_copy(corners[0],aabb.min);
    glm_vec3_copy(corners[0],aabb.max);

    for(int i = 0; i < 8; i++) {
        if(aabb.min[0] > corners[i][0])
            aabb.min[0] = corners[i][0];
        if(aabb.min[1] > corners[i][1])
            aabb.min[1] = corners[i][1];
        if(aabb.min[2] > corners[i][2])
            aabb.min[2] = corners[i][2];

        if(aabb.max[0] < corners[i][0])
            aabb.max[0] = corners[i][0];
        if(aabb.max[1] < corners[i][1])
            aabb.max[1] = corners[i][1];
        if(aabb.max[2] < corners[i][2])
            aabb.max[2] = corners[i][2];
    }

    origin[0] = (aabb.min[0] + aabb.max[0]) / 2.0f;
    origin[1] = (aabb.min[1] + aabb.max[1]) / 2.0f;
    origin[2] = (aabb.min[2] + aabb.max[2]) / 2.0f;

    glm_ortho(aabb.min[0],aabb.max[0],aabb.min[1],aabb.max[1],aabb.min[2],aabb.max[2],ortho); 
}

const char* attribute_type_to_str(const cgltf_attribute_type type) {
    switch (type) {
        case cgltf_attribute_type_position:
            return "position";
            break;
        case cgltf_attribute_type_normal:
            return "normals";
            break;
        case cgltf_attribute_type_texcoord:
            return "texture_coords";
            break;
        case cgltf_attribute_type_color:
            return "color";
        case cgltf_attribute_type_tangent:
            return "tangent";
        default:
            return "unknown type";
            break;
    }
}


void process_node(Arena* arena,const cgltf_data* data,cgltf_node* node,mat4 parent_transform,Scene* scene) {
    mat4 current_transform;
    glm_mat4_identity(current_transform);
   
    if(node->has_matrix) {
        glm_mat4_mul(parent_transform,(vec4*)node->matrix,current_transform);
    }else {
        glm_translate(current_transform,node->translation);
        glm_quat_rotate(current_transform,node->rotation,current_transform);
        glm_scale(current_transform,node->scale);

        glm_mat4_mul(parent_transform,current_transform,current_transform);
    }


    if(node->mesh) {
        cgltf_mesh* mesh = node->mesh;
        for(int i = 0; i < mesh->primitives_count; i++) {
            cgltf_primitive* primitive = &mesh->primitives[i];

            if(primitive->type != cgltf_primitive_type_triangles)
                assert(0 && "Triangles are only supported");

            assert(primitive->indices && "non indexed are not supporeted");

            cgltf_accessor* acc = primitive->indices;
            size_t indices_count = acc->count;
            uint8_t* indices_ptr = (uint8_t*)acc->buffer_view->buffer->data + acc->buffer_view->offset + acc->offset;

            for (size_t k = 0; k < indices_count; k++) {
                uint32_t value;
                if (acc->component_type == cgltf_component_type_r_16u)
                    value = ((uint16_t*)indices_ptr)[k];
                else if (acc->component_type == cgltf_component_type_r_32u)
                    value = ((uint32_t*)indices_ptr)[k];
                else if (acc->component_type == cgltf_component_type_r_8u)
                    value = indices_ptr[k];
                vector_push(scene->index_vector,uint32_t,value);
            }

            uint32_t verticies_count = primitive->attributes[0].data->count;
            Vertex* verticies = arena_alloc(arena,Vertex,verticies_count);

            for(int vert = 0; vert < verticies_count; vert++)
                glm_vec4_copy(white,verticies[vert].color);

            for(int j = 0; j < primitive->attributes_count; j++) {
                cgltf_attribute* attribute = &primitive->attributes[j];
                cgltf_accessor* accessor = attribute->data;
                
                uint8_t* data_ptr = (uint8_t*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
                assert(accessor->stride && "Stride is 0!");

                switch (attribute->type) {
                    case cgltf_attribute_type_position:
                        for(int pos_index = 0; pos_index < verticies_count; pos_index++) {
                            float* pos = (float*)(data_ptr + pos_index * accessor->stride);
                            vec4 world_pos = {0.0f,0.0f,0.0f,1.0f};
                            glm_vec3_copy(pos,world_pos);
                            glm_mat4_mulv(current_transform,world_pos,world_pos);
                            glm_vec3_copy(world_pos,verticies[pos_index].position);

                            if(world_pos[0] < scene->aabb.min[0])
                                scene->aabb.min[0] = world_pos[0]; 
                            else if(world_pos[0] > scene->aabb.max[0])
                                scene->aabb.max[0] = world_pos[0]; 
                            
                            if(world_pos[1] < scene->aabb.min[1])
                                scene->aabb.min[1] = world_pos[1]; 
                            else if(world_pos[1] > scene->aabb.max[1])
                                scene->aabb.max[1] = world_pos[1]; 

                            if(world_pos[2] < scene->aabb.min[2])
                                scene->aabb.min[2] = world_pos[2]; 
                            else if(world_pos[2] > scene->aabb.max[2])
                                scene->aabb.max[2] = world_pos[2]; 
                        }                      
                        break;
                    case cgltf_attribute_type_normal:
                        for(int normal_index = 0; normal_index < verticies_count; normal_index++) {
                            float* normal = (float*)(data_ptr + normal_index * accessor->stride);
                            vec4 aux = {1.0f};
                            glm_vec3_copy(normal,aux); 
                            glm_mat4_mulv(current_transform,aux,aux); 
                            glm_vec3_copy(aux,verticies[normal_index].normal);
                        }                      
                        break;
                    case cgltf_attribute_type_texcoord:
                        int index = attribute->index;
                        for(int uv_index = 0; uv_index < verticies_count; uv_index++) {
                            float* uv = (float*)(data_ptr + uv_index * accessor->stride);
                            glm_vec2_copy(uv,index == 0 ? verticies[uv_index].uv0 : verticies[uv_index].uv1);
                        }
                        break;
                    case cgltf_attribute_type_color:
                        for(int color_index = 0; color_index < verticies_count; color_index++) {
                            float* color = (float*)(data_ptr + color_index * accessor->stride);
                            if(accessor->type == cgltf_type_vec3) {
                                glm_vec3_copy(color,verticies[color_index].color);
                                verticies[color_index].color[3] = 1.0f;
                            } else if(accessor->type == cgltf_type_vec4) {
                                glm_vec4_copy(color,verticies[color_index].color);
                            }
                        }
                        break;
                    case cgltf_attribute_type_tangent:
                        for(int tangent_index = 0; tangent_index < verticies_count; tangent_index++) {
                            float* tangent = (float*)(data_ptr + tangent_index * accessor->stride);
                            glm_vec4_copy(tangent,verticies[tangent_index].tangent);
                            glm_mat4_mulv(current_transform,verticies[tangent_index].tangent,verticies[tangent_index].tangent); 
                        }
                        break;
                    case cgltf_attribute_type_joints:
                        assert(accessor->type == cgltf_type_vec4 && "If this fails then there are less than 3 joint ids");
                        assert(accessor->component_type != cgltf_component_type_r_8u && "Component type is not unsigend byte!!");

                        for(int joint_index = 0; joint_index < verticies_count; joint_index++) {
                            verticies[joint_index].joint_ids = *(u64*)(data_ptr + joint_index * accessor->stride);
                        }
                        break;
                    case cgltf_attribute_type_weights:
                        for(int weights_index = 0; weights_index < verticies_count; weights_index++) {
                            float* weights = (float*)(data_ptr + weights_index * accessor->stride);
                                glm_vec4_copy(weights,verticies[weights_index].weights);
                        }
                        break;
                    default:
                        printf("Missing attribute: %s\n",attribute_type_to_str(attribute->type));
                        break;
                }
            }

            vector_push_array(scene->vertex_vector,Vertex,verticies,verticies_count);
            
            DrawElementsIndirectCommand command = {
                indices_count,
                1,
                scene->index_vector.size - indices_count,
                scene->vertex_vector.size - verticies_count,
                0
            };
            
            cgltf_material* material = primitive->material;
            uint32_t material_index = 0;
            if(material) {
                material_index = material - data->materials + 1; 
                printf("[DEBUG] Material \"%s\" at index %d\n",material->name,material_index);
            } else {
                printf("[DEBUG] primitive does not have a material\n");
            }
            
            if(material->double_sided) {
                vector_push(scene->non_culled_command_material_index_vector,uint32_t,material_index);
                vector_push(scene->non_culled_backface_indirect_command_vector,DrawElementsIndirectCommand,command);
            } else {
                vector_push(scene->culled_command_material_index_vector,uint32_t,material_index);
                vector_push(scene->culled_backface_indirect_command_vector,DrawElementsIndirectCommand,command);
            }
        }

    }
   
    for(int n = 0; n < node->children_count; n++) {
        process_node(arena,data,node->children[n],current_transform,scene);
    }
}

void load_scene_from_gltf(Arena* arena,const char* folder_path,const char* file_name,Scene* scene) { 
    cgltf_options options = {0};
    cgltf_data* data = NULL;

    vec3 min = {FLT_MAX,FLT_MAX,FLT_MAX};
    vec3 max = {FLT_MIN,FLT_MIN,FLT_MIN};

    glm_vec3_copy(min,scene->aabb.min);
    glm_vec3_copy(max,scene->aabb.max);

    char* inter_path = arena_alloc(arena,char,strlen(folder_path) + 1 + 1);
    str_concat(folder_path,"/",inter_path);
    char* path = arena_alloc(arena,char,strlen(inter_path) + strlen(file_name) + 1);
    str_concat(inter_path,file_name,path);

    cgltf_result result = cgltf_parse_file(&options, path, &data);

    if (result != cgltf_result_success) {
        fprintf(stderr, "%s %d", "Failed to parse model",__LINE__);
        return;
    }

    result = cgltf_load_buffers(&options, data, path); 

    if (result != cgltf_result_success) {
        fprintf(stderr, "%s %d", "Failed to load buffers",__LINE__);
        return;
    }

    result = cgltf_validate(data);

    if (result != cgltf_result_success) {
        fprintf(stderr, "%s %d", "Failed to validate buffers",__LINE__);
        return;
    }

    for(int i = 0; i < data->textures_count; i++) {
        cgltf_texture* texture = &data->textures[i];
        cgltf_image* img = texture->image;

        GLuint64 bindless_handle = 0;
        if (img->buffer_view) {
            uint8_t* buffer_data = (uint8_t*)img->buffer_view->buffer->data + img->buffer_view->offset;
            size_t len = img->buffer_view->size;
            bindless_handle = bindless_texture_from_mem(buffer_data,len,texture->sampler);
        } 
        else if (img->uri) {
            if (strncmp(img->uri, "data:", 5) == 0) {
                assert(strchr(img->uri,','));
                const char* encoded_data = strchr(img->uri,',') + 1;
                void* decoded_data = NULL; 
                size_t len = strlen(encoded_data);
                size_t byte_len = (len * 3) / 4;

                cgltf_result result = cgltf_load_buffer_base64(&options,len,encoded_data,&decoded_data);
                if(result != cgltf_result_success) {
                    fprintf(stderr, "%s %d", "Failed to deecode base64 string!",__LINE__);
                    return; 
                }
                bindless_handle = bindless_texture_from_mem(decoded_data,byte_len,texture->sampler);  
                free(decoded_data); 
            } else {
                char* texture_path = arena_alloc(arena,char,strlen(inter_path) + strlen(img->uri) + 1);                   
                str_concat(inter_path,img->uri,texture_path);
                bindless_handle = bindless_texture_from_file(texture_path,texture->sampler);
            }
        }
        assert(bindless_handle && "Failed to set bindless handle");
        vector_push(scene->texture_handle_vector,GLuint64,bindless_handle);
    }

    for(int i = 0; i < data->materials_count; i++) {
        cgltf_material* mat = &data->materials[i];
         
        Material material = {0};

        const cgltf_texture* first_texture = data->textures; 
        int texture_index = 0;
         
        material.alpha_cutoff = mat->alpha_cutoff;
         
        switch(mat->alpha_mode) {
            case cgltf_alpha_mode_opaque:
                set_alpha_mode_opaque(material);
                break;
            case cgltf_alpha_mode_mask:
                set_alpha_mode_mask(material);
                break;
            case cgltf_alpha_mode_blend:
                set_alpha_mode_blend(material);
                break;
            default:
                break;
        }

        if(mat->normal_texture.texture) {
            texture_index = mat->normal_texture.texture - first_texture;
            printf("[DEBUG] normal map texture \"%s\" at index %d\n",mat->name,texture_index);
            material.normal_texture_index = texture_index + 1;
        }
        if(mat->occlusion_texture.texture) {
            texture_index = mat->occlusion_texture.texture - first_texture;
            printf("[DEBUG] ambient occlusion map texture \"%s\" at index %d\n",mat->name,texture_index);
            material.occlusion_texture_index = texture_index + 1;
        }
        if(mat->emissive_texture.texture) {
            texture_index = mat->emissive_texture.texture - first_texture;
            printf("[DEBUG] emissive texture \"%s\" at index %d\n",mat->name,texture_index);
            material.emissive_texture_index = texture_index + 1;
            glm_vec3_copy(mat->emissive_factor,material.emissive_factor);
        }

        if(mat->has_pbr_metallic_roughness) {
            glm_vec4_copy(mat->pbr_metallic_roughness.base_color_factor,material.base_color_factor);
            if(mat->pbr_metallic_roughness.base_color_texture.texture) {
                texture_index = mat->pbr_metallic_roughness.base_color_texture.texture - first_texture;
                printf("[DEBUG] base color map texture \"%s\" at index %d\n",mat->name,texture_index);
                material.base_color_texture_index = texture_index + 1;
            }
            if (mat->pbr_metallic_roughness.metallic_roughness_texture.texture) {
                texture_index = mat->pbr_metallic_roughness.metallic_roughness_texture.texture - first_texture;
                printf("[DEBUG] metallic map texture \"%s\" at index %d\n",mat->name,texture_index);
                material.metalic_texture_index = texture_index + 1;
            }
            material.metalic_factor = mat->pbr_metallic_roughness.metallic_factor;
            material.roughness_factor = mat->pbr_metallic_roughness.roughness_factor;
            glm_vec4_copy(mat->pbr_metallic_roughness.base_color_factor,material.base_color);

        } else if(mat->has_pbr_specular_glossiness) {
            assert(0 && "Glosiness not yet supported");

            glm_vec4_copy(mat->pbr_specular_glossiness.diffuse_factor,material.base_color_factor);

            if(mat->pbr_specular_glossiness.diffuse_texture.texture) {
                texture_index = mat->pbr_specular_glossiness.diffuse_texture.texture - first_texture;
                material.base_color_texture_index = texture_index;  
            }
            if (mat->pbr_specular_glossiness.specular_glossiness_texture.texture) {
                texture_index = mat->pbr_specular_glossiness.specular_glossiness_texture.texture - first_texture;
                material.metalic_texture_index = texture_index;
            }
        }else {
            assert(0 && "Other PBR workflows are NOT supported");
        }

        vector_push(scene->material_vector,Material,material);
    }

    cgltf_scene* gltf_scene = &data->scenes[0];
    for(int node_index = 0; node_index < gltf_scene->nodes_count; node_index++) {
        mat4 identity = GLM_MAT4_IDENTITY_INIT;
        process_node(arena,data,gltf_scene->nodes[node_index],identity,scene);
    }
    cgltf_free(data);
}

void scene_init(Scene* scene) {
    vector_create(scene->index_vector,uint32_t);
    vector_create(scene->vertex_vector,Vertex);
    vector_create(scene->culled_backface_indirect_command_vector,DrawElementsIndirectCommand);
    vector_create(scene->non_culled_backface_indirect_command_vector,DrawElementsIndirectCommand);
    vector_create(scene->texture_handle_vector,GLuint64);
    vector_create(scene->material_vector,Material);
    vector_create(scene->culled_command_material_index_vector,uint32_t);
    vector_create(scene->non_culled_command_material_index_vector,uint32_t);
    vector_create(scene->point_light_vector,PointLight);
}

void scene_buffers_init(Scene* scene) {
    assert(scene->vertex_vector.size && "Vertex Vector is empty!");
    assert(scene->index_vector.size && "Index Vector is empty!");

    glGenVertexArrays(1,&scene->vertex_array);
    glBindVertexArray(scene->vertex_array);

    glGenBuffers(1,&scene->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER,scene->vertex_buffer); 
    glBufferData(GL_ARRAY_BUFFER,scene->vertex_vector.size * sizeof(Vertex),scene->vertex_vector.data,GL_STATIC_DRAW);

    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,color));    // color
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,tangent));  // tangent 
    glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,weights));  // weights 
    glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,position)); // pos
    glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,normal));   // normal
    glVertexAttribPointer(5,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,uv0));      // uv0
    glVertexAttribPointer(6,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,uv1));      // uv1
    glVertexAttribIPointer(7,4,GL_UNSIGNED_SHORT,sizeof(Vertex),(void*)offsetof(Vertex,joint_ids)); // bone ids 
    
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glEnableVertexAttribArray(7);

    glGenBuffers(1,&scene->index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,scene->index_buffer); 
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,scene->index_vector.size * sizeof(uint32_t),scene->index_vector.data,GL_STATIC_DRAW);
    
    glBindVertexArray(0);

    glGenBuffers(1, &scene->culled_backface_indirect_command_buffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, scene->culled_backface_indirect_command_buffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, 
                 scene->culled_backface_indirect_command_vector.size * sizeof(DrawElementsIndirectCommand),
                 scene->culled_backface_indirect_command_vector.data, GL_STATIC_DRAW);

    glGenBuffers(1, &scene->non_culled_backface_indirect_command_buffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, scene->non_culled_backface_indirect_command_buffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, 
                 scene->non_culled_backface_indirect_command_vector.size * sizeof(DrawElementsIndirectCommand),
                 scene->non_culled_backface_indirect_command_vector.data, GL_STATIC_DRAW);

    glGenBuffers(1,&scene->texture_handles_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,scene->texture_handles_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,scene->texture_handle_vector.size * sizeof(GLuint64) , scene->texture_handle_vector.data, GL_STATIC_DRAW);

    glGenBuffers(1,&scene->material_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,scene->material_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,scene->material_vector.size * sizeof(Material) , scene->material_vector.data, GL_STATIC_DRAW);

    glGenBuffers(1,&scene->culled_command_material_index_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,scene->culled_command_material_index_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,scene->culled_command_material_index_vector.size * sizeof(uint32_t) , scene->culled_command_material_index_vector.data, GL_STATIC_DRAW);

    glGenBuffers(1,&scene->non_culled_command_material_index_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,scene->non_culled_command_material_index_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,scene->non_culled_command_material_index_vector.size * sizeof(uint32_t) , scene->non_culled_command_material_index_vector.data, GL_STATIC_DRAW);
   
    glGenBuffers(1,&scene->point_light_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER,scene->point_light_buffer); 
    glBufferData(GL_UNIFORM_BUFFER,sizeof(u32)*4 + scene->point_light_vector.size * sizeof(PointLight),NULL,GL_STATIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof(u32),&scene->point_light_vector.size);
    glBufferSubData(GL_UNIFORM_BUFFER,4*sizeof(u32),scene->point_light_vector.size * sizeof(PointLight),scene->point_light_vector.data);
}

void update_light_buffer(Scene* scene) {
    glBindBuffer(GL_UNIFORM_BUFFER,scene->point_light_buffer); 
    glBufferData(GL_UNIFORM_BUFFER,sizeof(u32)*4 + scene->point_light_vector.size * sizeof(PointLight),NULL,GL_STATIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER,0,sizeof(u32),&scene->point_light_vector.size);
    glBufferSubData(GL_UNIFORM_BUFFER,4*sizeof(u32),scene->point_light_vector.size * sizeof(PointLight),scene->point_light_vector.data);
}

void scene_data_destroy(Scene* scene_data) {
    
}

char* readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

GLuint shaderProgramCreate(const char* vs_path, const char* fs_path) {
    const char* vs_src = readFile(vs_path); 
    const char* fs_src = readFile(fs_path); 

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    GLint ok;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(vs, 512, NULL, log);
        fprintf(stderr, "VERTEX SHADER %s\n", log);
        glDeleteShader(vs);
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(fs, 512, NULL, log);
        fprintf(stderr, "FRAGMENT SHADER %s\n", log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        fprintf(stderr, "%s\n", log);
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(prog);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    free((char*)vs_src);
    free((char*)fs_src);
    return prog;
}

void shaderBind(GLuint program) {
    glUseProgram(program);
}

void shaderUnbind() {
    glUseProgram(0);
}

void shaderDestroy(GLuint program) {
    glDeleteProgram(program);
}

void shaderSetMat4Uniform(GLuint program,const char* name,mat4 value) {
    glUniformMatrix4fv(glGetUniformLocation(program,name),1,GL_FALSE,(const float*)value);
}

void shaderSetInt(GLuint program, const char* name,int value) {
    glUniform1i(glGetUniformLocation(program,name),value);
}

void shaderSetFloat(GLuint program, const char* name,float value) {
    glUniform1f(glGetUniformLocation(program,name),value);
}

typedef struct {
    u16 width;
    u16 height;
}TextureAtlas;

#define printVec3(vec) printf(#vec); printf(" : %.2f %.2f %.2f\n",vec[0],vec[1],vec[2])
#define printVec2(vec) printf(#vec); printf(" : %.2f %.2f\n",vec[0],vec[1])

void indexToUV(TextureAtlas atlas,i32 index, vec2 topLeft,vec2 topRight,vec2 bottomLeft,vec2 bottomRight) {
    ivec2 tilePos = { index%atlas.width, index/atlas.width };
    float xoffset = (float)tilePos[0]/atlas.width;
    float yoffset = (float)tilePos[1]/atlas.height;

    float xInc = 1.0f/atlas.width;
    float yInc = 1.0f/atlas.height;

    topLeft[0]     = xoffset;
    topLeft[1]     = 1.0f - yoffset; 

    topRight[0]    = topLeft[0] + xInc;
    topRight[1]    = topLeft[1];

    bottomLeft[0]  = topLeft[0];
    bottomLeft[1]  = topLeft[1] - yInc;

    bottomRight[0] =  topLeft[0] + xInc;
    bottomRight[1] =  topLeft[1] - yInc;
}

TextureAtlas createAtlas(i32 imgWidth,i32 imgHeight,i32 tileDim) {
    TextureAtlas atlas = { imgWidth/tileDim, imgHeight/tileDim };
    return atlas;
}

void scene_draw(Scene* scenes,u32 count,uint32_t shader_program,vec3 camera_pos,bool wireframe,u32 depth_map,mat4 light_view,mat4 light_ortho,vec3 light_dir) {
    shaderBind(shader_program);
    glUniform3f(glGetUniformLocation(shader_program,"camera_pos"),camera_pos[0],camera_pos[1],camera_pos[2]);
    glUniform3f(glGetUniformLocation(shader_program,"light_dir"),light_dir[0],light_dir[1],light_dir[2]);
    shaderSetMat4Uniform(shader_program,"light_view",light_view);
    shaderSetMat4Uniform(shader_program,"light_ortho",light_ortho);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,depth_map);

    for(int i = 0; i < count; i++) {
        update_light_buffer(&scenes[i]);

        glBindVertexArray(scenes[i].vertex_array); 

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,scenes[i].texture_handles_buffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,scenes[i].material_buffer);
        glBindBufferBase(GL_UNIFORM_BUFFER,3,scenes[i].point_light_buffer);

        if(scenes[i].culled_backface_indirect_command_vector.size) {
            glEnable(GL_CULL_FACE);
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER,scenes[i].culled_backface_indirect_command_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,scenes[i].culled_command_material_index_buffer);
            glMultiDrawElementsIndirect(wireframe ? GL_LINES : GL_TRIANGLES,GL_UNSIGNED_INT,0,scenes[i].culled_backface_indirect_command_vector.size,0);
        }

        if(scenes[i].non_culled_backface_indirect_command_vector.size) {
            glDisable(GL_CULL_FACE);
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER,scenes[i].non_culled_backface_indirect_command_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,scenes[i].non_culled_command_material_index_buffer);
            glMultiDrawElementsIndirect(wireframe ? GL_LINES : GL_TRIANGLES,GL_UNSIGNED_INT,0,scenes[i].non_culled_backface_indirect_command_vector.size,0);
        }
    }
}


void render_directional_shadowmap(const Scene* scenes,u32 count,mat4 ortho,mat4 light_view,vec3 light_dir,u32 dir_shadowmap_shader,u32* shadow_map_texture) {
    static u32 fbo = 0;

    if(!fbo) {
        glGenTextures(1, shadow_map_texture);
        glBindTexture(GL_TEXTURE_2D, *shadow_map_texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 2048, 2048, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float border_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

        glGenFramebuffers(1,&fbo);
        glBindFramebuffer(GL_FRAMEBUFFER,fbo); 

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, *shadow_map_texture,0);

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    glViewport(0, 0, 2048, 2048);

    shaderBind(dir_shadowmap_shader);

    shaderSetMat4Uniform(dir_shadowmap_shader,"view",light_view);
    shaderSetMat4Uniform(dir_shadowmap_shader,"ortho",ortho);

    glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glCullFace(GL_FRONT);
    glClear(GL_DEPTH_BUFFER_BIT);

    for(int i = 0; i < count; i++) {
        glBindVertexArray(scenes[i].vertex_array); 

        if(scenes[i].culled_backface_indirect_command_vector.size) {
            glEnable(GL_CULL_FACE);
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER,scenes[i].culled_backface_indirect_command_buffer);
            glMultiDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_INT,0,scenes[i].culled_backface_indirect_command_vector.size,0);
        }

        if(scenes[i].non_culled_backface_indirect_command_vector.size) {
            glDisable(GL_CULL_FACE);
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER,scenes[i].non_culled_backface_indirect_command_buffer);
            glMultiDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_INT,0,scenes[i].non_culled_backface_indirect_command_vector.size,0);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glCullFace(GL_BACK);
}

void APIENTRY debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, 
                            GLsizei length, const GLchar* message, const void* userParam) {
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
    if (type == GL_DEBUG_TYPE_ERROR) exit(1);
    else fprintf(stderr, "HELLO WORLD");
}


bool load_hdi_skybox(const char* path,GLuint* hdr_texture,GLuint* env_map) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, components;
    float *data = stbi_loadf(path, &width, &height, &components, 0);
    stbi_set_flip_vertically_on_load(false);

    if(!data)
        return false;

    glGenTextures(1, hdr_texture);
    glBindTexture(GL_TEXTURE_2D, *hdr_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); 

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    glGenTextures(1, env_map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *env_map);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 
                     512, 512, 0, GL_RGB, GL_FLOAT, NULL);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return true;
}

void skybox_buffers_init(unsigned int* vao_out,unsigned int* fbo_out,unsigned int* rbo_out) {
    static unsigned int vao,vbo,ebo,fbo,rbo;
    static const float unit_cube_verticies[] = {
        -0.5f, -0.5f,  0.5f, 
         0.5f, -0.5f,  0.5f, 
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f, 
        -0.5f, -0.5f, -0.5f, 
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f
    };

    static const uint32_t unit_cube_indices[] = {
        0, 1, 2, 2, 3, 0, 
        1, 5, 6, 6, 2, 1,
        7, 6, 5, 5, 4, 7,
        4, 0, 3, 3, 7, 4,
        4, 5, 1, 1, 0, 4,
        3, 2, 6, 6, 7, 3
    };

    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);

    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(unit_cube_verticies),unit_cube_verticies,GL_STATIC_DRAW);

    glGenBuffers(1,&ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(unit_cube_indices),unit_cube_indices,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3 * sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    *vao_out = vao;
    *fbo_out = fbo;
    *rbo_out = rbo;
}

void eq_rec_to_cubemap(unsigned int vao,unsigned int fbo,unsigned int hdr_texture,unsigned int cubemap_texture,unsigned int shader) {
    mat4 capture_proj;
    glm_perspective(glm_rad(90.0),1.0f, 0.1f,10.0f,capture_proj); 

    mat4 capture_views[6];  
    vec3 origin = {0.0};
    vec3 left   = {1.0f,0.0f,0.0};
    vec3 right  = {-1.0f,0.0f,0.0};
    vec3 up     = {0.0f,1.0f,0.0};
    vec3 down   = {0.0f,-1.0f,0.0};
    vec3 front  = {0.0f,0.0f,1.0};
    vec3 back   = {0.0f,0.0f,-1.0};
    vec3 up1 = {0.0,-1.0f,0.0};
    vec3 up2 = {0.0,0.0f,1.0};
    vec3 up3 = {0.0,0.0,-1.0};

    glm_lookat(origin, left,  up1, capture_views[0]);
    glm_lookat(origin, right, up1, capture_views[1]);
    glm_lookat(origin, up,    up2, capture_views[2]);
    glm_lookat(origin, down,  up3, capture_views[3]);
    glm_lookat(origin, front, up1, capture_views[4]);
    glm_lookat(origin, back,  up1, capture_views[5]);

    shaderBind(shader);
    shaderSetInt(shader,"equirectangularMap",0);
    shaderSetMat4Uniform(shader,"projection",capture_proj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,hdr_texture);

    glViewport(0, 0, 512, 512);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    for (unsigned int i = 0; i < 6; ++i) {
        shaderSetMat4Uniform(shader,"view",capture_views[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap_texture, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(vao); 
        glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0); 
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  
}

void create_irradiance_cubemap(unsigned int vao,unsigned int fbo,unsigned int rbo,unsigned int shader,unsigned int env_map,unsigned int* irradiance_map) {
    mat4 capture_proj;
    glm_perspective(glm_rad(90.0),1.0f, 0.1f,10.0f,capture_proj); 

    mat4 capture_views[6];  
    vec3 origin = {0.0};
    vec3 left   = {1.0f,0.0f,0.0};
    vec3 right  = {-1.0f,0.0f,0.0};
    vec3 up     = {0.0f,1.0f,0.0};
    vec3 down   = {0.0f,-1.0f,0.0};
    vec3 front  = {0.0f,0.0f,1.0};
    vec3 back   = {0.0f,0.0f,-1.0};
    vec3 up1 = {0.0,-1.0f,0.0};
    vec3 up2 = {0.0,0.0f,1.0};
    vec3 up3 = {0.0,0.0,-1.0};

    glm_lookat(origin, left,  up1, capture_views[0]);
    glm_lookat(origin, right, up1, capture_views[1]);
    glm_lookat(origin, up,    up2, capture_views[2]);
    glm_lookat(origin, down,  up3, capture_views[3]);
    glm_lookat(origin, front, up1, capture_views[4]);
    glm_lookat(origin, back,  up1, capture_views[5]);

    glGenTextures(1, irradiance_map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *irradiance_map);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGB16F, 32, 32);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    shaderBind(shader);  
    shaderSetInt(shader,"enviromentMap",0);
    shaderSetMat4Uniform(shader,"projection",capture_proj);
    glActiveTexture(GL_TEXTURE0); 
    glBindTexture(GL_TEXTURE_CUBE_MAP,env_map);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

    glViewport(0, 0, 32, 32);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    for (unsigned int i = 0; i < 6; ++i) {
        shaderSetMat4Uniform(shader,"view", capture_views[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *irradiance_map, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(vao); 
        glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0); 
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void prefilter_cubemap(unsigned int vao,unsigned int fbo, unsigned int rbo,unsigned int shader,unsigned int env_map,unsigned int *prefilter_map) {
    mat4 capture_proj;
    glm_perspective(glm_rad(90.0),1.0f, 0.1f,10.0f,capture_proj); 

    mat4 capture_views[6];  
    vec3 origin = {0.0};
    vec3 left   = {1.0f,0.0f,0.0};
    vec3 right  = {-1.0f,0.0f,0.0};
    vec3 up     = {0.0f,1.0f,0.0};
    vec3 down   = {0.0f,-1.0f,0.0};
    vec3 front  = {0.0f,0.0f,1.0};
    vec3 back   = {0.0f,0.0f,-1.0};
    vec3 up1 = {0.0,-1.0f,0.0};
    vec3 up2 = {0.0,0.0f,1.0};
    vec3 up3 = {0.0,0.0,-1.0};

    glm_lookat(origin, left,  up1, capture_views[0]);
    glm_lookat(origin, right, up1, capture_views[1]);
    glm_lookat(origin, up,    up2, capture_views[2]);
    glm_lookat(origin, down,  up3, capture_views[3]);
    glm_lookat(origin, front, up1, capture_views[4]);
    glm_lookat(origin, back,  up1, capture_views[5]);

    glGenTextures(1, prefilter_map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *prefilter_map);
    for (unsigned int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    shaderBind(shader);
    shaderSetInt(shader, "environmentMap", 0);
    shaderSetMat4Uniform(shader, "projection", capture_proj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env_map);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    unsigned int maxMipLevels = 5;

    for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
        unsigned int mipWidth = (unsigned int)(128 * pow(0.5, mip));
        unsigned int mipHeight = (unsigned int)(128 * pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        shaderSetFloat(shader, "roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i) {
            shaderSetMat4Uniform(shader, "view", capture_views[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *prefilter_map, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void generate_brdf_lut(unsigned int fbo,unsigned int rbo,unsigned int shader,unsigned int *brdf_lut) {
    unsigned int vao, vbo, ebo;
    float vertices[] = {
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f, // Top Left
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, // Bottom Left
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f, // Bottom Right
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f  // Top Right
    };

    unsigned int indices[] = {
        0, 1, 2, // First Triangle
        0, 2, 3  // Second Triangle
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glGenTextures(1, brdf_lut);

    glBindTexture(GL_TEXTURE_2D, *brdf_lut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *brdf_lut, 0);

    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shaderBind(shader);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_skybox_cube(unsigned int vao,unsigned int env_map,unsigned int shader) {
    shaderSetInt(shader, "environmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP,env_map);
    glBindVertexArray(vao); 
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0);
}

void draw_quad(u32 shader,u32 texture,float len) {
    static u32 vbo = 0;
    static u32 ebo = 0;
    static u32 vao = 0;

    shaderBind(shader);
    
    if(!vao && !vbo && !ebo) {
        static float vertices[] = {
            -1.0f ,  1.0f, 0.0f,  0.0f, 1.0f, // Top Left
            -1.0f,  0.5f, 0.0f,  0.0f, 0.0f, // Bottom Left
            -0.5f, 0.5, 0.0f,  1.0f, 0.0f, // Bottom Right
            -0.5,  1.0f, 0.0f,  1.0f, 1.0f  // Top Right
        };

        static unsigned int indices[] = {
            0, 1, 2, // First Triangle
            0, 2, 3  // Second Triangle
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);

        shaderSetInt(shader,"sampler",0);
    }

    glClear(GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,texture);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void scene_translate(Scene* scene,vec3 translation) {
    for(int i = 0; i < scene->vertex_vector.size; i++)
        glm_vec3_add(translation,scene->vertex_vector.data[i].position,scene->vertex_vector.data[i].position);
}

void scene_scale(Scene* scene,vec3 scale) {
    for(int i = 0; i < scene->vertex_vector.size; i++)
        glm_vec3_mul(scale,scene->vertex_vector.data[i].position,scene->vertex_vector.data[i].position);
}

int main() {
    Arena arena;
        arena_create(&arena,MB(100));

    if (!glfwInit()) {
        printf("Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Window* window = windowCreate(1280,960,"CCraft");
    if (!window) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    glDebugMessageCallback(debug_callback, NULL);

    if(!GLAD_GL_ARB_bindless_texture) {
        printf("bindless textures are not supported\n");
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDepthFunc(GL_LEQUAL);

    GLuint defaultProgram = shaderProgramCreate(path("shaders/default_vs.glsl"),path("shaders/default_fs.glsl"));
    GLuint eqrec_to_cubemap_shader = shaderProgramCreate(path("shaders/cubemap_vs.glsl"),path("shaders/eqrec_to_cubemap_fs.glsl"));
    GLuint irradiance_shader = shaderProgramCreate(path("shaders/cubemap_vs.glsl"),path("shaders/irradiance_convolution_fs.glsl"));
    GLuint background_shader = shaderProgramCreate(path("shaders/background_vs.glsl"),path("shaders/background_fs.glsl"));
    GLuint prefilter_shader = shaderProgramCreate(path("shaders/cubemap_vs.glsl"),path("shaders/prefilter_fs.glsl"));
    GLuint brdf_shader = shaderProgramCreate(path("shaders/brdf_vs.glsl"),path("shaders/brdf_fs.glsl"));
    GLuint shadowmap_shader = shaderProgramCreate(path("shaders/shadow_map_vs.glsl"),path("shaders/shadow_map_fs.glsl"));
    GLuint quad_shader = shaderProgramCreate(path("shaders/quad_vs.glsl"),path("shaders/quad_fs.glsl"));

    Camera defaultCam;
    cameraDefaultInit(&defaultCam);
    mat4 proj;
    mat4 view;
    cameraViewMat(&defaultCam,view);
    glm_perspective(glm_rad(90.0f),windowAspectRatio(window),0.01f,100.0f,proj);
    
    GLuint hdr_texture,env_map,irradiance_map,skybox_vao,skybox_fbo,skybox_rbo,prefilter_map,brdf_lut;
    load_hdi_skybox(asset_path("skybox.hdr"),&hdr_texture,&env_map);
    skybox_buffers_init(&skybox_vao,&skybox_fbo,&skybox_rbo);
    eq_rec_to_cubemap(skybox_vao,skybox_fbo,hdr_texture,env_map,eqrec_to_cubemap_shader);
    create_irradiance_cubemap(skybox_vao,skybox_fbo,skybox_rbo,irradiance_shader,env_map,&irradiance_map);
    prefilter_cubemap(skybox_vao, skybox_fbo, skybox_rbo, prefilter_shader,env_map, &prefilter_map);
    generate_brdf_lut(skybox_fbo, skybox_rbo, brdf_shader, &brdf_lut);

    GLuint64 bindless_irradiance_cubemap = glGetTextureHandleARB(irradiance_map);
    GLuint64 prefilter_bindless = glGetTextureHandleARB(prefilter_map);
    GLuint64 brdf_bindless = glGetTextureHandleARB(brdf_lut);

    glMakeTextureHandleResidentARB(bindless_irradiance_cubemap);
    glMakeTextureHandleResidentARB(prefilter_bindless);
    glMakeTextureHandleResidentARB(brdf_bindless);

    shaderBind(defaultProgram);
    shaderSetInt(defaultProgram,"dir_shadowmap",0);
    glUniformHandleui64ARB(glGetUniformLocation(defaultProgram, "irradiance_map"), bindless_irradiance_cubemap);
    glUniformHandleui64ARB(glGetUniformLocation(defaultProgram, "prefilter_map"), prefilter_bindless);
    glUniformHandleui64ARB(glGetUniformLocation(defaultProgram, "brdf_lut"), brdf_bindless);
    shaderBind(0);

    glViewport(0, 0, windowWidth(window), windowHeight(window));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    float lastFrame = 0.0f;
    float deltaTime = 0.0f;

    bool cameraMoved = false;
    
    GLuint64 missing_texture_handle = bindless_missing_texture(); 
    Material missing_material = {0};
    missing_material.base_color[0] = 1.0f;
    missing_material.base_color[1] = 1.0f;
    missing_material.base_color[2] = 1.0f;
    missing_material.base_color[3] = 1.0f;

    Scene scenes[2];
    u32 scene_count = sizeof(scenes) / sizeof(Scene);

    scene_init(&scenes[0]);
    vector_push(scenes[0].texture_handle_vector,GLuint64,missing_texture_handle);
    vector_push(scenes[0].material_vector,Material,missing_material);

    PointLight camera_light = { .color_intensity = {1.0f,0.0f,1.0f,0.5f}, .pos = {defaultCam.pos[0],defaultCam.pos[1],defaultCam.pos[2] }, .attenuation_factors = {0.5,0.5,0.5} };
    vector_push(scenes[0].point_light_vector,PointLight,camera_light);

    load_scene_from_gltf(&arena,asset_path("city"), "scene.gltf", &scenes[0]);
    scene_buffers_init(&scenes[0]);    


    scene_init(&scenes[1]);
    vector_push(scenes[1].texture_handle_vector,GLuint64,missing_texture_handle);
    vector_push(scenes[1].material_vector,Material,missing_material);

    vector_push(scenes[1].point_light_vector,PointLight,camera_light);

    load_scene_from_gltf(&arena,asset_path("car"), "scene.gltf", &scenes[1]);
    vec3 scale = { 0.01f, 0.01f, 0.01f };
    vec3 translation = {-5.0,0.25,0.0};
    scene_scale(&scenes[1],scale);
    scene_translate(&scenes[1],translation);
    
    scene_buffers_init(&scenes[1]);    


    bool wireframe = false;
    u32 depth_map = 0;

    while (!windowShouldClose(window)) {

        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        glClearColor(32.0f/255.0f,167.0f/255.0f,219.0f/255.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if(windowMouseMoved(window)) {
            vec2 mousePos;
            windowMousePos(window,mousePos);
            cameraUpdateMouse(&defaultCam,mousePos,windowPaused(window));
            cameraViewMat(&defaultCam,view);
        }

        cameraMoved = false;
        if (windowGetKey(window,GLFW_KEY_W) == GLFW_PRESS) {
            cameraUpdateOnMovement(&defaultCam,FORWARD,deltaTime);
            cameraViewMat(&defaultCam,view);
            cameraMoved = true;
        }
        if (windowGetKey(window,GLFW_KEY_S) == GLFW_PRESS) {
            cameraUpdateOnMovement(&defaultCam,BACKWARD,deltaTime);
            cameraViewMat(&defaultCam,view);
            cameraMoved = true;
        }
        if (windowGetKey(window,GLFW_KEY_A) == GLFW_PRESS) {
            cameraUpdateOnMovement(&defaultCam,LEFT_SIDE,deltaTime);
            cameraViewMat(&defaultCam,view);
            cameraMoved = true;
        }
        if (windowGetKey(window,GLFW_KEY_D) == GLFW_PRESS) {
            cameraUpdateOnMovement(&defaultCam,RIGHT_SIDE,deltaTime);
            cameraViewMat(&defaultCam,view);
            cameraMoved = true;
        }

        if(windowGetKey(window,GLFW_KEY_ESCAPE) == GLFW_PRESS)
            windowToggleMouseLock(window);

        if(windowResized(window))
            glm_perspective(glm_rad(90.0f),windowAspectRatio(window),0.01f,100.0f,proj);

        shaderBind(defaultProgram);
        shaderSetMat4Uniform(defaultProgram, "proj", proj);
        shaderSetMat4Uniform(defaultProgram, "view", view);
        
        int q_state = windowGetKey(window,GLFW_KEY_Q); 
        static bool lock = false;
        if(q_state == GLFW_PRESS && !lock) {
            wireframe = !wireframe;
            if(wireframe)
                glPolygonMode( GL_FRONT_AND_BACK,GL_LINE);
            else 
                glPolygonMode( GL_FRONT_AND_BACK,GL_FILL);
            lock = true;
        } else if(q_state == GLFW_RELEASE) {
            lock = false;
        }

        mat4 light_ortho;
        glm_ortho(-50,50,-50,50,-0.1,50,light_ortho);
        
        vec3 origin = {0.0f,0.0f,0.0f};

        for(int i = 0; i < scene_count; i++)
            glm_vec3_copy(defaultCam.pos,(float*)&scenes[i].point_light_vector.data[0].pos);

        static float time = 0.0f;
        time += 0.005f;
        vec3 light_dir; 
        light_dir[0] = sin(time);
        light_dir[1] = cos(time);
        light_dir[2] = 0.0f;

        glm_normalize(light_dir);
        vec3 light_pos;
        float distance = 30.0f;
        glm_vec3_scale(light_dir,distance, light_pos); 
        glm_vec3_add(origin, light_pos, light_pos);

        mat4 light_view;
        vec3 up = {0.0f, 1.0f, 0.0f};
        if(fabs(light_dir[1]) > 0.99f) up[0] = 1.0f; 
        glm_lookat(light_pos, origin, up, light_view);
 
        render_directional_shadowmap(scenes,scene_count,light_ortho,light_view,light_dir,shadowmap_shader,&depth_map);
        draw_quad(quad_shader,depth_map,0.4f);

        glViewport(0,0,windowWidth(window),windowHeight(window));
        scene_draw(scenes,scene_count,defaultProgram,defaultCam.pos,false,depth_map,light_view,light_ortho,light_dir);
        
        static u32 color_state = 0;
        if(windowKeyState(window,GLFW_KEY_1) == GLFW_PRESS)
            color_state = 0;
        else if(windowKeyState(window,GLFW_KEY_2) == GLFW_PRESS)
            color_state = 1;
        else if(windowKeyState(window,GLFW_KEY_3) == GLFW_PRESS)
            color_state = 2;
        else if(windowKeyState(window,GLFW_KEY_4) == GLFW_PRESS)
            color_state = 3;

        shaderBind(defaultProgram);
        shaderSetInt(defaultProgram,"color_state",color_state);

        shaderBind(background_shader);
        shaderSetMat4Uniform(background_shader,"view",view);
        shaderSetMat4Uniform(background_shader,"projection",proj);
        render_skybox_cube(skybox_vao,env_map,background_shader);

        windowUpdate(window);
    }

    shaderDestroy(defaultProgram);     
    windowDestroy(window);
    arena_free(&arena);

    glfwTerminate();
    return 0;
}
