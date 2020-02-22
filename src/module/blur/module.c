#include "module.h"
#include "picom.h"
#include "opengl.h"
#include "blur.h"

cfg_type_t cfg_type_blur_method = {
	.repr = CFG_TINTEGER,
	.set = NULL,
	.get = NULL,
	.unset = NULL,
};

/// Blur a given region of the rendering buffer.
static bool backend_blur(module_t *module, session_t *ps, backend_t *backend_data, double opacity, void *blur_ctx,
             const region_t *reg_blur, const region_t *reg_visible)
    attr_nonnull(1, 2, 3, 5, 6);
/// Create a blur context that can be used to call `blur`
static void *backend_create_blur_context(module_t *module, session_t *ps, backend_t *base, enum blur_method blur_method, void *args);
/// Destroy a blur context
static void backend_destroy_blur_context(module_t *module, session_t *ps, backend_t *base, void *ctx);
/// Get how many pixels outside of the blur area is needed for blur
static void backend_get_blur_size(module_t *module, session_t *ps, void *blur_context, int *width, int *height);

#include "blur-xrender.c"
#include "blur-gl.c"

struct module_options {
#define OPTION MODULE_DECLARE_OPTION
#include "cfg_mod.h"
#undef OPTION
} options;
struct module_properties {
#define OPTION MODULE_DECLARE_PROPERTY
#include "cfg_mod.h"
#undef OPTION
} prop;

struct window_options {
#define OPTION MODULE_DECLARE_OPTION
#include "cfg_win.h"
#undef OPTION
};
struct window_properties {
#define OPTION MODULE_DECLARE_PROPERTY
#include "cfg_win.h"
#undef OPTION
} winprop;

static void *backend_create_blur_context(module_t *module, session_t *ps, backend_t *base, enum blur_method blur_method, void *args) {
	UNUSED(module);

	switch (ps->o.backend) {
	case BKEND_XRENDER:
		return xrender_create_blur_context(base, blur_method, args);
	case BKEND_GLX:
		return gl_create_blur_context(base, blur_method, args);
	default:
		return NULL;
	}
}
static void backend_destroy_blur_context(module_t *module, session_t *ps, backend_t *base, void *ctx) {
	UNUSED(module);

	switch (ps->o.backend) {
	case BKEND_XRENDER:
		xrender_destroy_blur_context(base, ctx);
		break;
	case BKEND_GLX:
		gl_destroy_blur_context(base, ctx);
		break;
	default:
		break;
	}
}
static bool backend_blur(module_t *module, session_t *ps, backend_t *backend_data, double opacity, void *blur_ctx,
	        const region_t *reg_blur, const region_t *reg_visible) {
	UNUSED(module);
	UNUSED(ps);

	switch (ps->o.backend) {
	case BKEND_XRENDER:
		return xrender_blur(backend_data, opacity, blur_ctx, reg_blur, reg_visible);
	case BKEND_GLX:
		return gl_blur(backend_data, opacity, blur_ctx, reg_blur, reg_visible);
	default:
		return false;
	}
}
static void backend_get_blur_size(module_t *module, session_t *ps, void *blur_context, int *width, int *height) {
	UNUSED(module);

	switch (ps->o.backend) {
	case BKEND_XRENDER:
		xrender_get_blur_size(blur_context, width, height);
		break;
	case BKEND_GLX:
		gl_get_blur_size(blur_context, width, height);
		break;
	default:
		break;
	}
}

static bool initialize_blur(module_t *module, session_t *ps) {
	struct kernel_blur_args kargs;
	struct gaussian_blur_args gargs;
	struct box_blur_args bargs;

	void *args = NULL;
	switch (options.method) {
	case BLUR_METHOD_BOX:
		bargs.size = options.radius;
		args = (void *)&bargs;
		break;
	case BLUR_METHOD_KERNEL:
		kargs.kernel_count = options.kernel_count;
		kargs.kernels = options.kernels;
		args = (void *)&kargs;
		break;
	case BLUR_METHOD_GAUSSIAN:
		gargs.size = options.radius;
		gargs.deviation = options.deviation;
		args = (void *)&gargs;
		break;
	default: return true;
	}

	ps->backend_blur_context = backend_create_blur_context(module, ps,
		ps->backend_data, options.method, args);
	return ps->backend_blur_context != NULL;
}

