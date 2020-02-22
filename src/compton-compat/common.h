#pragma once
#include "../common.h"

#ifdef CONFIG_OPENGL
#include "opengl.h"
#include "../opengl.h"
#include "backend/gl/gl_common.h"

#define P_PAINTREG_START(var)                                                            \
	region_t reg_new;                                                                \
	int nrects;                                                                      \
	const rect_t *rects;                                                             \
	assert(width >= 0 && height >= 0);                                               \
	pixman_region32_init_rect(&reg_new, dx, dy, (uint)width, (uint)height);          \
	pixman_region32_intersect(&reg_new, &reg_new, (region_t *)reg_tgt);              \
	rects = pixman_region32_rectangles(&reg_new, &nrects);                           \
	glBegin(GL_QUADS);                                                               \
                                                                                         \
	for (int ri = 0; ri < nrects; ++ri) {                                            \
		rect_t var = rects[ri];

#define P_PAINTREG_END()                                                                 \
	}                                                                                \
	glEnd();                                                                         \
                                                                                         \
	pixman_region32_fini(&reg_new);
#endif

/**
 * Destroy a <code>Picture</code>.
 */
static inline void free_picture(xcb_connection_t *c, xcb_render_picture_t *p) {
	if (*p) {
		xcb_render_free_picture(c, *p);
		*p = XCB_NONE;
	}
}

/**
 * Free paint_t.
 */
static inline void free_paint(session_t *ps, paint_t *ppaint) {
#ifdef CONFIG_OPENGL
	free_paint_glx(ps, ppaint);
#endif
	free_picture(ps->c, &ppaint->pict);
	if (ppaint->pixmap)
		xcb_free_pixmap(ps->c, ppaint->pixmap);
	ppaint->pixmap = XCB_NONE;
}

/**
 * Free root tile related things.
 */
static inline void free_root_tile(session_t *ps) {
	free_picture(ps->c, &ps->root_tile_paint.pict);
#ifdef CONFIG_OPENGL
	free_texture(ps, &ps->root_tile_paint.ptex);
#else
	assert(!ps->root_tile_paint.ptex);
#endif
	if (ps->root_tile_fill) {
		xcb_free_pixmap(ps->c, ps->root_tile_paint.pixmap);
		ps->root_tile_paint.pixmap = XCB_NONE;
	}
	ps->root_tile_paint.pixmap = XCB_NONE;
	ps->root_tile_fill = false;
}

// Load module.h as late as possible.
// Effectively, this #include's "common.h" in module.h
