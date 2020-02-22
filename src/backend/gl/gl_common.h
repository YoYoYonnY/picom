// SPDX-License-Identifier: MPL-2.0
// Copyright (c) Yuxuan Shui <yshuiv7@gmail.com>
#pragma once
#include <GL/gl.h>
#include <GL/glext.h>
#include <locale.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "backend/backend.h"
#include "log.h"
#include "region.h"

static inline GLint glGetUniformLocationChecked(GLuint p, const char *name) {
	auto ret = glGetUniformLocation(p, name);
	if (ret < 0) {
		log_error("Failed to get location of uniform '%s'. the compositor might "
		          "not work correctly.",
		          name);
	}
	return ret;
}
/// Convert rectangles in X coordinates to OpenGL vertex and texture coordinates
/// @param[in] nrects, rects   rectangles
/// @param[in] dst_x, dst_y    origin of the OpenGL texture, affect the calculated texture
///                            coordinates
/// @param[in] texture_height  height of the OpenGL texture
/// @param[in] root_height     height of the back buffer
/// @param[in] y_inverted      whether the texture is y inverted
/// @param[out] coord, indices output
static inline void
x_rect_to_coords(int nrects, const rect_t *rects, int dst_x, int dst_y, int texture_height,
                 int root_height, bool y_inverted, GLint *coord, GLuint *indices) {
	dst_y = root_height - dst_y;
	if (y_inverted) {
		dst_y -= texture_height;
	}

	for (int i = 0; i < nrects; i++) {
		// Y-flip. Note after this, crect.y1 > crect.y2
		rect_t crect = rects[i];
		crect.y1 = root_height - crect.y1;
		crect.y2 = root_height - crect.y2;

		// Calculate texture coordinates
		// (texture_x1, texture_y1), texture coord for the _bottom left_ corner
		GLint texture_x1 = crect.x1 - dst_x, texture_y1 = crect.y2 - dst_y,
		      texture_x2 = texture_x1 + (crect.x2 - crect.x1),
		      texture_y2 = texture_y1 + (crect.y1 - crect.y2);

		// X pixmaps might be Y inverted, invert the texture coordinates
		if (y_inverted) {
			texture_y1 = texture_height - texture_y1;
			texture_y2 = texture_height - texture_y2;
		}

		// Vertex coordinates
		auto vx1 = crect.x1;
		auto vy1 = crect.y2;
		auto vx2 = crect.x2;
		auto vy2 = crect.y1;

		// log_trace("Rect %d: %f, %f, %f, %f -> %d, %d, %d, %d",
		//          ri, rx, ry, rxe, rye, rdx, rdy, rdxe, rdye);

		memcpy(&coord[i * 16],
		       (GLint[][2]){
		           {vx1, vy1},
		           {texture_x1, texture_y1},
		           {vx2, vy1},
		           {texture_x2, texture_y1},
		           {vx2, vy2},
		           {texture_x2, texture_y2},
		           {vx1, vy2},
		           {texture_x1, texture_y2},
		       },
		       sizeof(GLint[2]) * 8);

		GLuint u = (GLuint)(i * 4);
		memcpy(&indices[i * 6], (GLuint[]){u + 0, u + 1, u + 2, u + 2, u + 3, u + 0},
		       sizeof(GLuint) * 6);
	}
}

#define GLSL(version, ...) "#version " #version "\n" #__VA_ARGS__

// clang-format off
static const char *vertex_shader = GLSL(330,
	uniform mat4 projection;
	uniform vec2 orig;
	uniform vec2 texorig;
	layout(location = 0) in vec2 coord;
	layout(location = 1) in vec2 in_texcoord;
	out vec2 texcoord;
	void main() {
		gl_Position = projection * vec4(coord + orig, 0, 1);
		texcoord = in_texcoord + texorig;
	}
);
static const char dummy_frag[] = GLSL(330,
	uniform sampler2D tex;
	in vec2 texcoord;
	void main() {
		gl_FragColor = texelFetch(tex, ivec2(texcoord.xy), 0);
	}
);

static const char fill_frag[] = GLSL(330,
	uniform vec4 color;
	void main() {
		gl_FragColor = color;
	}
);

static const char fill_vert[] = GLSL(330,
	layout(location = 0) in vec2 in_coord;
	uniform mat4 projection;
	void main() {
		gl_Position = projection * vec4(in_coord, 0, 1);
	}
);

static const char interpolating_frag[] = GLSL(330,
	uniform sampler2D tex;
	in vec2 texcoord;
	void main() {
		gl_FragColor = vec4(texture2D(tex, vec2(texcoord.xy), 0).rgb, 1);
	}
);

static const char interpolating_vert[] = GLSL(330,
	uniform mat4 projection;
	uniform vec2 texsize;
	layout(location = 0) in vec2 in_coord;
	layout(location = 1) in vec2 in_texcoord;
	out vec2 texcoord;
	void main() {
		gl_Position = projection * vec4(in_coord, 0, 1);
		texcoord = in_texcoord / texsize;
	}
);
// clang-format on

static const GLuint vert_coord_loc = 0;
static const GLuint vert_in_texcoord_loc = 1;

#define CASESTRRET(s)                                                                    \
	case s: return #s

// Program and uniforms for window shader
typedef struct {
	GLuint prog;
	GLint unifm_opacity;
	GLint unifm_invert_color;
	GLint unifm_tex;
	GLint unifm_dim;
	GLint unifm_brightness;
	GLint unifm_max_brightness;
} gl_win_shader_t;

