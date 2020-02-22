// SPDX-License-Identifier: MPL-2.0
// Copyright (c) Yuxuan Shui <yshuiv7@gmail.com>
#include <xcb/sync.h>
#include <xcb/xcb.h>

#include "utils/compiler.h"

#include "backend/backend.h"
#include "common.h"
#include "config.h"
#include "log.h"
#include "region.h"
#include "types.h"
#include "win.h"
#include "x.h"

extern struct backend_operations xrender_ops, dummy_ops;
#ifdef CONFIG_OPENGL
extern struct backend_operations glx_ops;
#endif

struct backend_operations *backend_list[NUM_BKEND] = {
    [BKEND_XRENDER] = &xrender_ops,
    [BKEND_DUMMY] = &dummy_ops,
#ifdef CONFIG_OPENGL
    [BKEND_GLX] = &glx_ops,
#endif
};

/**
 * @param all_damage if true ignore damage and repaint the whole screen
 */
region_t get_damage(session_t *ps, bool all_damage) {
	region_t region;
	auto buffer_age_fn = ps->backend_data->ops->buffer_age;
	int buffer_age = buffer_age_fn ? buffer_age_fn(ps->backend_data) : -1;

	if (all_damage) {
		buffer_age = -1;
	}

	pixman_region32_init(&region);
	if (buffer_age == -1 || buffer_age > ps->ndamage) {
		pixman_region32_copy(&region, &ps->screen_reg);
	} else {
		for (int i = 0; i < buffer_age; i++) {
			auto curr = ((ps->damage - ps->damage_ring) + i) % ps->ndamage;
			log_trace("damage index: %d, damage ring offset: %ld", i, curr);
			dump_region(&ps->damage_ring[curr]);
			pixman_region32_union(&region, &region, &ps->damage_ring[curr]);
		}
		pixman_region32_intersect(&region, &region, &ps->screen_reg);
	}
	return region;
}

