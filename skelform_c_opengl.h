/*
MIT License

Copyright (c) 2026 BatteRaquette581

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**
 * @brief A one-header ANSI C no-external-dependency generic SkelForm runtime,
 *  that can be extended for other engines.
 * 
 * \file
 */


#ifndef SKELFORM_C_OPENGL_H
#define SKELFORM_C_OPENGL_H

#define STB_IMAGE_IMPLEMENTATION
#include <glad/glad.h> // NOTE TO DEVELOPERS: you can modify this line to other OpenGL headers
#include <jsmn.h>
#include <miniz/miniz.h>
#include <skelform_c/skelform_c.h>
#include <stb_image.h>
#include <stdio.h>
#include <string.h>

#ifndef SKF_OPENGL_TEXTURE_UNIT
#define SKF_OPENGL_TEXTURE_UNIT GL_TEXTURE_0
#endif


/**
 * @brief Represents a GPU texture, with an OpenGL texture ID, and a width and height.
 */
struct skf_opengl_AtlasTexture {
    /**
     * @brief An OpenGL texture ID.
     */
    GLuint texture_id;
    /**
     * @brief The atlas texture width in pixels.
     */
    GLsizei width;
    /**
     * @brief The atlas texture height in pixels.
     */
    GLsizei height;
};

/**
 * @brief A vector (dynamic array) type. Append with `skf_Vec_append`,
    and check if an element is contained using `skf_Vec_contains`.
 * @attention Don't forget to free instances with `free(vector.elements)`!
 */
struct skf_opengl_Vec_AtlasTexture {
    /**
     * @brief Pointer to the actual elements.
     */
    struct skf_opengl_AtlasTexture *elements;
    /**
     * @brief How much elements the vector can hold without re-allocating.
     */
    size_t capacity;
    /**
     * @brief How much elements the vector currently holds.
     */
    size_t size;
};

/**
 * @brief Represents a .skf file, with an armature, texture atlases, and other
 *  OpenGL necessary attributes.
 * @attention Don't forget to free instances with `skf_opengl_free_file`!
 */
struct skf_opengl_File {
    /**
     * @brief The armature contained in the file.
     */
    struct skf_Armature armature;
    /**
     * @brief The texture atlases contained in the file.
     */
    struct skf_opengl_Vec_AtlasTexture atlases;
    /**
     * @internal
     */
    GLuint ebo, vao, vbo;
    /**
     * @internal
     */
    GLsizei indices_count;
};

/**
 * @brief Represents options when constructing the mesh.
 * @attention Initially try a scale like `1 / 2000000.f`.
 */
struct skf_opengl_ConstructOptions {
    /**
     * @brief Animation playing speed.
     * @attention Unused.
     */
    float speed;
    /*
     * @brief Contains the offsets applied to the bones when constructing the mesh.
     */
    struct skf_Vec2 position;
    /*
     * @brief Contains the scaling applied to the bones when constructing the mesh.
     */
    struct skf_Vec2 scale;
};

enum skf_opengl_JSMNPrimitiveType {
    skf_opengl_JSMNPrimitiveType_BOOL,
    skf_opengl_JSMNPrimitiveType_FLOAT,
    skf_opengl_JSMNPrimitiveType_INT,
    skf_opengl_JSMNPrimitiveType_NULL
};

union skf_opengl_JSMNPrimitiveValue {
    skf_bool boolean;
    int32_t integer;
    float floating_point;
};

struct skf_opengl_JSMNPrimitive {
    union skf_opengl_JSMNPrimitiveValue data;
    enum skf_opengl_JSMNPrimitiveType type;
};


const char *skf_opengl_vertex_shader_source = " \
#version 140 compatibility\n                    \
in vec3 position;                               \
in vec2 uv;                                     \
in vec4 texture_rect;                           \
out vec2 frag_uv;                               \
flat out vec4 frag_texture_rect;                \
void main()                                     \
{                                               \
    gl_Position = vec4(position, 1.0);          \
    frag_uv = uv;                               \
    frag_texture_rect = texture_rect;           \
}                                               \
";

const char *skf_opengl_fragment_shader_source = "   \
#version 140 compatibility\n                        \
in vec2 frag_uv;                                    \
flat in vec4 frag_texture_rect;                     \
out vec4 color;                                     \
uniform sampler2D atlas0;                           \
uniform vec2 atlas0_resolution;                     \
void main()                                         \
{                                                   \
    color = texture(atlas0, (frag_texture_rect.xy   \
        + vec2(frag_uv.x, 1.0 - frag_uv.y) *        \
        frag_texture_rect.zw) / atlas0_resolution); \
}                                                   \
";


/**
 * @defgroup skf_opengl_functions Runtime Functions
 * @brief All public skelform_opengl_c functions.
 * @{
 */

char *skf_opengl_jsmn_string_value(const char *data, jsmntok_t *token)
{
    int length = token->end - token->start;
    char *value = malloc((length + 1) * sizeof(char)); // don't forget the null terminator!
    memcpy(value, data + token->start, length);
    value[length] = '\0';
    return value;
}

struct skf_opengl_JSMNPrimitive skf_opengl_jsmn_primitive_value(const char *data, jsmntok_t *token)
{
    struct skf_opengl_JSMNPrimitive primitive;
    const char first_char = data[token->start];
    if (first_char == 't' || first_char == 'f') {
        primitive.data.boolean = first_char == 't' ? skf_true : skf_false;
        primitive.type = skf_opengl_JSMNPrimitiveType_BOOL; 
    } else if (first_char == 'n') {
        primitive.type = skf_opengl_JSMNPrimitiveType_NULL;
    } else {
        const char *number_string = skf_opengl_jsmn_string_value(data, token);
        if (strchr(number_string, '.') == NULL) { // integer, no floating point dot found
            primitive.data.integer = strtol(number_string, NULL, 10);
            primitive.type = skf_opengl_JSMNPrimitiveType_INT;
        } else {
            primitive.data.floating_point = strtof(number_string, NULL);
            primitive.type = skf_opengl_JSMNPrimitiveType_FLOAT;
        }
        free((void*) number_string);
    }
    return primitive;
}

#define skf_opengl_parse_float(index, destination, error_message) {                              \
    jsmntok_t *float_token = &tokens[index];                                                     \
    if (float_token->type != JSMN_PRIMITIVE) {                                                   \
        fprintf(stderr, "skf_opengl: expected a primitive " error_message " at %s\n", skf_path); \
        return skf_false;                                                                        \
    }                                                                                            \
    struct skf_opengl_JSMNPrimitive primitive =                                                  \
        skf_opengl_jsmn_primitive_value(data, float_token);                                      \
    if (primitive.type != skf_opengl_JSMNPrimitiveType_FLOAT) {                                  \
        fprintf(stderr, "skf_opengl: expected an float " error_message " at %s\n", skf_path);    \
        return skf_false;                                                                        \
    }                                                                                            \
    destination = primitive.data.floating_point;                                                 \
}

