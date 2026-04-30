/**
 * Created by: artDev, DuyKhanhTran
 * Copyright (c) 2025 artDev, SerpentSpirale, PojavLauncherTeam, Digital Genesis LLC.
#include "es3_functions.h"
 * For use under LGPL-3.0
 */
#include <stdio.h>
#include <dlfcn.h>

#include <stdbool.h>
#include "GL/gl.h"
#include <GLES3/gl3.h>
#include "string_utils.h"
#include <stdlib.h>
#include <string.h>
extern es3_functions_t es3_functions;
#include "proc.h"
#include "egl.h"
#include "glformats.h"
#include "main.h"
#include "swizzle.h"
#include "libraryinternal.h"
#include "env.h"

void glClearDepth(GLdouble depth) {
    if(!current_context) return;
    es3_functions.glClearDepthf((GLfloat) depth);
}

void *glMapBuffer(GLenum target, GLenum access) {
    if(!current_context) return NULL;

    GLenum access_range;
    GLint length;

    switch (target) {
        // GL 4.2
        case GL_ATOMIC_COUNTER_BUFFER:
        // GL 4.3
        case GL_DISPATCH_INDIRECT_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
        // GL 4.4
        case GL_QUERY_BUFFER:
            printf("ERROR: glMapBuffer unsupported target=0x%x\n", target);
            break; // not supported for now
	    case GL_DRAW_INDIRECT_BUFFER:
        case GL_TEXTURE_BUFFER:
            printf("ERROR: glMapBuffer unimplemented target=0x%x\n", target);
            break;
    }

    switch (access) {
        case GL_READ_ONLY:
            access_range = GL_MAP_READ_BIT;
            break;

        case GL_WRITE_ONLY:
            access_range = GL_MAP_WRITE_BIT;
            break;

        case GL_READ_WRITE:
            access_range = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
            break;
    }

    es3_functions.glGetBufferParameteriv(target, GL_BUFFER_SIZE, &length);
    return es3_functions.glMapBufferRange(target, 0, length, access_range);
}

INTERNAL int isProxyTexture(GLenum target) {
    switch (target) {
        case GL_PROXY_TEXTURE_1D:
        case GL_PROXY_TEXTURE_2D:
        case GL_PROXY_TEXTURE_3D:
        case GL_PROXY_TEXTURE_RECTANGLE_ARB:
            return 1;
    }
    return 0;
}

INTERNAL GLenum get_textarget_query_param(GLenum target) {
    switch (target) {
        case GL_TEXTURE_2D:
            return GL_TEXTURE_BINDING_2D;
        case GL_TEXTURE_2D_MULTISAMPLE:
            return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
        case GL_TEXTURE_3D:
            return GL_TEXTURE_BINDING_3D;
        case GL_TEXTURE_2D_ARRAY:
            return GL_TEXTURE_BINDING_2D_ARRAY;
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP:
            return GL_TEXTURE_BINDING_CUBE_MAP;
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
        case GL_TEXTURE_BUFFER:
            return GL_TEXTURE_BUFFER_BINDING;
        default:
            return 0;
    }
}

static int inline nlevel(int size, int level) {
    if(size) {
        size>>=level;
        if(!size) size=1;
    }
    return size;
}

static bool trigger_texlevelparameter = false;

static bool check_texlevelparameter() {
    if(current_context->es31) return true;
    if(trigger_texlevelparameter) return false;
    printf("glGetTexLevelParameter* functions are not supported below OpenGL ES 3.1\n");
    trigger_texlevelparameter = true;
    return false;
}

static void proxy_getlevelparameter(GLenum target, GLint level, GLenum pname, GLint *params) {
    switch (pname) {
        case GL_TEXTURE_WIDTH:
            (*params) = nlevel(current_context->proxy_width, level);
            break;
        case GL_TEXTURE_HEIGHT:
            (*params) = nlevel(current_context->proxy_height, level);
            break;
        case GL_TEXTURE_INTERNAL_FORMAT:
            (*params) = current_context->proxy_intformat;
            break;
    }
}