/// paint all windows
void paint_all_new(session_t *ps, struct managed_win *t, bool ignore_damage) {
	if (ps->o.xrender_sync_fence || (ps->drivers & DRIVER_NVIDIA)) {
		if (ps->xsync_exists && !x_fence_sync(ps->c, ps->sync_fence)) {
			log_error("x_fence_sync failed, xrender-sync-fence will be "
			          "disabled from now on.");
			xcb_sync_destroy_fence(ps->c, ps->sync_fence);
			ps->sync_fence = XCB_NONE;
			ps->o.xrender_sync_fence = false;
			ps->xsync_exists = false;
		}
	}
	// All painting will be limited to the damage, if _some_ of
	// the paints bleed out of the damage region, it will destroy
	// part of the image we want to reuse
	if (!ignore_damage) {
		ps->reg_damage = get_damage(ps, ps->o.monitor_repaint || !ps->o.use_damage);
	} else {
		pixman_region32_init(&ps->reg_damage);
		pixman_region32_copy(&ps->reg_damage, &ps->screen_reg);
	}

	if (!pixman_region32_not_empty(&ps->reg_damage)) {
		pixman_region32_fini(&ps->reg_damage);
		return;
	}

#ifdef DEBUG_REPAINT
	static struct timespec last_paint = {0};
#endif

	module_emit(MODEV_STAGE_PAINT_PREPARE, ps, t);

	pixman_region32_init(&ps->reg_visible);
	pixman_region32_copy(&ps->reg_visible, &ps->screen_reg);
	if (t && !ps->o.transparent_clipping) {
		// Calculate the region upon which the root window (wallpaper) is to be
		// painted based on the ignore region of the lowest window, if available
		//
		// NOTE If transparent_clipping is enabled, transparent windows are
		// included in the reg_ignore, but we still want to have the wallpaper
		// beneath them, so we don't use reg_ignore for wallpaper in that case.
		pixman_region32_subtract(&ps->reg_visible, &ps->reg_visible, t->reg_ignore);
	}

	// TODO Bind root pixmap

	if (ps->backend_data->ops->prepare) {
		ps->backend_data->ops->prepare(ps->backend_data, &ps->reg_paint);
	}

	if (ps->root_image) {
		ps->backend_data->ops->compose(ps->backend_data, ps->root_image, 0, 0,
		                               &ps->reg_paint, &ps->reg_visible);
	}

	// Windows are sorted from bottom to top
	// Each window has a reg_ignore, which is the region obscured by all the windows
	// on top of that window. This is used to reduce the number of pixels painted.
	//
	// Whether this is beneficial is to be determined XXX
	for (auto w = t; w; w = w->prev_trans) {
		pixman_region32_subtract(&ps->reg_visible, &ps->screen_reg, w->reg_ignore);
		assert(!(w->flags & WIN_FLAGS_IMAGE_ERROR));
		assert(!(w->flags & WIN_FLAGS_PIXMAP_STALE));
		assert(!(w->flags & WIN_FLAGS_PIXMAP_NONE));

		// The bounding shape of the window, in global/target coordinates
		// reminder: bounding shape contains the WM frame
		w->reg_bound = win_get_bounding_shape_global_by_val(w);

		// The clip region for the current window, in global/target coordinates
		// reg_paint_in_bound \in reg_paint
		pixman_region32_init(&w->reg_paint_in_bound);
		pixman_region32_intersect(&w->reg_paint_in_bound, &w->reg_bound, &ps->reg_paint);
		if (ps->o.transparent_clipping) {
			// <transparent-clipping-note>
			// If transparent_clipping is enabled, we need to be SURE that
			// things are not drawn inside reg_ignore, because otherwise they
			// will appear underneath transparent windows.
			// So here we have make sure reg_paint_in_bound \in reg_visible
			// There are a few other places below where this is needed as
			// well.
			pixman_region32_intersect(&w->reg_paint_in_bound,
			                          &w->reg_paint_in_bound, &ps->reg_visible);
		}

		module_emit(MODEV_STAGE_WIN_BLUR, ps, w);

		// Draw shadow on target
		if (w->shadow) {
			assert(!(w->flags & WIN_FLAGS_SHADOW_NONE));
			// Clip region for the shadow
			// reg_shadow \in reg_paint
			auto reg_shadow = win_extents_by_val(w);
			pixman_region32_intersect(&reg_shadow, &reg_shadow, &ps->reg_paint);
			if (!ps->o.wintype_option[w->window_type].full_shadow) {
				pixman_region32_subtract(&reg_shadow, &reg_shadow, &w->reg_bound);
			}

			// Mask out the region we don't want shadow on
			if (pixman_region32_not_empty(&ps->shadow_exclude_reg)) {
				pixman_region32_subtract(&reg_shadow, &reg_shadow,
				                         &ps->shadow_exclude_reg);
			}

			if (ps->o.xinerama_shadow_crop && w->xinerama_scr >= 0 &&
			    w->xinerama_scr < ps->xinerama_nscrs) {
				// There can be a window where number of screens is
				// updated, but the screen number attached to the windows
				// have not.
				//
				// Window screen number will be updated eventually, so
				// here we just check to make sure we don't access out of
				// bounds.
				pixman_region32_intersect(
				    &reg_shadow, &reg_shadow,
				    &ps->xinerama_scr_regs[w->xinerama_scr]);
			}

			if (ps->o.transparent_clipping) {
				// ref: <transparent-clipping-note>
				pixman_region32_intersect(&reg_shadow, &reg_shadow,
				                          &ps->reg_visible);
			}

			assert(w->shadow_image);
			if (w->opacity == 1) {
				ps->backend_data->ops->compose(
				    ps->backend_data, w->shadow_image, w->g.x + w->shadow_dx,
				    w->g.y + w->shadow_dy, &reg_shadow, &ps->reg_visible);
			} else {
				auto new_img = ps->backend_data->ops->copy(
				    ps->backend_data, w->shadow_image, &ps->reg_visible);
				ps->backend_data->ops->image_op(
				    ps->backend_data, IMAGE_OP_APPLY_ALPHA_ALL, new_img,
				    NULL, &ps->reg_visible, (double[]){w->opacity});
				ps->backend_data->ops->compose(
				    ps->backend_data, new_img, w->g.x + w->shadow_dx,
				    w->g.y + w->shadow_dy, &reg_shadow, &ps->reg_visible);
				ps->backend_data->ops->release_image(ps->backend_data, new_img);
			}
			pixman_region32_fini(&reg_shadow);
		}

		// Set max brightness
		if (ps->o.max_brightness < 1.0) {
			ps->backend_data->ops->image_op(
			    ps->backend_data, IMAGE_OP_MAX_BRIGHTNESS, w->win_image, NULL,
			    &ps->reg_visible, &ps->o.max_brightness);
		}

		// Draw window on target
		if (!w->invert_color && !w->dim && w->frame_opacity == 1 && w->opacity == 1) {
			ps->backend_data->ops->compose(ps->backend_data, w->win_image,
			                               w->g.x, w->g.y,
			                               &w->reg_paint_in_bound, &ps->reg_visible);
		} else if (w->opacity * MAX_ALPHA >= 1) {
			// We don't need to paint the window body itself if it's
			// completely transparent.

			// For window image processing, we don't have to limit the process
			// region to damage for correctness. (see <damager-note> for
			// details)

			// The bounding shape, in window local coordinates
			region_t reg_bound_local;
			pixman_region32_init(&reg_bound_local);
			pixman_region32_copy(&reg_bound_local, &w->reg_bound);
			pixman_region32_translate(&reg_bound_local, -w->g.x, -w->g.y);

			// The visible region, in window local coordinates
			// Although we don't limit process region to damage, we provide
			// that info in reg_visible as a hint. Since window image data
			// outside of the damage region won't be painted onto target
			region_t reg_visible_local;
			pixman_region32_init(&reg_visible_local);
			pixman_region32_intersect(&reg_visible_local, &ps->reg_visible, &ps->reg_paint);
			pixman_region32_translate(&reg_visible_local, -w->g.x, -w->g.y);
			// Data outside of the bounding shape won't be visible, but it is
			// not necessary to limit the image operations to the bounding
			// shape yet. So pass that as the visible region, not the clip
			// region.
			pixman_region32_intersect(&reg_visible_local, &reg_visible_local,
			                          &reg_bound_local);

			auto new_img = ps->backend_data->ops->copy(
			    ps->backend_data, w->win_image, &reg_visible_local);
			if (w->invert_color) {
				ps->backend_data->ops->image_op(
				    ps->backend_data, IMAGE_OP_INVERT_COLOR_ALL, new_img,
				    NULL, &reg_visible_local, NULL);
			}
			if (w->dim) {
				double dim_opacity = ps->o.inactive_dim;
				if (!ps->o.inactive_dim_fixed) {
					dim_opacity *= w->opacity;
				}
				ps->backend_data->ops->image_op(
				    ps->backend_data, IMAGE_OP_DIM_ALL, new_img, NULL,
				    &reg_visible_local, (double[]){dim_opacity});
			}
			if (w->frame_opacity != 1) {
				auto reg_frame = win_get_region_frame_local_by_val(w);
				ps->backend_data->ops->image_op(
				    ps->backend_data, IMAGE_OP_APPLY_ALPHA, new_img, &reg_frame,
				    &reg_visible_local, (double[]){w->frame_opacity});
				pixman_region32_fini(&reg_frame);
			}
			if (w->opacity != 1) {
				ps->backend_data->ops->image_op(
				    ps->backend_data, IMAGE_OP_APPLY_ALPHA_ALL, new_img,
				    NULL, &reg_visible_local, (double[]){w->opacity});
			}
			ps->backend_data->ops->compose(ps->backend_data, new_img, w->g.x,
			                               w->g.y, &w->reg_paint_in_bound,
			                               &ps->reg_visible);
			ps->backend_data->ops->release_image(ps->backend_data, new_img);
			pixman_region32_fini(&reg_visible_local);
			pixman_region32_fini(&reg_bound_local);
		}
		pixman_region32_fini(&w->reg_bound);
		pixman_region32_fini(&w->reg_paint_in_bound);
	}
	pixman_region32_fini(&ps->reg_paint);

	if (ps->o.monitor_repaint) {
		auto reg_damage_debug = get_damage(ps, false);
		ps->backend_data->ops->fill(
		    ps->backend_data, (struct color){0.5, 0, 0, 0.5}, &reg_damage_debug);
		pixman_region32_fini(&reg_damage_debug);
	}

	// Move the head of the damage ring
	ps->damage = ps->damage - 1;
	if (ps->damage < ps->damage_ring) {
		ps->damage = ps->damage_ring + ps->ndamage - 1;
	}
	pixman_region32_clear(ps->damage);

	if (ps->backend_data->ops->present) {
		// Present the rendered scene
		// Vsync is done here
		ps->backend_data->ops->present(ps->backend_data, &ps->reg_damage);
	}

	pixman_region32_fini(&ps->reg_damage);

#ifdef DEBUG_REPAINT
	struct timespec now = get_time_timespec();
	struct timespec diff = {0};
	timespec_subtract(&diff, &now, &last_paint);
	log_trace("[ %5ld:%09ld ] ", diff.tv_sec, diff.tv_nsec);
	last_paint = now;
	log_trace("paint:");
	for (win *w = t; w; w = w->prev_trans)
		log_trace(" %#010lx", w->id);
#endif
}

// vim: set noet sw=8 ts=8 :