#define skf_opengl_parse_integer(index, destination, error_message) {                            \
    jsmntok_t *integer_token = &tokens[index];                                                   \
    if (integer_token->type != JSMN_PRIMITIVE) {                                                 \
        fprintf(stderr, "skf_opengl: expected a primitive " error_message " at %s\n", skf_path); \
        free((void*) value);                                                                     \
        return skf_false;                                                                        \
    }                                                                                            \
    struct skf_opengl_JSMNPrimitive primitive =                                                  \
        skf_opengl_jsmn_primitive_value(data, integer_token);                                    \
    if (primitive.type != skf_opengl_JSMNPrimitiveType_INT) {                                    \
        fprintf(stderr, "skf_opengl: expected an integer " error_message " at %s\n", skf_path);  \
        free((void*) value);                                                                     \
        return skf_false;                                                                        \
    }                                                                                            \
    destination = primitive.data.integer;                                                        \
}

#define skf_opengl_parse_integer_unsigned(index, destination, error_message) {                   \
    jsmntok_t *integer_token = &tokens[index];                                                   \
    if (integer_token->type != JSMN_PRIMITIVE) {                                                 \
        fprintf(stderr, "skf_opengl: expected a primitive " error_message " at %s\n", skf_path); \
        free((void*) value);                                                                     \
        return skf_false;                                                                        \
    }                                                                                            \
    struct skf_opengl_JSMNPrimitive primitive =                                                  \
        skf_opengl_jsmn_primitive_value(data, integer_token);                                    \
    if (primitive.type != skf_opengl_JSMNPrimitiveType_INT) {                                    \
        fprintf(stderr, "skf_opengl: expected an integer " error_message " at %s\n", skf_path);  \
        free((void*) value);                                                                     \
        return skf_false;                                                                        \
    }                                                                                            \
    if (primitive.data.integer < 0) {                                                            \
        fprintf(stderr, "skf_opengl: negative integer " error_message " at %s\n", skf_path);     \
        free((void*) value);                                                                     \
        return skf_false;                                                                        \
    }                                                                                            \
    destination = primitive.data.integer;                                                        \
}

#define skf_opengl_parse_vec2(index, destination, error_message, is_int) {                                       \
    jsmntok_t *vec_token = &tokens[index];                                                               \
    if (vec_token->type != JSMN_OBJECT) {                                                                \
        fprintf(stderr, "skf_opengl: " error_message " vector is not object at %s\n", skf_path);         \
        free((void*) value);                                                                             \
        return skf_false;                                                                                \
    }                                                                                                    \
    struct skf_Vec2 vec;                                                                                 \
    if (!skf_opengl_parse_vec2_object(&vec, data, tokens, vec_token->size, token_i, skf_path, is_int)) { \
        free((void*) value);                                                                             \
        return skf_false;                                                                                \
    }                                                                                                    \
    destination = vec;                                                                                   \
}

#define skf_opengl_parse_string(index, destination, error_message) {                          \
    jsmntok_t *string_token = &tokens[index];                                                 \
    if (string_token->type != JSMN_STRING) {                                                  \
        fprintf(stderr, "skf_opengl: expected a string " error_message " at %s\n", skf_path); \
        free((void*) value);                                                                  \
        return skf_false;                                                                     \
    }                                                                                         \
    destination = skf_opengl_jsmn_string_value(data, string_token);                           \
}

#define skf_opengl_parse_id_array(index, destination, error_message) {                      \
    jsmntok_t *array_token = &tokens[index];                                                \
    if (array_token->type != JSMN_ARRAY) {                                                  \
        free((void*) value);                                                                \
        fprintf(stderr, "skf_opengl: " error_message " is not an array at %s\n", skf_path); \
        return skf_false;                                                                   \
    }                                                                                       \
    struct skf_Vec_uint32_t vec = {0};                                                      \
    for (int _ = 0; _ < array_token->size; _++) {                                           \
        uint32_t element;                                                                   \
        skf_opengl_parse_integer_unsigned(index, element, error_message);                   \
        skf_Vec_append(vec, element);                                                       \
    };                                                                                      \
    destination = vec;                                                                      \
}

#define skf_opengl_parse_boolean(index, destination, error_message) {                            \
    jsmntok_t *boolean_token = &tokens[index];                                                   \
    if (boolean_token->type != JSMN_PRIMITIVE) {                                                 \
        free((void*) value);                                                                     \
        fprintf(stderr, "skf_opengl: " error_message " is not a primitive at %s\n", skf_path);   \
        return skf_false;                                                                        \
    }                                                                                            \
    struct skf_opengl_JSMNPrimitive primitive =                                                  \
        skf_opengl_jsmn_primitive_value(data, boolean_token);                                    \
    if (primitive.type != skf_opengl_JSMNPrimitiveType_BOOL) {                                   \
        fprintf(stderr, "skf_opengl: expected boolean for " error_message " at %s\n", skf_path); \
        free((void*) value);                                                                     \
        return skf_false;                                                                        \
    }                                                                                            \
    destination = primitive.data.boolean;                                                        \
}