void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params) {
    if(!current_context) return;
    if(isProxyTexture(target)) {
        GLint param = 0;
        proxy_getlevelparameter(target, level, pname, &param);
        *params = (GLfloat) param;
        return;
    }
    if(!check_texlevelparameter()) return;
    es3_functions.glGetTexLevelParameterfv(target, level, pname, params);
}

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params) {
    if(!current_context) return;
    if (isProxyTexture(target)) {
        proxy_getlevelparameter(target, level, pname, params);
        return;
    }
    if(!check_texlevelparameter()) return;
    es3_functions.glGetTexLevelParameteriv(target, level, pname, params);

}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data) {
    if(!current_context) return;
    if (isProxyTexture(target)) {
        current_context->proxy_width = ((width<<level)>current_context->maxTextureSize)?0:width;
        current_context->proxy_height = ((height<<level)>current_context->maxTextureSize)?0:height;
        current_context->proxy_intformat = internalformat;
    } else {
        if(data != NULL) swizzle_process_upload(target, &format, &type);
        pick_internalformat(&internalformat, &type, &format, &data);
        es3_functions.glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
    }
}

INTERNAL bool filter_params_integer(GLenum target, GLenum pname, GLint param) {
    return true;
}
INTERNAL bool filter_params_float(GLenum target, GLenum pname, GLfloat param) {
    if(pname == GL_TEXTURE_LOD_BIAS) {
        if(param != 0.0f) {
            static bool lodbias_trigger = false;
            if(!lodbias_trigger) {
                printf("LTW: setting GL_TEXTURE_LOD_BIAS to nondefault value not supported\n");
            }
        }
        return false;
    }
    return true;
}
void glTexParameterf( 	GLenum target,
                         GLenum pname,
                         GLfloat param) {
    if(!current_context) return;
    if(!filter_params_integer(target, pname, (GLint) param)) return;
    if(!filter_params_float(target, pname, param)) return;
    es3_functions.glTexParameterf(target, pname, param);
}
void glTexParameteri( 	GLenum target,
                         GLenum pname,
                         GLint param) {
    if(!current_context) return;
    if(!filter_params_integer(target, pname, param)) return;
    if(!filter_params_float(target, pname, (GLfloat)param)) return;;
    swizzle_process_swizzle_param(target, pname, &param)
    es3_functions.glTexParameteri(target, pname, param);
}

void glTexParameterfv( 	GLenum target,
                          GLenum pname,
                          const GLfloat * params) {
    if(!current_context) return;
    if(!filter_params_integer(target, pname, (GLint)*params)) return;
    if(!filter_params_float(target, pname, *params)) return;
    es3_functions.glTexParameterfv(target, pname, params);
}
void glTexParameteriv( 	GLenum target,
                          GLenum pname,
                          const GLint * params) {
    if(!current_context) return;
    if(!filter_params_integer(target, pname, *params)) return;
    if(!filter_params_float(target, pname, (GLfloat)*params)) return;
    swizzle_process_swizzle_param(target, pname, params);
    es3_functions.glTexParameteriv(target, pname, params);
}
static bool trigger_gltexparameteri = false;
void glTexParameterIiv( 	GLenum target,
                           GLenum pname,
                           const GLint * params) {
    if(!current_context) return;
    if(pname != GL_TEXTURE_SWIZZLE_RGBA) {
        if(!trigger_gltexparameteri) {
            printf("LTW: glTexParameterIiv for parameters other than GL_TEXTURE_SWIZZLE_RGBA is not supported\n");
            trigger_gltexparameteri = true;
        }
        return;
    }
    swizzle_process_swizzle_param(target, pname, params);
}

void glTexParameterIuiv( 	GLenum target,
                            GLenum pname,
                            const GLuint * params) {
    if(!current_context) return;
    if(pname != GL_TEXTURE_SWIZZLE_RGBA) {
        if(!trigger_gltexparameteri) {
            printf("LTW: glTexParameterIuiv for parameters other than GL_TEXTURE_SWIZZLE_RGBA is not supported\n");
            trigger_gltexparameteri = true;
        }
        return;
    }
    swizzle_process_swizzle_param(target, pname, params);
}

void glRenderbufferStorage(	GLenum target,
                               GLenum internalformat,
                               GLsizei width,
                               GLsizei height) {
    if(!current_context) return;
    if(internalformat == GL_DEPTH_COMPONENT) internalformat = GL_DEPTH_COMPONENT16;
    es3_functions.glRenderbufferStorage(target, internalformat, width, height);
}

static bool never_flush_buffers;

