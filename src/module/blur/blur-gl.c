#include "backend/gl/glx.h"
#include "backend/gl/gl_common.h"

// Program and uniforms for blur shader
typedef struct {
	GLuint prog;
	GLint unifm_opacity;
	GLint orig_loc;
	GLint texorig_loc;
} gl_blur_shader_t;

struct gl_blur_context {
	enum blur_method method;
	gl_blur_shader_t *blur_shader;

	/// Temporary textures used for blurring. They are always the same size as the
	/// target, so they are always big enough without resizing.
	/// Turns out calling glTexImage to resize is expensive, so we avoid that.
	GLuint blur_texture[2];
	/// Temporary fbo used for blurring
	GLuint blur_fbo;

	int texture_width, texture_height;

	/// How much do we need to resize the damaged region for blurring.
	int resize_width, resize_height;

	int npasses;
};

bool gl_blur(backend_t *base, double opacity, void *, const region_t *reg_blur,
             const region_t *reg_visible);
void *gl_create_blur_context(backend_t *base, enum blur_method, void *args);
void gl_destroy_blur_context(backend_t *base, void *ctx);
void gl_get_blur_size(void *blur_context, int *width, int *height);

/**
 * Blur contents in a particular region.
 */
bool gl_blur(backend_t *base, double opacity, void *ctx, const region_t *reg_blur,
             const region_t *reg_visible attr_unused) {
	struct gl_blur_context *bctx = ctx;
	struct gl_data *gd = (void *)base;

	if (gd->width + bctx->resize_width * 2 != bctx->texture_width ||
	    gd->height + bctx->resize_height * 2 != bctx->texture_height) {
		// Resize the temporary textures used for blur in case the root
		// size changed
		bctx->texture_width = gd->width + bctx->resize_width * 2;
		bctx->texture_height = gd->height + bctx->resize_height * 2;

		glBindTexture(GL_TEXTURE_2D, bctx->blur_texture[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bctx->texture_width,
		             bctx->texture_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, bctx->blur_texture[1]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bctx->texture_width,
		             bctx->texture_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

		// XXX: do we need projection matrix for blur at all?
		// Note: OpenGL matrices are column major
		GLfloat projection_matrix[4][4] = {
		    {2.0f / (GLfloat)bctx->texture_width, 0, 0, 0},
		    {0, 2.0f / (GLfloat)bctx->texture_height, 0, 0},
		    {0, 0, 0, 0},
		    {-1, -1, 0, 1}};

		// Update projection matrices in the blur shaders
		for (int i = 0; i < bctx->npasses - 1; i++) {
			assert(bctx->blur_shader[i].prog);
			glUseProgram(bctx->blur_shader[i].prog);
			int pml = glGetUniformLocationChecked(bctx->blur_shader[i].prog,
			                                      "projection");
			glUniformMatrix4fv(pml, 1, false, projection_matrix[0]);
		}

		GLfloat projection_matrix2[4][4] = {{2.0f / (GLfloat)gd->width, 0, 0, 0},
		                                    {0, 2.0f / (GLfloat)gd->height, 0, 0},
		                                    {0, 0, 0, 0},
		                                    {-1, -1, 0, 1}};
		assert(bctx->blur_shader[bctx->npasses - 1].prog);
		glUseProgram(bctx->blur_shader[bctx->npasses - 1].prog);
		int pml = glGetUniformLocationChecked(
		    bctx->blur_shader[bctx->npasses - 1].prog, "projection");
		glUniformMatrix4fv(pml, 1, false, projection_matrix2[0]);
	}

	// Remainder: regions are in Xorg coordinates
	auto reg_blur_resized =
	    resize_region(reg_blur, bctx->resize_width, bctx->resize_height);
	const rect_t *extent = pixman_region32_extents((region_t *)reg_blur),
	             *extent_resized = pixman_region32_extents(&reg_blur_resized);
	int width = extent->x2 - extent->x1, height = extent->y2 - extent->y1;
	int dst_y_resized_screen_coord = gd->height - extent_resized->y2,
	    dst_y_resized_fb_coord = bctx->texture_height - extent_resized->y2;
	if (width == 0 || height == 0) {
		return true;
	}

	bool ret = false;
	int nrects, nrects_resized;
	const rect_t *rects = pixman_region32_rectangles((region_t *)reg_blur, &nrects),
	             *rects_resized =
	                 pixman_region32_rectangles(&reg_blur_resized, &nrects_resized);
	if (!nrects || !nrects_resized) {
		return true;
	}

	auto coord = ccalloc(nrects * 16, GLint);
	auto indices = ccalloc(nrects * 6, GLuint);
	x_rect_to_coords(nrects, rects, extent_resized->x1, extent_resized->y2,
	                 bctx->texture_height, gd->height, false, coord, indices);

	auto coord_resized = ccalloc(nrects_resized * 16, GLint);
	auto indices_resized = ccalloc(nrects_resized * 6, GLuint);
	x_rect_to_coords(nrects_resized, rects_resized, extent_resized->x1,
	                 extent_resized->y2, bctx->texture_height, bctx->texture_height,
	                 false, coord_resized, indices_resized);
	pixman_region32_fini(&reg_blur_resized);

	GLuint vao[2];
	glGenVertexArrays(2, vao);
	GLuint bo[4];
	glGenBuffers(4, bo);

	glBindVertexArray(vao[0]);
	glBindBuffer(GL_ARRAY_BUFFER, bo[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bo[1]);
	glBufferData(GL_ARRAY_BUFFER, (long)sizeof(*coord) * nrects * 16, coord, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (long)sizeof(*indices) * nrects * 6,
	             indices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(vert_coord_loc);
	glEnableVertexAttribArray(vert_in_texcoord_loc);
	glVertexAttribPointer(vert_coord_loc, 2, GL_INT, GL_FALSE, sizeof(GLint) * 4, NULL);
	glVertexAttribPointer(vert_in_texcoord_loc, 2, GL_INT, GL_FALSE,
	                      sizeof(GLint) * 4, (void *)(sizeof(GLint) * 2));

	glBindVertexArray(vao[1]);
	glBindBuffer(GL_ARRAY_BUFFER, bo[2]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bo[3]);
	glBufferData(GL_ARRAY_BUFFER, (long)sizeof(*coord_resized) * nrects_resized * 16,
	             coord_resized, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
	             (long)sizeof(*indices_resized) * nrects_resized * 6, indices_resized,
	             GL_STATIC_DRAW);
	glEnableVertexAttribArray(vert_coord_loc);
	glEnableVertexAttribArray(vert_in_texcoord_loc);
	glVertexAttribPointer(vert_coord_loc, 2, GL_INT, GL_FALSE, sizeof(GLint) * 4, NULL);
	glVertexAttribPointer(vert_in_texcoord_loc, 2, GL_INT, GL_FALSE,
	                      sizeof(GLint) * 4, (void *)(sizeof(GLint) * 2));

	int curr = 0;
	for (int i = 0; i < bctx->npasses; ++i) {
		const gl_blur_shader_t *p = &bctx->blur_shader[i];
		assert(p->prog);

		assert(bctx->blur_texture[curr]);

		// The origin to use when sampling from the source texture
		GLint texorig_x, texorig_y;
		GLuint src_texture;

		if (i == 0) {
			texorig_x = extent_resized->x1;
			texorig_y = dst_y_resized_screen_coord;
			src_texture = gd->back_texture;
		} else {
			texorig_x = 0;
			texorig_y = 0;
			src_texture = bctx->blur_texture[curr];
		}

		glBindTexture(GL_TEXTURE_2D, src_texture);
		glUseProgram(p->prog);
		if (i < bctx->npasses - 1) {
			// not last pass, draw into framebuffer, with resized regions
			glBindVertexArray(vao[1]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bctx->blur_fbo);

			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			                       GL_TEXTURE_2D, bctx->blur_texture[!curr], 0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				log_error("Framebuffer attachment failed.");
				goto end;
			}
			glUniform1f(p->unifm_opacity, 1.0);
			// For other than last pass, we are drawing to a texture, we
			// translate the render origin so we don't need a big texture
			glUniform2f(p->orig_loc, -(GLfloat)extent_resized->x1,
			            -(GLfloat)dst_y_resized_fb_coord);
			glViewport(0, 0, bctx->texture_width, bctx->texture_height);
		} else {
			// last pass, draw directly into the back buffer, with origin
			// regions
			glBindVertexArray(vao[0]);
			glBindFramebuffer(GL_FRAMEBUFFER, gd->back_fbo);
			glUniform1f(p->unifm_opacity, (float)opacity);
			glUniform2f(p->orig_loc, 0, 0);
			glViewport(0, 0, gd->width, gd->height);
		}

		glUniform2f(p->texorig_loc, (GLfloat)texorig_x, (GLfloat)texorig_y);
		glDrawElements(GL_TRIANGLES, nrects * 6, GL_UNSIGNED_INT, NULL);

		// XXX use multiple draw calls is probably going to be slow than
		//     just simply blur the whole area.

		curr = !curr;
	}

	ret = true;

end:
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(4, bo);
	glBindVertexArray(0);
	glDeleteVertexArrays(2, vao);

	free(indices);
	free(coord);
	free(indices_resized);
	free(coord_resized);

	gl_check_err();

	return ret;
}

static inline void gl_free_blur_shader(gl_blur_shader_t *shader) {
	if (shader->prog) {
		glDeleteProgram(shader->prog);
	}

	shader->prog = 0;
}

void gl_destroy_blur_context(backend_t *base attr_unused, void *ctx) {
	struct gl_blur_context *bctx = ctx;
	// Free GLSL shaders/programs
	for (int i = 0; i < bctx->npasses; ++i) {
		gl_free_blur_shader(&bctx->blur_shader[i]);
	}
	free(bctx->blur_shader);

	glDeleteTextures(bctx->npasses > 1 ? 2 : 1, bctx->blur_texture);
	if (bctx->npasses > 1) {
		glDeleteFramebuffers(1, &bctx->blur_fbo);
	}
	free(bctx);

	gl_check_err();
}

/**
 * Initialize GL blur filters.
 */
void *gl_create_blur_context(backend_t *base, enum blur_method method, void *args) {
	bool success = true;
	auto gd = (struct gl_data *)base;

	struct conv **kernels;
	auto ctx = ccalloc(1, struct gl_blur_context);

	if (!method || method >= BLUR_METHOD_INVALID) {
		ctx->method = BLUR_METHOD_NONE;
		return ctx;
	}

	int nkernels;
	ctx->method = BLUR_METHOD_KERNEL;
	if (method == BLUR_METHOD_KERNEL) {
		nkernels = ((struct kernel_blur_args *)args)->kernel_count;
		kernels = ((struct kernel_blur_args *)args)->kernels;
	} else {
		kernels = generate_blur_kernel(method, args, &nkernels);
	}

	if (!nkernels) {
		ctx->method = BLUR_METHOD_NONE;
		return ctx;
	}

	ctx->blur_shader = ccalloc(max2(2, nkernels), gl_blur_shader_t);

	char *lc_numeric_old = strdup(setlocale(LC_NUMERIC, NULL));
	// Enforce LC_NUMERIC locale "C" here to make sure decimal point is sane
	// Thanks to hiciu for reporting.
	setlocale(LC_NUMERIC, "C");

	// clang-format off
	static const char *FRAG_SHADER_BLUR = GLSL(330,
		%s\n // other extension pragmas
		uniform sampler2D tex_scr;
		uniform float opacity;
		in vec2 texcoord;
		out vec4 out_color;
		void main() {
			vec4 sum = vec4(0.0, 0.0, 0.0, 0.0);
			%s //body of the convolution
			out_color = sum / float(%.7g) * opacity;
		}
	);
	static const char *FRAG_SHADER_BLUR_ADD = QUOTE(
		sum += float(%.7g) *
		       texelFetch(tex_scr, ivec2(texcoord + vec2(%d, %d)), 0);
	);
	// clang-format on

	const char *shader_add = FRAG_SHADER_BLUR_ADD;
	char *extension = strdup("");

	for (int i = 0; i < nkernels; i++) {
		auto kern = kernels[i];
		// Build shader
		int width = kern->w, height = kern->h;
		int nele = width * height;
		size_t body_len = (strlen(shader_add) + 42) * (uint)nele;
		char *shader_body = ccalloc(body_len, char);
		char *pc = shader_body;

		double sum = 0.0;
		for (int j = 0; j < height; ++j) {
			for (int k = 0; k < width; ++k) {
				double val;
				val = kern->data[j * width + k];
				if (val == 0) {
					continue;
				}
				sum += val;
				pc += snprintf(pc, body_len - (ulong)(pc - shader_body),
				               FRAG_SHADER_BLUR_ADD, val, k - width / 2,
				               j - height / 2);
				assert(pc < shader_body + body_len);
			}
		}

		auto pass = ctx->blur_shader + i;
		size_t shader_len = strlen(FRAG_SHADER_BLUR) + strlen(extension) +
		                    strlen(shader_body) + 10 /* sum */ +
		                    1 /* null terminator */;
		char *shader_str = ccalloc(shader_len, char);
		auto real_shader_len = snprintf(shader_str, shader_len, FRAG_SHADER_BLUR,
		                                extension, shader_body, sum);
		CHECK(real_shader_len >= 0);
		CHECK((size_t)real_shader_len < shader_len);
		free(shader_body);

		// Build program
		pass->prog = gl_create_program_from_str(vertex_shader, shader_str);
		free(shader_str);
		if (!pass->prog) {
			log_error("Failed to create GLSL program.");
			success = false;
			goto out;
		}
		glBindFragDataLocation(pass->prog, 0, "out_color");

		// Get uniform addresses
		pass->unifm_opacity = glGetUniformLocationChecked(pass->prog, "opacity");
		pass->orig_loc = glGetUniformLocationChecked(pass->prog, "orig");
		pass->texorig_loc = glGetUniformLocationChecked(pass->prog, "texorig");
		ctx->resize_width += kern->w / 2;
		ctx->resize_height += kern->h / 2;
	}

	if (nkernels == 1) {
		// Generate an extra null pass so we don't need special code path for
		// the single pass case
		auto pass = &ctx->blur_shader[1];
		pass->prog = gl_create_program_from_str(vertex_shader, dummy_frag);
		pass->unifm_opacity = glGetUniformLocationChecked(pass->prog, "opacity");
		pass->orig_loc = glGetUniformLocationChecked(pass->prog, "orig");
		pass->texorig_loc = glGetUniformLocationChecked(pass->prog, "texorig");
		ctx->npasses = 2;
	} else {
		ctx->npasses = nkernels;
	}

	// Texture size will be defined by gl_blur
	glGenTextures(2, ctx->blur_texture);
	glBindTexture(GL_TEXTURE_2D, ctx->blur_texture[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, ctx->blur_texture[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// Generate FBO and textures when needed
	glGenFramebuffers(1, &ctx->blur_fbo);
	if (!ctx->blur_fbo) {
		log_error("Failed to generate framebuffer object for blur");
		success = false;
		goto out;
	}

out:
	if (method != BLUR_METHOD_KERNEL) {
		// We generated the blur kernels, so we need to free them
		for (int i = 0; i < nkernels; i++) {
			free(kernels[i]);
		}
		free(kernels);
	}

	if (!success) {
		gl_destroy_blur_context(&gd->base, ctx);
		ctx = NULL;
	}

	free(extension);
	// Restore LC_NUMERIC
	setlocale(LC_NUMERIC, lc_numeric_old);
	free(lc_numeric_old);

	gl_check_err();
	return ctx;
}

void gl_get_blur_size(void *blur_context, int *width, int *height) {
	struct gl_blur_context *ctx = blur_context;
	*width = ctx->resize_width;
	*height = ctx->resize_height;
}