skf_bool skf_opengl_parse_vec2_object(
    struct skf_Vec2 *vec,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path,
    skf_bool is_integer_vec
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "x") == 0) {
                jsmntok_t *number_token = &tokens[*token_i = *token_i + 1];
                if (number_token->type != JSMN_PRIMITIVE) {
                    fprintf(stderr, "skf_opengl: expected a primitive vector X at %s\n", skf_path);
                    free((void*) value);
                    return skf_false;
                }
                struct skf_opengl_JSMNPrimitive primitive =
                    skf_opengl_jsmn_primitive_value(data, number_token);
                if (primitive.type == skf_opengl_JSMNPrimitiveType_INT && is_integer_vec) {
                    vec->x = (float) primitive.data.integer;
                } else if (primitive.type == skf_opengl_JSMNPrimitiveType_FLOAT) {
                    vec->x = (float) (is_integer_vec ? ((int) primitive.data.floating_point)
                        : primitive.data.floating_point);
                } else {
                    fprintf(stderr, "skf_opengl: expected a vector X number at %s\n", skf_path);
                    free((void*) value);
                    return skf_false;
                }

            } else if (strcmp(value, "y") == 0) {
                jsmntok_t *number_token = &tokens[*token_i = *token_i + 1];
                if (number_token->type != JSMN_PRIMITIVE) {
                    fprintf(stderr, "skf_opengl: expected a primitive vector Y at %s\n", skf_path);
                    free((void*) value);
                    return skf_false;
                }
                struct skf_opengl_JSMNPrimitive primitive =
                    skf_opengl_jsmn_primitive_value(data, number_token);
                if (primitive.type == skf_opengl_JSMNPrimitiveType_INT && is_integer_vec) {
                    vec->y = (float) primitive.data.integer;
                } else if (primitive.type == skf_opengl_JSMNPrimitiveType_FLOAT) {
                    vec->y = (float) (is_integer_vec ? ((int) primitive.data.floating_point)
                        : primitive.data.floating_point);
                } else {
                    fprintf(stderr, "skf_opengl: expected a vector Y number at %s\n", skf_path);
                    free((void*) value);
                    return skf_false;
                }
                
            } else {
                fprintf(stderr, "skf_opengl: unexpected vector property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_bone_vertex(
    struct skf_Vertex *vertex,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "id") == 0) {
                // ignore, skip value
                *token_i = *token_i + 1;

            } else if (strcmp(value, "pos") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, vertex->pos, "bone vertex position", skf_false);

            } else if (strcmp(value, "init_pos") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, vertex->init_pos, "bone vertex initial position", skf_false);

            } else if (strcmp(value, "uv") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, vertex->uv, "bone vertex UV", skf_false);
                
            } else {
                fprintf(stderr, "skf_opengl: unexpected bone vertex property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_bonebindvert(
    struct skf_BoneBindVert *bonebindvert,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "id") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, bonebindvert->id, "bone bind vertex ID");

            } else if (strcmp(value, "weight") == 0) {
                skf_opengl_parse_float(*token_i = *token_i + 1, bonebindvert->weight, "bone bind vertex weight");

            } else {
                fprintf(stderr, "skf_opengl: unexpected bone bind vertex property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_bonebind(
    struct skf_BoneBind *bonebind,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "bone_id") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, bonebind->bone_id, "bone bind ID");

            } else if (strcmp(value, "is_path") == 0) {
                skf_opengl_parse_boolean(*token_i = *token_i + 1, bonebind->is_path, "bone bind is_path");

            } else if (strcmp(value, "verts") == 0) {
                jsmntok_t *array_token = &tokens[*token_i = *token_i + 1];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: bone bind vertices array is not an array at %s\n", skf_path);
                    return skf_false;
                }
                struct skf_Vec_BoneBindVert bone_bind_vertices = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *vertex_token = &tokens[*token_i = *token_i + 1];
                    if (vertex_token->type != JSMN_OBJECT) {
                        free(bone_bind_vertices.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: bone bind vertex is not an object at %s\n", skf_path);
                        return skf_false;
                    }
                    struct skf_BoneBindVert bonebindvert;
                    if (!skf_opengl_parse_bonebindvert(&bonebindvert, data, tokens, vertex_token->size, token_i, skf_path)) {
                        free(bone_bind_vertices.elements);
                        free((void*) value);
                        return skf_false;
                    }
                    skf_Vec_append(bone_bind_vertices, bonebindvert);
                };
                bonebind->verts = bone_bind_vertices;
                
            } else {
                fprintf(stderr, "skf_opengl: unexpected bone vertex property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_bone(
    struct skf_Bone *bone,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "id") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, bone->id, "bone ID");

            } else if (strcmp(value, "name") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, bone->name, "bone name");

            } else if (strcmp(value, "parent_id") == 0) {
                skf_opengl_parse_integer(*token_i = *token_i + 1, bone->parent_id, "parent bone ID");

            } else if (strcmp(value, "pos") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, bone->pos, "bone position", skf_false);

            } else if (strcmp(value, "scale") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, bone->scale, "bone scale", skf_false);

            } else if (strcmp(value, "rot") == 0) {
                skf_opengl_parse_float(*token_i = *token_i + 1, bone->rot, "bone rotation");

            } else if (strcmp(value, "ik_family_id") == 0) {
                skf_opengl_parse_integer(*token_i = *token_i + 1, bone->ik_family_id, "bone IK family ID");

            } else if (strcmp(value, "ik_target_id") == 0) {
                skf_opengl_parse_integer(*token_i = *token_i + 1, bone->ik_target_id, "bone IK target ID");

            } else if (strcmp(value, "ik_bone_ids") == 0) {
                skf_opengl_parse_id_array(*token_i = *token_i + 1, bone->ik_bone_ids, "IK bone IDs");

            } else if (strcmp(value, "init_pos") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, bone->init_pos, "bone initial position", skf_false);

            } else if (strcmp(value, "init_scale") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, bone->init_scale, "bone initial scale", skf_false);

            } else if (strcmp(value, "init_rot") == 0) {
                skf_opengl_parse_float(*token_i = *token_i + 1, bone->init_rot, "bone initial rotation");

            } else if (strcmp(value, "zindex") == 0) {
                skf_opengl_parse_integer(*token_i = *token_i + 1, bone->zindex, "bone Z index");

            } else if (strcmp(value, "init_zindex") == 0) {
                skf_opengl_parse_integer(*token_i = *token_i + 1, bone->init_zindex, "bone initial Z index");
            
            } else if (strcmp(value, "tex") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, bone->tex, "bone texture");
            
            } else if (strcmp(value, "init_tex") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, bone->init_tex, "bone initial texture");
            
            } else if (strcmp(value, "ik_constraint") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, bone->ik_constraint, "bone IK constraint");
            
            } else if (strcmp(value, "init_ik_constraint") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, bone->init_ik_constraint, "bone initial IK constraint");
            
            } else if (strcmp(value, "ik_mode") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, bone->ik_mode, "bone IK mode");
            
            } else if (strcmp(value, "init_ik_mode") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, bone->init_ik_mode, "bone initial IK mode");
            
            } else if (strcmp(value, "vertices") == 0) {
                jsmntok_t *array_token = &tokens[*token_i = *token_i + 1];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: bone vertices array is not an array at %s\n", skf_path);
                    return skf_false;
                }
                struct skf_Vec_Vertex vertices = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *vertex_token = &tokens[*token_i = *token_i + 1];
                    if (vertex_token->type != JSMN_OBJECT) {
                        free(vertices.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: bone vertex is not an object at %s\n", skf_path);
                        return skf_false;
                    }
                    struct skf_Vertex vertex;
                    if (!skf_opengl_parse_bone_vertex(&vertex, data, tokens, vertex_token->size, token_i, skf_path)) {
                        free(vertices.elements);
                        free((void*) value);
                        return skf_false;
                    }
                    skf_Vec_append(vertices, vertex);
                };
                bone->vertices = vertices;
            
            } else if (strcmp(value, "indices") == 0) {
                skf_opengl_parse_id_array(*token_i = *token_i + 1, bone->indices, "bone indices");

            } else if (strcmp(value, "binds") == 0) {
                jsmntok_t *array_token = &tokens[*token_i = *token_i + 1];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: bone binds array is not an array at %s\n", skf_path);
                    return skf_false;
                }
                struct skf_Vec_BoneBind bone_binds = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *bone_bind_token = &tokens[*token_i = *token_i + 1];
                    if (bone_bind_token->type != JSMN_OBJECT) {
                        free(bone_binds.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: bone bind is not an object at %s\n", skf_path);
                        return skf_false;
                    }
                    struct skf_BoneBind bone_bind;
                    if (!skf_opengl_parse_bonebind(&bone_bind, data, tokens, bone_bind_token->size, token_i, skf_path)) {
                        free(bone_binds.elements);
                        free((void*) value);
                        return skf_false;
                    }
                    skf_Vec_append(bone_binds, bone_bind);
                };
                bone->binds = bone_binds;
            
            } else {
                fprintf(stderr, "skf_opengl: unexpected bone property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_keyframe(
    struct skf_Keyframe *keyframe,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);
        
            if (strcmp(value, "frame") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, keyframe->frame, "keyframe frame");

            } else if (strcmp(value, "bone_id") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, keyframe->bone_id, "keyframe bone ID");

            } else if (strcmp(value, "element") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, keyframe->element, "keyframe element");

            } else if (strcmp(value, "value") == 0) {
                skf_opengl_parse_float(*token_i = *token_i + 1, keyframe->value, "keyframe value");

            } else if (strcmp(value, "start_handle") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, keyframe->start_handle, "keyframe start handle", skf_false);

            } else if (strcmp(value, "end_handle") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, keyframe->end_handle, "keyframe end handle", skf_false);

            } else if (strcmp(value, "handle_preset") == 0) {
                char *handle_preset_string;
                skf_opengl_parse_string(*token_i = *token_i + 1, handle_preset_string, "keyframe handle preset");
                if (strcmp(handle_preset_string, "Linear") == 0) {
                    keyframe->handle_preset = skf_HandlePreset_Linear;
                } else if (strcmp(handle_preset_string, "SineIn") == 0) {
                    keyframe->handle_preset = skf_HandlePreset_SineIn;
                } else if (strcmp(handle_preset_string, "SineOut") == 0) {
                    keyframe->handle_preset = skf_HandlePreset_SineOut;
                } else if (strcmp(handle_preset_string, "SineInOut") == 0) {
                    keyframe->handle_preset = skf_HandlePreset_SineInOut;
                } else if (strcmp(handle_preset_string, "None") == 0) {
                    keyframe->handle_preset = skf_HandlePreset_None;
                } else if (strcmp(handle_preset_string, "Custom") == 0) {
                    keyframe->handle_preset = skf_HandlePreset_Custom;
                } else {
                    fprintf(stderr, "skf_opengl: unexpected handle preset %s at %s\n",
                        handle_preset_string, skf_path);
                    free((void*) value);
                    return skf_false;
                }

            } else {
                fprintf(stderr, "skf_opengl: unexpected animation property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }


            free((void*) value);
            break;
        }
        
        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_animation(
    struct skf_Animation *animation,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);
            
            if (strcmp(value, "id") == 0) {
                // ignore, skip value
                *token_i = *token_i + 1;

            } else if (strcmp(value, "fps") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, animation->fps, "animation FPS");

            } else if (strcmp(value, "name") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, animation->name, "animation name");

            } else if (strcmp(value, "keyframes") == 0) {
                jsmntok_t *array_token = &tokens[*token_i = *token_i + 1];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: IK root IDs is not an array at %s\n", skf_path);
                    return skf_false;
                }
                struct skf_Vec_Keyframe keyframes = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *keyframe_token = &tokens[*token_i = *token_i + 1];
                    if (keyframe_token->type != JSMN_OBJECT) {
                        free(keyframes.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: non-object keyframe at %s\n", skf_path);
                        return skf_false;
                    }
                    struct skf_Keyframe keyframe;
                    keyframe.value_str = NULL;
                    if (!skf_opengl_parse_keyframe(&keyframe, data, tokens, keyframe_token->size, token_i, skf_path)) {
                        free(keyframes.elements);
                        free((void*) value);
                        return skf_false;
                    }
                    skf_Vec_append(keyframes, keyframe);
                };
                animation->keyframes = keyframes;

            } else {
                fprintf(stderr, "skf_opengl: unexpected animation property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }


            free((void*) value);
            break;
        }
        
        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_texatlas(
    struct skf_TexAtlas *atlas,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "filename") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, atlas->filename, "atlas filename");

            } else if (strcmp(value, "size") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, atlas->size, "atlas size", skf_true);

            } else {
                fprintf(stderr, "skf_opengl: unexpected atlas property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_texture(
    struct skf_Texture *texture,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "name") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, texture->name, "style texture name");
                
            } else if (strcmp(value, "offset") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, texture->offset, "style texture atlas offset", skf_true);
                
            } else if (strcmp(value, "size") == 0) {
                skf_opengl_parse_vec2(*token_i = *token_i + 1, texture->size, "style texture atlas size", skf_true);
                
            } else if (strcmp(value, "atlas_idx") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, texture->atlas_idx, "style texture atlas index");

            } else {
                fprintf(stderr, "skf_opengl: unexpected style texture property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

skf_bool skf_opengl_parse_style(
    struct skf_Style *style,
    const char *data,
    jsmntok_t *tokens,
    const int length,
    int *token_i,
    const char *skf_path
)
{
    for (int _ = 0; _ < length; _++) {
        jsmntok_t *token = &tokens[*token_i = *token_i + 1];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "id") == 0) {
                skf_opengl_parse_integer_unsigned(*token_i = *token_i + 1, style->id, "style ID");

            } else if (strcmp(value, "name") == 0) {
                skf_opengl_parse_string(*token_i = *token_i + 1, style->name, "style name");

            } else if (strcmp(value, "active") == 0) {
                skf_opengl_parse_boolean(*token_i = *token_i + 1, style->active, "style active boolean");
                
            } else if (strcmp(value, "textures") == 0) {
                jsmntok_t *array_token = &tokens[*token_i = *token_i + 1];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: texturees is not an array at %s\n", skf_path);
                    return skf_false;
                }
                struct skf_Vec_Texture textures = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *texture_token = &tokens[*token_i = *token_i + 1];
                    if (texture_token->type != JSMN_OBJECT) {
                        free(textures.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: non-object texture at %s\n", skf_path);
                        return skf_false;
                    }
                    struct skf_Texture texture;
                    if (!skf_opengl_parse_texture(&texture, data, tokens, texture_token->size, token_i, skf_path)) {
                        free(textures.elements);
                        free((void*) value);
                        return skf_false;
                    }
                    skf_Vec_append(textures, texture);
                }
                style->textures = textures;

            } else {
                fprintf(stderr, "skf_opengl: unexpected atlas property %s at %s\n", value, skf_path);
                free((void*) value);
                return skf_false;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            char *value = skf_opengl_jsmn_string_value(data, token);
            fprintf(stderr, "skf_opengl: unexpected token %d %s at %s\n", *token_i, value, skf_path);
            free(value);
            return skf_false;
        }
        }
    }

    return skf_true;
}