static int backend_init(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(evid);
	UNUSED(ud);

	if (!initialize_blur(module, ps)) {
		log_fatal("Failed to prepare for background blur, aborting...");
		ps->backend_data->ops->deinit(ps->backend_data);
		ps->backend_data = NULL;
		quit(ps);
		return false;
	}

	return 0;
}
static int backend_deinit(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(evid);
	UNUSED(ud);

	if (ps->o.backend == BKEND_GLX) {
		// Free GLSL shaders/programs
		for (int i = 0; i < options.kernel_count; ++i) {
			glx_blur_pass_t *ppass = &ps->psglx->blur_passes[i];
			if (ppass->frag_shader)
				glDeleteShader(ppass->frag_shader);
			if (ppass->prog)
				glDeleteProgram(ppass->prog);
		}
		free(ps->psglx->blur_passes);
	}
	if (ps->backend_blur_context) {
		backend_destroy_blur_context(module, ps,
		     ps->backend_data, ps->backend_blur_context);
		ps->backend_blur_context = NULL;
	}

	return 0;
}
static int prepare(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(evid);
	UNUSED(module);

	struct managed_win *bottom_window = ud; // TODO: Create struct paint_pass_data to allow sharing more than just this

	// Prepare the reg_paint damage region

	// <damage-note>
	// If use_damage is enabled, we MUST make sure only the damaged regions of the
	// screen are ever touched by the compositor. The reason is that at the beginning
	// of each render, we clear the damaged regions with the wallpaper, and nothing
	// else. If later during the render we changed anything outside the damaged
	// region, that won't be cleared by the next render, and will thus accumulate.
	// (e.g. if shadow is drawn outside the damaged region, it will become thicker and
	// thicker over time.)
	//
	// Really this solution is suboptimal, we should really just calculate the actual
	// damaged region, but this is much simpler to implement and sufficient when not using
	// custom shaders and only using a single blur filter at a time.

	assert(options.method != BLUR_METHOD_INVALID);
	if (options.method != BLUR_METHOD_NONE) {
		int blur_width, blur_height;
		backend_get_blur_size(module, ps, ps->backend_blur_context,
		                                     &blur_width, &blur_height);

		// The region of screen a given window influences will be smeared
		// out by blur. With more windows on top of the given window, the
		// influences region will be smeared out more.
		//
		// Also, blurring requires data slightly outside the area that needs
		// to be blurred. The more semi-transparent windows are stacked on top
		// of each other, the larger the area will be.
		//
		// Instead of accurately calculate how much bigger the damage
		// region will be because of blur, we assume the worst case here.
		// That is, the damaged window is at the bottom of the stack, and
		// all other windows have semi-transparent background
		int resize_factor = 1;
		if (bottom_window) {
			resize_factor = bottom_window->stacking_rank;
		}
		resize_region_in_place(&ps->reg_damage, blur_width * resize_factor,
		                       blur_height * resize_factor);
		// FIXME: I am pretty sure it doesn't need to do both of these? ^ v
		ps->reg_paint = resize_region(&ps->reg_damage, blur_width * resize_factor,
		                          blur_height * resize_factor);
		pixman_region32_intersect(&ps->reg_paint, &ps->reg_paint, &ps->screen_reg);
		pixman_region32_intersect(&ps->reg_damage, &ps->reg_damage, &ps->screen_reg);
	} else {
		pixman_region32_init(&ps->reg_paint);
		pixman_region32_copy(&ps->reg_paint, &ps->reg_damage);
	}
	return 0;
}
static int blur(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(evid);
	UNUSED(module);

	struct managed_win *w = ud;
	struct window_options *winoptions = win_get_windata(w, module->windata_cookie);

	// Blur window background
	// TODO since the background might change the content of the window (e.g.
	//      with shaders), we should consult the background whether the window
	//      is transparent or not. for now we will just rely on the
	//      force_win_blend option
	auto real_win_mode = w->mode;

	if (winoptions->blur_background &&
	    (ps->o.force_win_blend || real_win_mode == WMODE_TRANS ||
	     (options.background_frame && real_win_mode == WMODE_FRAME_TRANS))) {
		// Minimize the region we try to blur, if the window
		// itself is not opaque, only the frame is.

		double blur_opacity = 1;
		if (w->state == WSTATE_MAPPING) {
			// Gradually increase the blur intensity during
			// fading in.
			blur_opacity = w->opacity * w->opacity_target;
		} else if (w->state == WSTATE_UNMAPPING ||
		           w->state == WSTATE_DESTROYING) {
			// Gradually decrease the blur intensity during
			// fading out.
			blur_opacity =
			    w->opacity * win_calc_opacity_target(ps, w, true);
		}

		pedantic_assert(blur_opacity >= 0 && blur_opacity <= 1);

		if (real_win_mode == WMODE_TRANS || ps->o.force_win_blend) {
			// We need to blur the bounding shape of the window
			// (reg_paint_in_bound = reg_bound \cap reg_paint)
			backend_blur(module, ps, 
			    ps->backend_data, blur_opacity, ps->backend_blur_context,
			    &w->reg_paint_in_bound, &ps->reg_visible);
		} else {
			// Window itself is solid, we only need to blur the frame
			// region

			// Readability assertions
			assert(options.background_frame);
			assert(real_win_mode == WMODE_FRAME_TRANS);

			region_t reg_blur = win_get_region_frame_local_by_val(w);
			pixman_region32_translate(&reg_blur, w->g.x, w->g.y);
			// make sure reg_blur \in reg_paint
			pixman_region32_intersect(&reg_blur, &reg_blur, &ps->reg_paint);
			if (ps->o.transparent_clipping) {
				// ref: <transparent-clipping-note>
				pixman_region32_intersect(&reg_blur, &reg_blur,
				                          &ps->reg_visible);
			}
			backend_blur(module, ps, ps->backend_data, blur_opacity,
			                            ps->backend_blur_context,
			                            &reg_blur, &ps->reg_visible);
			pixman_region32_fini(&reg_blur);
		}
	}
	return 0;
}
static int onexit(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(evid);
	UNUSED(module);
	UNUSED(ps);
	UNUSED(ud);

	for (int i = 0; i < options.kernel_count; ++i) {
		free(options.kernels[i]);
	}
	free(options.kernels);
	return 0;
}
static void
set_blur_background(session_t *ps, module_t *module, struct managed_win *w, bool blur_background_new) {
	struct window_options *winoptions = win_get_windata(w, module->windata_cookie);

	if (winoptions->blur_background == blur_background_new)
		return;

	winoptions->blur_background = blur_background_new;

	// This damage might not be absolutely necessary (e.g. when the window is opaque),
	// but blur_background changes should be rare, so this should be fine.
	add_damage_from_win(ps, w);
}