void glBufferStorage(GLenum target,
                     GLsizeiptr size,
                     const void * data,
                     GLbitfield flags) {
    if(!current_context || !current_context->buffer_storage) return;
    // Enable coherence to make sure the buffers are synced without flushing.
    if((flags & GL_MAP_PERSISTENT_BIT) && never_flush_buffers) flags |= GL_MAP_COHERENT_BIT;
    es3_functions.glBufferStorageEXT(target, size, data, flags);
}

void *glMapBufferRange( 	GLenum target,
                           GLintptr offset,
                           GLsizeiptr length,
                           GLbitfield access) {
    if(never_flush_buffers) access &= ~GL_MAP_FLUSH_EXPLICIT_BIT;
    es3_functions.glGetError();
    void* map = es3_functions.glMapBufferRange(target, offset, length, access);
    printf("glMapBufferRange=%p-%p error=%x\n", map, ((char*)map)+length, es3_functions.glGetError());
    return map;
}

void glFlushMappedBufferRange( 	GLenum target,
                                  GLintptr offset,
                                  GLsizeiptr length) {
    if(!never_flush_buffers) es3_functions.glFlushMappedBufferRange(target, offset, length);
}

const GLubyte* glGetStringi(GLenum name, GLuint index) {
    if(!current_context || name != GL_EXTENSIONS) return NULL;
    if(index < current_context->nextensions_es) {
        return es3_functions.glGetStringi(name, index);
    }else {
        int extra_index = index - current_context->nextensions_es;
        if(extra_index >= current_context->nextras) return NULL;
        return (const GLubyte*)current_context->extra_extensions_array[extra_index];
    }
}