#define SKF_OPENGL_ARMATURE_JSON_MAX_TOKENS 65536
skf_bool skf_opengl_load_armature(
    struct skf_Armature *armature,
    const char *data,
    const size_t size,
    const char *skf_path
)
{
    skf_bool success = skf_true;
    jsmn_parser parser;
    jsmntok_t *tokens = malloc(SKF_OPENGL_ARMATURE_JSON_MAX_TOKENS * sizeof(jsmntok_t));
    jsmn_init(&parser);
    const int r = jsmn_parse(&parser, data, size, tokens, SKF_OPENGL_ARMATURE_JSON_MAX_TOKENS);
    if (r < 0) {
        fprintf(stderr, "skf_opengl: failed to read armature.json at path %s\n", skf_path);
        success = skf_false;
        goto cleanup;
    }
    jsmntok_t *root = &tokens[0];
    if (r < 1 || root->type != JSMN_OBJECT) {
        fprintf(stderr, "skf_opengl: failed to read armature.json at path %s, expected an object\n", skf_path);
        success = skf_false;
        goto cleanup;
    }

    int i = 0;
    for (int _ = 0; _ < root->size; _++) {
        jsmntok_t *token = &tokens[++i];
        switch (token->type) {
        case JSMN_STRING: {
            const char *value = skf_opengl_jsmn_string_value(data, token);

            if (strcmp(value, "version") == 0) {
                jsmntok_t *version_token = &tokens[++i];
                if (version_token->type != JSMN_STRING) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: version is not a string at %s\n", skf_path);
                    success = skf_false;
                    goto cleanup;
                }
                char *version = skf_opengl_jsmn_string_value(data, version_token);
                if (strcmp(version, SKF_VERSION) != 0) {
                    free((void*) version);
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: version is incompatible at %s\n", skf_path);
                    success = skf_false;
                    goto cleanup;
                }
                free(version);

            } else if (strcmp(value, "ik_root_ids") == 0) {
                jsmntok_t *array_token = &tokens[++i];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: IK root IDs is not an array at %s\n", skf_path);
                    success = skf_false;
                    goto cleanup;
                }
                struct skf_Vec_uint32_t ik_root_ids = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    uint32_t ik_root_id;
                    skf_opengl_parse_integer_unsigned(++i, ik_root_id, "IK root ID")
                    skf_Vec_append(ik_root_ids, ik_root_id);
                };
                armature->ik_root_ids = ik_root_ids;

            } else if (strcmp(value, "baked_ik") == 0) {
                jsmntok_t *boolean_token = &tokens[++i];
                if (boolean_token->type != JSMN_PRIMITIVE) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: baked_ik is not a primitive at %s\n", skf_path);
                    return skf_false;
                }
                struct skf_opengl_JSMNPrimitive primitive =
                    skf_opengl_jsmn_primitive_value(data, boolean_token);
                if (primitive.type != skf_opengl_JSMNPrimitiveType_BOOL) {
                    fprintf(stderr, "skf_opengl: expected boolean for baked_ik at %s\n", skf_path);
                    free((void*) value);
                    success = skf_false;
                    goto cleanup;
                }
                armature->baked_ik = primitive.data.boolean;
            
            } else if (strcmp(value, "img_format") == 0) {
                // ignore, skip value
                i++;
            
            } else if (strcmp(value, "bones") == 0) {
                jsmntok_t *array_token = &tokens[++i];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: bones is not an array at %s\n", skf_path);
                    success = skf_false;
                    goto cleanup;
                }
                struct skf_Vec_Bone bones = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *bone_token = &tokens[++i];
                    if (bone_token->type != JSMN_OBJECT) {
                        free(bones.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: non-object bone at %s\n", skf_path);
                        success = skf_false;
                        goto cleanup;
                    }
                    struct skf_Bone bone;
                    bone.ik_bone_ids = (struct skf_Vec_uint32_t) {0};
                    bone.vertices = (struct skf_Vec_Vertex) {0};
                    bone.indices = (struct skf_Vec_uint32_t) {0};
                    bone.binds = (struct skf_Vec_BoneBind) {0};
                    bone.tint = (struct skf_Tint) {1.0f, 1.0f, 1.0f, 1.0f};
                    bone.init_tint = (struct skf_Tint) {1.0f, 1.0f, 1.0f, 1.0f};
                    bone.tex = NULL;
                    bone.init_tex = NULL;
                    bone.ik_constraint = NULL;
                    bone.init_ik_constraint = NULL;
                    bone.ik_mode = NULL;
                    bone.init_ik_mode = NULL;
                    bone.ik_target_id = -1;
                    bone.hidden = skf_false;
                    bone.init_hidden = skf_false;
                    if (!skf_opengl_parse_bone(&bone, data, tokens, bone_token->size, &i, skf_path)) {
                        free(bones.elements);
                        free((void*) value);
                        success = skf_false;
                        goto cleanup;
                    }
                    skf_Vec_append(bones, bone);
                }
                armature->bones = bones;

            } else if (strcmp(value, "animations") == 0) {
                jsmntok_t *array_token = &tokens[++i];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: animations is not an array at %s\n", skf_path);
                    success = skf_false;
                    goto cleanup;
                }
                struct skf_Vec_Animation animations = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *animation_token = &tokens[++i];
                    if (animation_token->type != JSMN_OBJECT) {
                        free(animations.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: non-object animation at %s\n", skf_path);
                        success = skf_false;
                        goto cleanup;
                    }
                    struct skf_Animation animation;
                    animation.keyframes = (struct skf_Vec_Keyframe) {0};
                    if (!skf_opengl_parse_animation(&animation, data, tokens, animation_token->size, &i, skf_path)) {
                        free(animations.elements);
                        free((void*) value);
                        success = skf_false;
                        goto cleanup;
                    }
                    skf_Vec_append(animations, animation);
                }
                armature->animations = animations;

            } else if (strcmp(value, "atlases") == 0) {
                jsmntok_t *array_token = &tokens[++i];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: atlases is not an array at %s\n", skf_path);
                    success = skf_false;
                    goto cleanup;
                }
                struct skf_Vec_TexAtlas atlases = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *atlas_token = &tokens[++i];
                    if (atlas_token->type != JSMN_OBJECT) {
                        free(atlases.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: non-object atlas at %s\n", skf_path);
                        success = skf_false;
                        goto cleanup;
                    }
                    struct skf_TexAtlas atlas;
                    if (!skf_opengl_parse_texatlas(&atlas, data, tokens, atlas_token->size, &i, skf_path)) {
                        free(atlases.elements);
                        free((void*) value);
                        success = skf_false;
                        goto cleanup;
                    }
                    skf_Vec_append(atlases, atlas);
                }
                armature->atlases = atlases;

            } else if (strcmp(value, "styles") == 0) {
                jsmntok_t *array_token = &tokens[++i];
                if (array_token->type != JSMN_ARRAY) {
                    free((void*) value);
                    fprintf(stderr, "skf_opengl: styles is not an array at %s\n", skf_path);
                    success = skf_false;
                    goto cleanup;
                }
                struct skf_Vec_Style styles = {0};
                for (int _ = 0; _ < array_token->size; _++) {
                    jsmntok_t *style_token = &tokens[++i];
                    if (style_token->type != JSMN_OBJECT) {
                        free(styles.elements);
                        free((void*) value);
                        fprintf(stderr, "skf_opengl: non-object style at %s\n", skf_path);
                        success = skf_false;
                        goto cleanup;
                    }
                    struct skf_Style style;
                    style.active = skf_true;
                    style.textures = (struct skf_Vec_Texture) {0};
                    if (!skf_opengl_parse_style(&style, data, tokens, style_token->size, &i, skf_path)) {
                        free(styles.elements);
                        free((void*) value);
                        success = skf_false;
                        goto cleanup;
                    }
                    skf_Vec_append(styles, style);
                }
                armature->styles = styles;

            } else {
                fprintf(stderr, "skf_opengl: unexpected property %s at %s\n", value, skf_path);
                free((void*) value);
                success = skf_false;
                goto cleanup;
            }

            free((void*) value);
            break;
        }

        case JSMN_ARRAY:
        case JSMN_PRIMITIVE:
        case JSMN_OBJECT:
        case JSMN_UNDEFINED: {
            fprintf(stderr, "skf_opengl: unexpected token at %s\n", skf_path);
            success = skf_false;
            goto cleanup;
        }
        }
    }

    goto cleanup;