/**
 * Determine if a window should have background blurred.
 */
static void determine_blur_background(session_t *ps, module_t *module, struct managed_win *w) {
	if (w->a.map_state != XCB_MAP_STATE_VIEWABLE)
		return;

	auto blur_method = options.method;
	c2_lptr_t *blur_background_blacklist = options.background_blacklist;
	bool blur_background_new =
	    blur_method && !c2_match(ps, w, blur_background_blacklist, NULL);

	set_blur_background(ps, module, w, blur_background_new);
}
static int onwinchanged(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(evid);
	UNUSED(module);

	struct managed_win *w = ud;

	c2_lptr_t *blur_background_blacklist = options.background_blacklist;
	if (blur_background_blacklist)
		determine_blur_background(ps, module, w);
	return 0;
}
static int onwinmapped(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(evid);
	UNUSED(module);

	struct managed_win *w = ud;

	determine_blur_background(ps, module, w);

	return 0;
}
static int load(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(module);
	UNUSED(ud);

	module->options = &options;
#define OPTION(...) MODULE_ADD_OPTION(prop, &module->cfg_module, &options, __VA_ARGS__)
#include "cfg_mod.h"
#undef OPTION
#define OPTION(...) MODULE_SET_OPTION_DEFAULT(prop, &module->cfg_module, &options, __VA_ARGS__)
#include "cfg_mod.h"
#undef OPTION

	module_subscribe(module, MODEV_STAGE_PAINT_PREPARE, prepare);
	module_subscribe(module, MODEV_STAGE_WIN_BLUR, blur);
	module_subscribe(module, MODEV_BACKEND_CREATE_START, backend_init);
	module_subscribe(module, MODEV_BACKEND_DESTROY_START, backend_deinit);
	module_subscribe(module, MODEV_EXIT, onexit);
	module_subscribe(module, MODEV_WIN_CHANGED, onwinchanged);
	module_subscribe(module, MODEV_WIN_MAPPED, onwinmapped);

	return 0;
}
modinfo_t modinfo_blur = {
	.name = "blur",
	.load = load,
	.unload = NULL,
};
