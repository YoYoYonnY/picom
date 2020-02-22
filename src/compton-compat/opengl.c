#include "opengl.h"
#include "common.h"
#include <xcb/render.h>
#include <xcb/xcb.h>

/**
 * Blur contents in a particular region.
 *
 * XXX seems to be way to complex for what it does
 */
bool glx_blur_dst(session_t *ps, int dx, int dy, int width, int height, float z,
				  GLfloat factor_center, const region_t *reg_tgt, glx_blur_cache_t *pbc) {
	assert(ps->psglx->blur_passes[0].prog);
	int blur_kernel_count = *module_xgetint(ps->module_blur, "kernel_count");
	const bool more_passes = blur_kernel_count > 1;
	const bool have_scissors = glIsEnabled(GL_SCISSOR_TEST);
	const bool have_stencil = glIsEnabled(GL_STENCIL_TEST);
	bool ret = false;

	// Calculate copy region size
	glx_blur_cache_t ibc = {.width = 0, .height = 0};
	if (!pbc)
		pbc = &ibc;

	int mdx = dx, mdy = dy, mwidth = width, mheight = height;
	// log_trace("%d, %d, %d, %d", mdx, mdy, mwidth, mheight);

	/*
	if (ps->o.resize_damage > 0) {
	  int inc_x = 0, inc_y = 0;
	  for (int i = 0; i < MAX_BLUR_PASS; ++i) {
		XFixed *kern = ps->o.blur_kerns[i];
		if (!kern) break;
		inc_x += XFIXED_TO_DOUBLE(kern[0]) / 2;
		inc_y += XFIXED_TO_DOUBLE(kern[1]) / 2;
	  }
	  inc_x = min2(ps->o.resize_damage, inc_x);
	  inc_y = min2(ps->o.resize_damage, inc_y);

	  mdx = max2(dx - inc_x, 0);
	  mdy = max2(dy - inc_y, 0);
	  int mdx2 = min2(dx + width + inc_x, ps->root_width),
		  mdy2 = min2(dy + height + inc_y, ps->root_height);
	  mwidth = mdx2 - mdx;
	  mheight = mdy2 - mdy;
	}
	*/

	GLenum tex_tgt = GL_TEXTURE_RECTANGLE;
	if (ps->psglx->has_texture_non_power_of_two)
		tex_tgt = GL_TEXTURE_2D;

	// Free textures if size inconsistency discovered
	if (mwidth != pbc->width || mheight != pbc->height)
		free_glx_bc_resize(ps, pbc);

	// Generate FBO and textures if needed
	if (!pbc->textures[0])
		pbc->textures[0] = glx_gen_texture(tex_tgt, mwidth, mheight);
	GLuint tex_scr = pbc->textures[0];
	if (more_passes && !pbc->textures[1])
		pbc->textures[1] = glx_gen_texture(tex_tgt, mwidth, mheight);
	pbc->width = mwidth;
	pbc->height = mheight;
	GLuint tex_scr2 = pbc->textures[1];
	if (more_passes && !pbc->fbo)
		glGenFramebuffers(1, &pbc->fbo);
	const GLuint fbo = pbc->fbo;

	if (!tex_scr || (more_passes && !tex_scr2)) {
		log_error("Failed to allocate texture.");
		goto glx_blur_dst_end;
	}
	if (more_passes && !fbo) {
		log_error("Failed to allocate framebuffer.");
		goto glx_blur_dst_end;
	}

	// Read destination pixels into a texture
	glEnable(tex_tgt);
	glBindTexture(tex_tgt, tex_scr);
	glx_copy_region_to_tex(ps, tex_tgt, mdx, mdy, mdx, mdy, mwidth, mheight);
	/*
	if (tex_scr2) {
	  glBindTexture(tex_tgt, tex_scr2);
	  glx_copy_region_to_tex(ps, tex_tgt, mdx, mdy, mdx, mdy, mwidth, dx - mdx);
	  glx_copy_region_to_tex(ps, tex_tgt, mdx, mdy, mdx, dy + height,
		  mwidth, mdy + mheight - dy - height);
	  glx_copy_region_to_tex(ps, tex_tgt, mdx, mdy, mdx, dy, dx - mdx, height);
	  glx_copy_region_to_tex(ps, tex_tgt, mdx, mdy, dx + width, dy,
		  mdx + mwidth - dx - width, height);
	} */

	// Texture scaling factor
	GLfloat texfac_x = 1.0f, texfac_y = 1.0f;
	if (tex_tgt == GL_TEXTURE_2D) {
		texfac_x /= (GLfloat)mwidth;
		texfac_y /= (GLfloat)mheight;
	}

	// Paint it back
	if (more_passes) {
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_SCISSOR_TEST);
	}

	bool last_pass = false;
	for (int i = 0; i < blur_kernel_count; ++i) {
		last_pass = (i == blur_kernel_count - 1);
		const glx_blur_pass_t *ppass = &ps->psglx->blur_passes[i];
		assert(ppass->prog);

		assert(tex_scr);
		glBindTexture(tex_tgt, tex_scr);

		if (!last_pass) {
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
								   GL_TEXTURE_2D, tex_scr2, 0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				log_error("Framebuffer attachment failed.");
				goto glx_blur_dst_end;
			}
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDrawBuffer(GL_BACK);
			if (have_scissors)
				glEnable(GL_SCISSOR_TEST);
			if (have_stencil)
				glEnable(GL_STENCIL_TEST);
		}

		// Color negation for testing...
		// glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		// glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
		// glTexEnvf(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glUseProgram(ppass->prog);
		if (ppass->unifm_offset_x >= 0)
			glUniform1f(ppass->unifm_offset_x, texfac_x);
		if (ppass->unifm_offset_y >= 0)
			glUniform1f(ppass->unifm_offset_y, texfac_y);
		if (ppass->unifm_factor_center >= 0)
			glUniform1f(ppass->unifm_factor_center, factor_center);

		P_PAINTREG_START(crect) {
			auto rx = (GLfloat)(crect.x1 - mdx) * texfac_x;
			auto ry = (GLfloat)(mheight - (crect.y1 - mdy)) * texfac_y;
			auto rxe = rx + (GLfloat)(crect.x2 - crect.x1) * texfac_x;
			auto rye = ry - (GLfloat)(crect.y2 - crect.y1) * texfac_y;
			auto rdx = (GLfloat)(crect.x1 - mdx);
			auto rdy = (GLfloat)(mheight - crect.y1 + mdy);
			if (last_pass) {
				rdx = (GLfloat)crect.x1;
				rdy = (GLfloat)(ps->root_height - crect.y1);
			}
			auto rdxe = rdx + (GLfloat)(crect.x2 - crect.x1);
			auto rdye = rdy - (GLfloat)(crect.y2 - crect.y1);

			// log_trace("%f, %f, %f, %f -> %f, %f, %f, %f", rx, ry,
			// rxe, rye, rdx,
			//          rdy, rdxe, rdye);

			glTexCoord2f(rx, ry);
			glVertex3f(rdx, rdy, z);

			glTexCoord2f(rxe, ry);
			glVertex3f(rdxe, rdy, z);

			glTexCoord2f(rxe, rye);
			glVertex3f(rdxe, rdye, z);

			glTexCoord2f(rx, rye);
			glVertex3f(rdx, rdye, z);
		}
		P_PAINTREG_END();

		glUseProgram(0);

		// Swap tex_scr and tex_scr2
		{
			GLuint tmp = tex_scr2;
			tex_scr2 = tex_scr;
			tex_scr = tmp;
		}
	}

	ret = true;

glx_blur_dst_end:
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(tex_tgt, 0);
	glDisable(tex_tgt);
	if (have_scissors)
		glEnable(GL_SCISSOR_TEST);
	if (have_stencil)
		glEnable(GL_STENCIL_TEST);

	if (&ibc == pbc) {
		free_glx_bc(ps, pbc);
	}

	gl_check_err();

	return ret;
}