cleanup:
    free(tokens);
    return success;
}

/**
 * @ingroup skf_opengl_functions
 * @brief Frees the given bones, they will become unusable afterwards.
 * 
 * @param bones The bones to free the memory from.
 */
void skf_opengl_free_bones(struct skf_Vec_Bone *bones)
{
    for (size_t i = 0; i < bones->size; i++) {
        struct skf_Bone *bone = &bones->elements[i];
        for (size_t j = 0; j < bone->binds.size; j++) {
            free(bone->binds.elements[j].verts.elements);
        }
        free(bone->binds.elements);
        free(bone->ik_bone_ids.elements);
        free(bone->ik_constraint);
        free(bone->ik_mode);
        free(bone->indices.elements);
        free(bone->init_ik_constraint);
        free(bone->init_ik_mode);
        free(bone->init_tex);
        free(bone->name);
        free(bone->tex);
        free(bone->vertices.elements);
    }
    free(bones->elements);
}

/**
 * @ingroup skf_opengl_functions
 * @brief Frees the resources of the file, the file will become unusable
 *  afterwards.
 * 
 * @param file The file to be freed.
 */
void skf_opengl_free_file(struct skf_opengl_File *file)
{
    // TODO: free the dynamically allocated names, atlases and vectors
    for (size_t i = 0; i < file->armature.animations.size; i++) {
        struct skf_Vec_Keyframe *kf = &file->armature.animations.elements[i].keyframes;
        for (size_t j = 0; j < kf->size; j++) {
            free(kf->elements[j].element);
            free(kf->elements[j].value_str);
        }
        free(kf->elements);
        free(file->armature.animations.elements[i].name);
    }
    free(file->armature.animations.elements);
    
    for (size_t i = 0; i < file->armature.atlases.size; i++) {
        free(file->armature.atlases.elements[i].filename);
    }
    free(file->armature.atlases.elements);
    
    skf_opengl_free_bones(&file->armature.bones);

    free(file->armature.ik_root_ids.elements);
    
    for (size_t i = 0; i < file->armature.styles.size; i++) {
        struct skf_Style *style = &file->armature.styles.elements[i];
        free(style->name);
        for (size_t j = 0; j < style->textures.size; j++) {
            free(style->textures.elements[j].name);
        }
        free(style->textures.elements);
    }
    free(file->armature.styles.elements);
    
    for (size_t i = 0; i < file->armature.textures.size; i++) {
        free(file->armature.textures.elements[i].name);
    }
    free(file->armature.textures.elements);

    for (size_t i = 0; i < file->atlases.size; i++) {
        glDeleteTextures(1, &file->atlases.elements[i].texture_id);
    }
    free(file->atlases.elements);

    glDeleteBuffers(1, &file->ebo);
    glDeleteVertexArrays(1, &file->vao);
    glDeleteBuffers(1, &file->vbo);
    for (size_t texture_i = 0; texture_i < file->atlases.size; texture_i++) {
        glDeleteTextures(1, &file->atlases.elements[texture_i].texture_id);
    }
    free(file);
}