const GLubyte* glGetString(GLenum name) {
    if(!current_context) return NULL;
    switch(name) {
        case GL_VERSION:
            return (const GLubyte*)"3.0 OpenLTW";
        case GL_SHADING_LANGUAGE_VERSION:
            return (const GLubyte*)"4.60 LTW";
        case GL_VENDOR:
            return (const GLubyte*)"PojavLauncherTeam & QuestCraft Developers";
        case GL_EXTENSIONS:
            return (const GLubyte*)"GL_EXT_debug_marker GL_ARM_mali_shader_binary GL_EXT_disjoint_timer_query GL_EXT_texture_filter_anisotropic GL_EXT_texture_format_BGRA8888 GL_EXT_texture_type_2_10_10_10_REV GL_OES_compressed_ETC1_RGB8_texture GL_OES_depth_texture GL_OES_depth_texture_cube_map GL_OES_depth24 GL_OES_EGL_image GL_OES_EGL_image_external GL_OES_EGL_image_external_essl3 GL_OES_EGL_sync GL_OES_element_index_uint GL_OES_fbo_render_mipmap GL_OES_get_program_binary GL_OES_mapbuffer GL_OES_packed_depth_stencil GL_OES_rgb8_rgba8 GL_OES_standard_derivatives GL_OES_surfaceless_context GL_OES_texture_3D GL_OES_texture_compression_astc GL_OES_texture_float GL_OES_texture_float_linear GL_OES_texture_half_float GL_OES_texture_half_float_linear GL_OES_texture_npot GL_OES_vertex_array_object GL_EXT_blend_minmax GL_EXT_read_format_bgra GL_EXT_occlusion_query_boolean GL_EXT_debug_label GL_EXT_texture_rg GL_EXT_discard_framebuffer GL_EXT_multisampled_render_to_texture GL_EXT_multisampled_render_to_texture2 GL_EXT_robustness GL_EXT_texture_storage GL_EXT_sRGB GL_EXT_texture_sRGB_decode GL_EXT_texture_sRGB_R8 GL_EXT_texture_sRGB_RG8 GL_EXT_sRGB_write_control GL_EXT_texture_compression_astc_decode_mode GL_EXT_texture_compression_astc_decode_mode_rgb9e5 GL_KHR_debug GL_KHR_texture_compression_astc_ldr GL_KHR_texture_compression_astc_hdr GL_KHR_no_error GL_KHR_parallel_shader_compile GL_OES_EGL_image_external_essl1 GL_EXT_external_buffer GL_EXT_buffer_storage GL_EXT_shader_pixel_local_storage GL_ARM_shader_framebuffer_fetch GL_ARM_shader_framebuffer_fetch_depth_stencil GL_ARM_mali_program_binary GL_EXT_copy_image GL_EXT_color_buffer_half_float GL_EXT_color_buffer_float GL_EXT_YUV_target GL_OES_texture_stencil8 GL_OES_shader_multisample_interpolation GL_OES_shader_image_atomic GL_OES_sample_variables GL_OES_sample_shading GL_OES_gpu_shader5 GL_OES_shader_io_blocks GL_OES_texture_buffer GL_OES_texture_cube_map_array GL_OES_texture_view GL_OES_geometry_shader GL_OES_geometry_point_size GL_OES_tessellation_shader GL_OES_tessellation_point_size GL_EXT_texture_border_clamp GL_EXT_texture_buffer GL_EXT_texture_cube_map_array GL_EXT_texture_view GL_EXT_geometry_shader GL_EXT_geometry_point_size GL_EXT_tessellation_shader GL_EXT_tessellation_point_size GL_EXT_gpu_shader5 GL_EXT_shader_io_blocks GL_EXT_shader_implicit_conversions GL_EXT_shader_integer_mix GL_EXT_shader_non_constant_global_initializers GL_EXT_protected_textures GL_KHR_blend_equation_advanced GL_KHR_blend_equation_advanced_coherent GL_OES_texture_storage_multisample_2d_array GL_OES_viewport_array GL_EXT_separate_shader_objects GL_OVR_multiview GL_OVR_multiview2 GL_OVR_multiview_multisampled_render_to_texture GL_KHR_robustness GL_KHR_robust_buffer_access_behavior GL_EXT_draw_buffers_indexed";
            return (const GLubyte*)"GL_EXT_debug_marker GL_ARM_mali_shader_binary GL_EXT_disjoint_timer_query GL_EXT_texture_filter_anisotropic GL_EXT_texture_format_BGRA8888 GL_EXT_texture_type_2_10_10_10_REV GL_OES_compressed_ETC1_RGB8_texture GL_OES_depth_texture GL_OES_depth_texture_cube_map GL_OES_depth24 GL_OES_EGL_image GL_OES_EGL_image_external GL_OES_EGL_image_external_essl3 GL_OES_EGL_sync GL_OES_element_index_uint GL_OES_fbo_render_mipmap GL_OES_get_program_binary GL_OES_mapbuffer GL_OES_packed_depth_stencil GL_OES_rgb8_rgba8 GL_OES_standard_derivatives GL_OES_surfaceless_context GL_OES_texture_3D GL_OES_texture_compression_astc GL_OES_texture_float GL_OES_texture_float_linear GL_OES_texture_half_float GL_OES_texture_half_float_linear GL_OES_texture_npot GL_OES_vertex_array_object GL_EXT_blend_minmax GL_EXT_read_format_bgra GL_EXT_occlusion_query_boolean GL_EXT_debug_label GL_EXT_texture_rg GL_EXT_discard_framebuffer GL_EXT_multisampled_render_to_texture GL_EXT_multisampled_render_to_texture2 GL_EXT_robustness GL_EXT_texture_storage GL_EXT_sRGB GL_EXT_texture_sRGB_decode GL_EXT_texture_sRGB_R8 GL_EXT_texture_sRGB_RG8 GL_EXT_sRGB_write_control GL_EXT_texture_compression_astc_decode_mode GL_EXT_texture_compression_astc_decode_mode_rgb9e5 GL_KHR_debug GL_KHR_texture_compression_astc_ldr GL_KHR_texture_compression_astc_hdr GL_KHR_no_error GL_KHR_parallel_shader_compile GL_OES_EGL_image_external_essl1 GL_EXT_external_buffer GL_EXT_buffer_storage GL_EXT_shader_pixel_local_storage GL_ARM_shader_framebuffer_fetch GL_ARM_shader_framebuffer_fetch_depth_stencil GL_ARM_mali_program_binary GL_EXT_copy_image GL_EXT_color_buffer_half_float GL_EXT_color_buffer_float GL_EXT_YUV_target GL_OES_texture_stencil8 GL_OES_shader_multisample_interpolation GL_OES_shader_image_atomic GL_OES_sample_variables GL_OES_sample_shading GL_OES_gpu_shader5 GL_OES_shader_io_blocks GL_OES_texture_buffer GL_OES_texture_cube_map_array GL_OES_texture_view GL_OES_geometry_shader GL_OES_geometry_point_size GL_OES_tessellation_shader GL_OES_tessellation_point_size GL_EXT_texture_border_clamp GL_EXT_texture_buffer GL_EXT_texture_cube_map_array GL_EXT_texture_view GL_EXT_geometry_shader GL_EXT_geometry_point_size GL_EXT_tessellation_shader GL_EXT_tessellation_point_size GL_EXT_gpu_shader5 GL_EXT_shader_io_blocks GL_EXT_shader_implicit_conversions GL_EXT_shader_integer_mix GL_EXT_shader_non_constant_global_initializers GL_EXT_protected_textures GL_KHR_blend_equation_advanced GL_KHR_blend_equation_advanced_coherent GL_OES_texture_storage_multisample_2d_array GL_OES_viewport_array GL_EXT_separate_shader_objects GL_OVR_multiview GL_OVR_multiview2 GL_OVR_multiview_multisampled_render_to_texture GL_KHR_robustness GL_KHR_robust_buffer_access_behavior GL_EXT_draw_buffers_indexed";
        default:
            return es3_functions.glGetString(name);
    }
}