// Program and uniforms for brightness shader
typedef struct {
	GLuint prog;
} gl_brightness_shader_t;

typedef struct {
	GLuint prog;
	GLint color_loc;
} gl_fill_shader_t;

struct gl_texture {
	int refcount;
	GLuint texture;
	int width, height;
	bool y_inverted;

	// Textures for auxiliary uses.
	GLuint auxiliary_texture[2];
	void *user_data;
};

/// @brief Wrapper of a binded GLX texture.
typedef struct gl_image {
	struct gl_texture *inner;
	double opacity;
	double dim;
	double max_brightness;
	int ewidth, eheight;
	bool has_alpha;
	bool color_inverted;
} gl_image_t;

struct gl_data {
	backend_t base;
	// If we are using proprietary NVIDIA driver
	bool is_nvidia;
	// Height and width of the viewport
	int height, width;
	gl_win_shader_t win_shader;
	gl_brightness_shader_t brightness_shader;
	gl_fill_shader_t fill_shader;
	GLuint back_texture, back_fbo;
	GLuint present_prog;

	/// Called when an gl_texture is decoupled from the texture it refers. Returns
	/// the decoupled user_data
	void *(*decouple_texture_user_data)(backend_t *base, void *user_data);

	/// Release the user data attached to a gl_texture
	void (*release_user_data)(backend_t *base, struct gl_texture *);

	struct log_target *logger;
};

typedef struct session session_t;

#define GL_PROG_MAIN_INIT                                                                \
	{ .prog = 0, .unifm_opacity = -1, .unifm_invert_color = -1, .unifm_tex = -1, }

GLuint gl_create_shader(GLenum shader_type, const char *shader_str);
GLuint gl_create_program(const GLuint *const shaders, int nshaders);
GLuint gl_create_program_from_str(const char *vert_shader_str, const char *frag_shader_str);

/**
 * @brief Render a region with texture data.
 */
void gl_compose(backend_t *, void *ptex, int dst_x, int dst_y, const region_t *reg_tgt,
                const region_t *reg_visible);

void gl_resize(struct gl_data *, int width, int height);

bool gl_init(struct gl_data *gd, session_t *);
void gl_deinit(struct gl_data *gd);

GLuint gl_new_texture(GLenum target);

bool gl_image_op(backend_t *base, enum image_operations op, void *image_data,
                 const region_t *reg_op, const region_t *reg_visible, void *arg);

void gl_release_image(backend_t *base, void *image_data);

void *gl_copy(backend_t *base, const void *image_data, const region_t *reg_visible);

bool gl_is_image_transparent(backend_t *base, void *image_data);
void gl_fill(backend_t *base, struct color, const region_t *clip);

void gl_present(backend_t *base, const region_t *);

static inline void gl_delete_texture(GLuint texture) {
	glDeleteTextures(1, &texture);
}

/**
 * Get a textual representation of an OpenGL error.
 */
static inline const char *gl_get_err_str(GLenum err) {
	switch (err) {
		CASESTRRET(GL_NO_ERROR);
		CASESTRRET(GL_INVALID_ENUM);
		CASESTRRET(GL_INVALID_VALUE);
		CASESTRRET(GL_INVALID_OPERATION);
		CASESTRRET(GL_INVALID_FRAMEBUFFER_OPERATION);
		CASESTRRET(GL_OUT_OF_MEMORY);
		CASESTRRET(GL_STACK_UNDERFLOW);
		CASESTRRET(GL_STACK_OVERFLOW);
	}
	return NULL;
}

/**
 * Check for GLX error.
 *
 * http://blog.nobel-joergensen.com/2013/01/29/debugging-opengl-using-glgeterror/
 */
static inline void gl_check_err_(const char *func, int line) {
	GLenum err = GL_NO_ERROR;

	while (GL_NO_ERROR != (err = glGetError())) {
		const char *errtext = gl_get_err_str(err);
		if (errtext) {
			log_printf(tls_logger, LOG_LEVEL_ERROR, func,
			           "GLX error at line %d: %s", line, errtext);
		} else {
			log_printf(tls_logger, LOG_LEVEL_ERROR, func,
			           "GLX error at line %d: %d", line, err);
		}
	}
}

static inline GLuint glx_gen_texture(GLenum tex_tgt, int width, int height) {
	GLuint tex = 0;
	glGenTextures(1, &tex);
	if (!tex)
		return 0;
	glEnable(tex_tgt);
	glBindTexture(tex_tgt, tex);
	glTexParameteri(tex_tgt, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(tex_tgt, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(tex_tgt, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex_tgt, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(tex_tgt, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(tex_tgt, 0);

	return tex;
}

static inline void glx_copy_region_to_tex(session_t *ps, GLenum tex_tgt, int basex,
                                          int basey, int dx, int dy, int width, int height) {
	if (width > 0 && height > 0)
		glCopyTexSubImage2D(tex_tgt, 0, dx - basex, dy - basey, dx,
		                    ps->root_height - dy - height, width, height);
}

static inline void gl_clear_err(void) {
	while (glGetError() != GL_NO_ERROR);
}

#define gl_check_err() gl_check_err_(__func__, __LINE__)

/**
 * Check if a GLX extension exists.
 */
static inline bool gl_has_extension(const char *ext) {
	int nexts = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &nexts);
	for (int i = 0; i < nexts || !nexts; i++) {
		const char *exti = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);
		if (exti == NULL) {
			break;
		}
		if (strcmp(ext, exti) == 0) {
			return true;
		}
	}
	gl_clear_err();
	log_info("Missing GL extension %s.", ext);
	return false;
}