int skf_opengl_compare_bone_z_indices(const void *a, const void *b)
{
    return ((struct skf_Bone*) a)->zindex - ((struct skf_Bone*) b)->zindex;
}

/**
 * @ingroup skf_opengl_functions
 * @brief Loads a file's armature, atlases, styles, etc.
 * 
 * @param skf_path Relative to the current working directory, if not absolute.
 * @return struct skf_opengl_File* Must be freed with `skf_opengl_free_file`.
 */
struct skf_opengl_File *skf_opengl_load(const char *skf_path)
{
    mz_zip_archive archive = {0};
    if (!mz_zip_reader_init_file(&archive, skf_path, 0)) {
        fprintf(stderr, "skf_opengl: failed to open .skf archive at %s\n", skf_path);
        return NULL;
    }

    mz_uint armature_json_file_index = UINT32_MAX;
    struct skf_opengl_File *file = malloc(sizeof(struct skf_opengl_File));
    if (!file) {
        fprintf(stderr, "skf_opengl: failed to allocate skf_opengl_File\n");
        file = NULL;
        goto cleanup;
    }
    
    const mz_uint file_count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint file_i = 0; file_i < file_count; file_i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&archive, file_i, &stat))
            continue;
        const char *filename = stat.m_filename;
        unsigned int atlas_index;

        if (strcmp(filename, "armature.json") == 0) {
            armature_json_file_index = file_i;
        } else if (
            sscanf(filename, "atlas%u.png", &atlas_index) == 1 ||
            strcmp(filename, "editor.json")   == 0             ||
            strcmp(filename, "thumbnail.png") == 0             ||
            strcmp(filename, "readme.md")     == 0
        ) {
            // skipping
        } else {
            fprintf(stderr, "skf_opengl: unknown file %s in archive at path %s\n", filename, skf_path);
            free(file);
            file = NULL;
            goto cleanup;
        }
    }

    if (armature_json_file_index == UINT32_MAX) {
        fprintf(stderr, "skf_opengl: did not find armature.json in archive at path %s\n", skf_path);
        free(file);
        file = NULL;
        goto cleanup;
    }
    
    size_t size;
    // not null-terminated!
    char *data = mz_zip_reader_extract_to_heap(&archive, armature_json_file_index, &size, 0);
    if (data == NULL) {
        fprintf(stderr, "skf_opengl: failed to read armature.json at path %s\n", skf_path);
        free(file);
        file = NULL;
        goto cleanup;
    }
    struct skf_Armature armature;
    if (skf_opengl_load_armature(&armature, data, size, skf_path)) {
        if (armature.atlases.size != 1) {
            fprintf(stderr, "skf_opengl: sorry, only 1 atlas per armature is allowed for now at %s\n", skf_path);
            skf_opengl_free_file(file);
            file = NULL;
            goto cleanup;
        }

        file->armature = armature;
        file->atlases = (struct skf_opengl_Vec_AtlasTexture) {0};
        for (size_t i = 0; i < file->armature.atlases.size; i++) {
            struct skf_TexAtlas *texatlas = &file->armature.atlases.elements[i];
            uint8_t *atlas_image_data =
            mz_zip_reader_extract_file_to_heap(&archive, texatlas->filename, &size, 0);
            if (data == NULL) {
                fprintf(stderr, "skf_opengl: failed to read atlas%lld.png at path %s\n", i, skf_path);
                skf_opengl_free_file(file);
                file = NULL;
                goto cleanup;
            }
            int x, y, n, size;
            uint8_t *atlas_pixels = stbi_load_from_memory(atlas_image_data, (int) size, &x, &y, &n,
                STBI_rgb_alpha);
            free(atlas_image_data);
            if (atlas_pixels == NULL) {
                fprintf(stderr, "skf_opengl: failed to load atlas%lld.png at path %s\n", i, skf_path);
                skf_opengl_free_file(file);
                file = NULL;
                goto cleanup;
            }
            struct skf_opengl_AtlasTexture atlas_texture;
            atlas_texture.width = (GLsizei) texatlas->size.x;
            atlas_texture.height = (GLsizei) texatlas->size.y;

            glGenTextures(1, &atlas_texture.texture_id);
            glBindTexture(GL_TEXTURE_2D, atlas_texture.texture_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_texture.width, atlas_texture.height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, atlas_pixels);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0); // unbind
            stbi_image_free(atlas_pixels);
            skf_Vec_append(file->atlases, atlas_texture);
        }
    } else {
        free(file);
        file = NULL;
    }
    free(data);
    
    glGenBuffers(1, &file->ebo);
    glGenVertexArrays(1, &file->vao);
    glGenBuffers(1, &file->vbo);