static bool debug = false;

void glEnable(GLenum cap) {
    if(!current_context) return;
    if(cap == GL_DEBUG_OUTPUT && !debug) return;
    es3_functions.glEnable(cap);
}

INTERNAL int get_buffer_index(GLenum buffer) {
    switch (buffer) {
        case GL_ARRAY_BUFFER: return 0;
        case GL_COPY_READ_BUFFER: return 1;
        case GL_COPY_WRITE_BUFFER: return 2;
        case GL_PIXEL_PACK_BUFFER: return 3;
        case GL_PIXEL_UNPACK_BUFFER: return 4;
        case GL_TRANSFORM_FEEDBACK_BUFFER: return 5;
        case GL_UNIFORM_BUFFER: return 6;
        case GL_SHADER_STORAGE_BUFFER: return 7;
        case GL_DRAW_INDIRECT_BUFFER: return 8;
        default: return -1;
    }
}

INTERNAL int get_base_buffer_index(GLenum buffer) {
    switch (buffer) {
        case GL_ATOMIC_COUNTER_BUFFER: return 0;
        case GL_SHADER_STORAGE_BUFFER: return 1;
        case GL_TRANSFORM_FEEDBACK_BUFFER: return 2;
        case GL_UNIFORM_BUFFER: return 3;
        default: return -1;
    }
}

INTERNAL GLenum get_base_buffer_enum(int buffer_index) {
    switch (buffer_index) {
        case 0: return GL_ATOMIC_COUNTER_BUFFER;
        case 1: return GL_SHADER_STORAGE_BUFFER;
        case 2: return GL_TRANSFORM_FEEDBACK_BUFFER;
        case 3: return GL_UNIFORM_BUFFER;
        default: return -1;
    }
}

void glBindBuffer(GLenum buffer, GLuint name) {
    if(!current_context) return;
    es3_functions.glBindBuffer(buffer, name);
    int buffer_index = get_buffer_index(buffer);
    if(buffer_index == -1) return;
    current_context->bound_buffers[buffer_index] = name;
}

static basebuffer_binding_t* set_basebuffer(GLenum target, GLuint index, GLuint buffer) {
    int buffer_mapindex = get_base_buffer_index(target);
    if(buffer_mapindex == -1) return NULL;
    if(!buffer) {
        basebuffer_binding_t *old_binding = unordered_map_remove(current_context->bound_basebuffers[buffer_mapindex], (void*)index);
        free(old_binding);
        return NULL;
    }else {
        basebuffer_binding_t *binding = unordered_map_get(current_context->bound_basebuffers[buffer_mapindex], (void*)index);
        if(binding == NULL) {
            binding = calloc(1, sizeof(basebuffer_binding_t));
            unordered_map_put(current_context->bound_basebuffers[buffer_mapindex], (void*)index, binding);
        }
        binding->index = index;
        binding->buffer = buffer;
        return binding;
    }
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    if(!current_context) return;
    es3_functions.glBindBufferBase(target, index, buffer);
    basebuffer_binding_t * binding = set_basebuffer(target, index, buffer);
    if(!binding) return;
    binding->ranged = false;
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if(!current_context) return;
    es3_functions.glBindBufferRange(target, index, buffer, offset, size);
    basebuffer_binding_t * binding = set_basebuffer(target, index, buffer);
    if(!binding) return;
    binding->ranged = true;
    binding->size = size;
    binding->offset = offset;
}