cleanup:
    mz_zip_reader_end(&archive);
    return file;
}

/**
 * @ingroup skf_opengl_functions
 * @brief Constructs the bones, necessary before drawing them.
 * @attention The returned bones must be freed afterwards using `skf_opengl_free_bones`.
 * 
 * @param armature Pointer to the file's armature.
 * @param options Construct options.
 * @param resolution Window resolution.
 * @return struct skf_Vec_Bone The vector of bones that will be used to be drawn.
 */
struct skf_Vec_Bone skf_opengl_construct(
    struct skf_Armature *armature,
    const struct skf_opengl_ConstructOptions *options,
    const struct skf_Vec2 resolution
)
{
    const struct skf_Vec2 scale = skf_vec2_mul(
        options->scale,
        (struct skf_Vec2) {resolution.y, resolution.x}
    );
    struct skf_Vec_Bone bones = skf_construct(armature);
    for (size_t bone_i = 0; bone_i < bones.size; bone_i++) {
        struct skf_Bone *bone = &bones.elements[bone_i];
        bone->scale = skf_vec2_mul(bone->scale, scale);
        bone->pos = skf_vec2_add(skf_vec2_mul(bone->pos, scale), options->position);
        skf_check_bone_flip(bone, options->scale);
        for (size_t vertex_i = 0; vertex_i < bone->vertices.size; vertex_i++) {
            struct skf_Vertex *vertex = &bone->vertices.elements[vertex_i];
            vertex->pos = skf_vec2_add(skf_vec2_mul(vertex->pos, scale), options->position);
        }
    }
    qsort(bones.elements, bones.size, sizeof(*bones.elements), skf_opengl_compare_bone_z_indices);
    return bones;
}

#ifndef SKF_OPENGL_INFOLOG_SIZE
#define SKF_OPENGL_INFOLOG_SIZE 512
#endif
/**
 * @ingroup skf_opengl_functions
 * @brief Compiles the OpenGL shader that will need to be passed when using `skf_opengl_draw`.
 * @attention The OpenGL info log size can be modified by defining `SKF_OPENGL_INFOLOG_SIZE`
 *  before including this header.
 * 
 * @return GLuint The shader program ID.
 */
GLuint skf_opengl_create_shader()
{
    int success;
    char infolog[SKF_OPENGL_INFOLOG_SIZE];

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &skf_opengl_vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, SKF_OPENGL_INFOLOG_SIZE, NULL, infolog);
        fprintf(stderr, "failed to compile the vertex shader: %s\n", infolog);
        return UINT32_MAX;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &skf_opengl_fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(vertex_shader);
        glGetShaderInfoLog(fragment_shader, SKF_OPENGL_INFOLOG_SIZE, NULL, infolog);
        fprintf(stderr, "failed to compile the fragment shader: %s\n", infolog);
        return UINT32_MAX;
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glBindAttribLocation(shader_program, 0, "position");
    glBindAttribLocation(shader_program, 1, "uv");
    glBindAttribLocation(shader_program, 2, "texture_rect");
    glLinkProgram(shader_program);
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, SKF_OPENGL_INFOLOG_SIZE, NULL, infolog);
        fprintf(stderr, "failed to link the shader program: %s\n", infolog);
        return UINT32_MAX;
    }

    return shader_program;
}

/**
 * @ingroup skf_opengl_functions
 * @brief Generates the mesh to be later drawn using `skf_opengl_draw`.
 * @attention The bones must be constructed first, after a call to `skf_construct`.
 * 
 * @param file The file that contains the armature.
 * @param constructed_bones A vector of constructed bones, made by `skf_construct`.
 * @param options The options for constructing the mesh.
 * @param resolution The window resolution.
 */
void skf_opengl_create_mesh(
    struct skf_opengl_File *file,
    const struct skf_Vec_Bone *constructed_bones,
    const struct skf_opengl_ConstructOptions *options,
    const struct skf_Vec2 resolution
)
{
    const struct skf_Vec2 scale = skf_vec2_mul(
        options->scale,
        (struct skf_Vec2) {resolution.y, resolution.x}
    );
    struct skf_Vec_float vertex_data = {0};
    struct skf_Vec_uint32_t indices = {0};
    struct skf_Vec_float texture_rects = {0};

    GLuint texture_i = 0;
    for (size_t style_i = 0; style_i < file->armature.styles.size; style_i++) {
        const struct skf_Style *style = &file->armature.styles.elements[style_i];
        for (size_t i = 0; i < style->textures.size; i++) {
            const struct skf_Texture *texture = &style->textures.elements[i];
            skf_Vec_append(texture_rects, texture->offset.x);
            skf_Vec_append(texture_rects, texture->offset.y);
            skf_Vec_append(texture_rects, texture->size.x);
            skf_Vec_append(texture_rects, texture->size.y);
            texture_i++;
        }
    }
    
    uint32_t index_offset = 0;
    for (size_t bone_i = 0; bone_i < constructed_bones->size; bone_i++) {
        const struct skf_Bone *bone = &constructed_bones->elements[bone_i];
        if (bone->tex == NULL)
            continue;
        const struct skf_Texture *texture = skf_get_bone_texture(bone->tex, &file->armature.styles);
        if (texture == NULL)
            continue;
        const struct skf_Vec2 bone_scale = skf_vec2_div(bone->scale, scale);

        if (bone->vertices.size != 0) {
            // non-quad mesh, vertices and indices form triangles
            for (size_t vertex_i = 0; vertex_i < bone->vertices.size; vertex_i++) {
                struct skf_Vertex *vertex = &bone->vertices.elements[vertex_i];
                skf_Vec_append(vertex_data, (vertex->pos.x - bone->pos.x) * bone_scale.x + bone->pos.x);
                //skf_Vec_append(vertex_data, vertex->pos.x);
                skf_Vec_append(vertex_data, (vertex->pos.y - bone->pos.y) * bone_scale.y + bone->pos.y);
                //skf_Vec_append(vertex_data, vertex->pos.y);
                skf_Vec_append(vertex_data, 0.0f);
                skf_Vec_append(vertex_data, vertex->uv.x);
                skf_Vec_append(vertex_data, vertex->uv.y);
                skf_Vec_append(vertex_data, texture->offset.x);
                skf_Vec_append(vertex_data, texture->offset.y);
                skf_Vec_append(vertex_data, texture->size.x);
                skf_Vec_append(vertex_data, texture->size.y);
            }
            for (size_t index_i = 0; index_i < bone->indices.size; index_i++)
                skf_Vec_append(indices, bone->indices.elements[index_i] + index_offset);
            index_offset += (uint32_t) bone->vertices.size;

        } else {
            // quad meshes
            const struct skf_Vec2 right = skf_vec2_mul(skf_vec2_rotate(
                (struct skf_Vec2) {texture->size.x * 0.5f, 0.0f},
                bone->rot
            ), bone->scale);
            const struct skf_Vec2 up = skf_vec2_mul(skf_vec2_rotate(
                (struct skf_Vec2) {0.0f, texture->size.y * 0.5f},
                bone->rot
            ), bone->scale);
            skf_Vec_append(vertex_data, bone->pos.x - right.x - up.x);
            skf_Vec_append(vertex_data, bone->pos.y - right.y - up.y);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, texture->offset.x);
            skf_Vec_append(vertex_data, texture->offset.y);
            skf_Vec_append(vertex_data, texture->size.x);
            skf_Vec_append(vertex_data, texture->size.y);
            skf_Vec_append(vertex_data, bone->pos.x - right.x + up.x);
            skf_Vec_append(vertex_data, bone->pos.y - right.y + up.y);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, 1.0f);
            skf_Vec_append(vertex_data, texture->offset.x);
            skf_Vec_append(vertex_data, texture->offset.y);
            skf_Vec_append(vertex_data, texture->size.x);
            skf_Vec_append(vertex_data, texture->size.y);
            skf_Vec_append(vertex_data, bone->pos.x + right.x + up.x);
            skf_Vec_append(vertex_data, bone->pos.y + right.y + up.y);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, 1.0f);
            skf_Vec_append(vertex_data, 1.0f);
            skf_Vec_append(vertex_data, texture->offset.x);
            skf_Vec_append(vertex_data, texture->offset.y);
            skf_Vec_append(vertex_data, texture->size.x);
            skf_Vec_append(vertex_data, texture->size.y);
            skf_Vec_append(vertex_data, bone->pos.x + right.x - up.x);
            skf_Vec_append(vertex_data, bone->pos.y + right.y - up.y);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, 1.0f);
            skf_Vec_append(vertex_data, 0.0f);
            skf_Vec_append(vertex_data, texture->offset.x);
            skf_Vec_append(vertex_data, texture->offset.y);
            skf_Vec_append(vertex_data, texture->size.x);
            skf_Vec_append(vertex_data, texture->size.y);
            skf_Vec_append(indices, index_offset + 0);
            skf_Vec_append(indices, index_offset + 1);
            skf_Vec_append(indices, index_offset + 3);
            skf_Vec_append(indices, index_offset + 1);
            skf_Vec_append(indices, index_offset + 2);
            skf_Vec_append(indices, index_offset + 3);
            index_offset += 4;
        }
    }

    file->indices_count = (GLsizei) indices.size;
    glBindVertexArray(file->vao);
    glBindBuffer(GL_ARRAY_BUFFER, file->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (sizeof(*vertex_data.elements) * vertex_data.size),
        vertex_data.elements, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, file->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (sizeof(*indices.elements) * indices.size),
        indices.elements, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 9, (void*) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 9, (void*) (sizeof(GLfloat) * 3));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 9, (void*) (sizeof(GLfloat) * 5));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    free(vertex_data.elements);
    free(indices.elements);
    free(texture_rects.elements);
}

/**
 * @ingroup skf_opengl_functions
 * @brief Animates the given bones, with a vector of animations, and their respective
 *  frames and blending frames, options, and window resolution.
 * @attention The bones are constructed and the mesh is regenerated: no need to call
 *  `skf_opengl_construct` nor `skf_opengl_create_mesh` afterwards.
 * 
 * @param file File that contains the armature to be animated.
 * @param bones Contains the constructed bones, to be overridden.
 * @param animations The current animations being played.
 * @param frames The frames for the respective animations.
 * @param blend_frames The blending frames for the respective animations.
 * @param options Options for constructing the new animated bones and mesh.
 * @param resolution The window resolution in pixels.
 */
void skf_opengl_animate(
    struct skf_opengl_File *file,
    struct skf_Vec_Bone *bones,
    const struct skf_Vec_Animation *anims,
    const struct skf_Vec_uint32_t *frames,
    const struct skf_Vec_uint32_t *blend_frames,
    struct skf_opengl_ConstructOptions *options,
    const struct skf_Vec2 resolution
)
{
    skf_animate(&file->armature.bones, anims, frames, blend_frames);
    free(bones->elements);
    *bones = skf_opengl_construct(&file->armature, options, resolution);
    skf_opengl_create_mesh(file, bones, options, resolution);
}

/**
 * @ingroup skf_opengl_functions
 * @brief Draws the armature from the file.
 * @attention The bones must be constructed initially once after loading the file,
 *  using `skf_create_mesh`.
 * 
 * @param file File that contains the armature to be drawn.
 * @param shader_program The value returned by `skf_opengl_create_shader`.
 */
void skf_opengl_draw(const struct skf_opengl_File *file, const GLuint shader_program)
{
    glUseProgram(shader_program);
    glUniform1i(glGetUniformLocation(shader_program, "atlas0"), 0);
    glUniform2f(glGetUniformLocation(shader_program, "atlas0_resolution"),
        (float) file->atlases.elements[0].width, (float) file->atlases.elements[0].height);
    glActiveTexture(GL_TEXTURE0);
    GLint previous_binding;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_binding);
    glBindTexture(GL_TEXTURE_2D, file->atlases.elements[0].texture_id);
    glBindVertexArray(file->vao);
    glDrawElements(GL_TRIANGLES, file->indices_count, GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, (GLuint) previous_binding); // restore previous binding
    glBindVertexArray(0);
}

/**
 * @ingroup skf_opengl_functions
 * @brief Helper function to enable transparency.
 */
void skf_opengl_enable_transparency()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

#ifndef _WIN32
#include <sys/time.h>
/**
 * @ingroup skf_opengl_functions
 * @brief Returns the Unix epoch (time since 1/1/1970) in milliseconds.
 * 
 * @return uint64_t The Unix epoch (time since 1/1/1970) in milliseconds.
 */
uint64_t skf_opengl_get_epoch_ms()
{
    struct timeval timeval;
    gettimeofday(&timeval, NULL);
    return (uint64_t) timeval.tv_sec * 1000 + timeval.tv_usec / 1000;
}
#else
#include <windows.h>
/**
 * @ingroup skf_opengl_functions
 * @brief Returns the Unix epoch (time since 1/1/1970) in milliseconds.
 * @note For consistency on Windows, a Unix epoch is given and converted from Windows time.
 * 
 * @return uint64_t The Unix epoch (time since 1/1/1970) in milliseconds.
 */
uint64_t skf_opengl_get_epoch_ms()
{
    FILETIME filetime;
    GetSystemTimeAsFileTime(&filetime);
    uint64_t windows_epoch_100ns = ((uint64_t) filetime.dwHighDateTime << 32)
        | filetime.dwLowDateTime;
    // convert to ms, adjust windows epoch (1/1/1601) to unix epoch (1/1/1970)
    // credits to https://stackoverflow.com/a/46024468
    return (windows_epoch_100ns - 0x019DB1DED53E8000) / 10000;
}
#endif

/** @} */

#endif