void glUseProgram(GLuint program) {
    if(!current_context) return;
    es3_functions.glUseProgram(program);
    current_context->program = program;
}

void glGetIntegerv(GLenum pname, GLint* data) {
    if(!current_context) return;
    switch (pname) {
        case GL_NUM_EXTENSIONS:
            es3_functions.glGetIntegerv(pname, data);
            (*data) += current_context->nextras;
            break;
        case GL_MAX_COLOR_ATTACHMENTS:
            *data = MAX_FBTARGETS;
            return;
        case GL_MAX_DRAW_BUFFERS:
            *data = current_context->max_drawbuffers;
            break;
        default:
            es3_functions.glGetIntegerv(pname, data);
    }
}

void glGetQueryObjectiv( 	GLuint id,
                            GLenum pname,
                            GLint * params) {
    if(!current_context) return;
    // This is not recommended but i don't care
    es3_functions.glGetQueryObjectuiv(id, pname, (GLuint*)params);
}

void glDepthRange(GLdouble nearVal,
                  GLdouble farVal) {
    if(!current_context) return;
    es3_functions.glDepthRangef((GLfloat)nearVal, (GLfloat)farVal);
}

void glDeleteTextures(GLsizei n, const GLuint *textures) {
    if(!current_context) return;
    es3_functions.glDeleteTextures(n, textures);
    for(int i = 0; i < n; i++) {
        void* tracker = unordered_map_remove(current_context->texture_swztrack_map, (void*)textures[i]);
        free(tracker);
    }
}

static bool buf_tex_trigger = false;

void glTexBuffer(GLenum target, GLenum internalFormat, GLuint buffer) {
    if(!current_context) return;
    if(current_context->es32) es3_functions.glTexBuffer(target, internalFormat, buffer);
    else if(current_context->buffer_texture_ext) es3_functions.glTexBufferEXT(target, internalFormat, buffer);
    else if(!buf_tex_trigger) {
        buf_tex_trigger = true;
        printf("LTW: Buffer textures aren't supported on your device\n");
    }
}

void glTexBufferARB(GLenum target, GLenum internalFormat, GLuint buffer) {
    glTexBuffer(target, internalFormat, buffer);
}

void glTexBufferRange(GLenum target, GLenum internalFormat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if(!current_context) return;
    if(current_context->es32) es3_functions.glTexBufferRange(target, internalFormat, buffer, offset, size);
    else if(current_context->buffer_texture_ext) es3_functions.glTexBufferRangeEXT(target, internalFormat, buffer, offset, size);
    else if(!buf_tex_trigger) {
        buf_tex_trigger = true;
        printf("LTW: Buffer textures aren't supported on your device\n");
    }
}

void glTexBufferRangeARB(GLenum target, GLenum internalFormat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    glTexBufferRange(target, internalFormat, buffer, offset, size);
}

static bool noerror;

__attribute((constructor)) void init_noerror() {
    noerror = env_istrue("LIBGL_NOERROR");
    debug = env_istrue("LTW_DEBUG");
    never_flush_buffers = env_istrue("LTW_NEVER_FLUSH_BUFFERS");
    if(!noerror) printf("LTW will NOT ignore GL errors. This may break mods, consider yourself warned.\n");
    if(debug) printf("LTW will allow GL_DEBUG_OUTPUT to be enabled. Expect massive logs.\n");
    if(never_flush_buffers) printf("LTW will prevent all explicit buffer flushes.\n");
}

GLenum glGetError() {
    if(noerror) return 0;
    else return es3_functions.glGetError();
}

void glDebugMessageControl( 	GLenum source,
                               GLenum type,
                               GLenum severity,
                               GLsizei count,
                               const GLuint *ids,
                               GLboolean enabled) {
    //STUB
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    // Técnica Mobile Glue: Sincroniza Viewport e Scissor para corrigir imagem esticada
    es3_functions.glViewport(x, y, width, height);
    es3_functions.glScissor(x, y, width, height);
}
