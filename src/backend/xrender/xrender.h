#pragma once
#include <xcb/composite.h>
#include <xcb/present.h>
#include <xcb/render.h>
#include <xcb/sync.h>
#include <xcb/xcb.h>

#include "backend/backend.h"

typedef struct _xrender_data {
	backend_t base;
	/// If vsync is enabled and supported by the current system
	bool vsync;
	xcb_visualid_t default_visual;
	/// Target window
	xcb_window_t target_win;
	/// Painting target, it is either the root or the overlay
	xcb_render_picture_t target;
	/// Back buffers. Double buffer, with 1 for temporary render use
	xcb_render_picture_t back[3];
	/// The back buffer that is for temporary use
	/// Age of each back buffer.
	int buffer_age[3];
	/// The back buffer we should be painting into
	int curr_back;
	/// The corresponding pixmap to the back buffer
	xcb_pixmap_t back_pixmap[3];
	/// The original root window content, usually the wallpaper.
	/// We save it so we don't loss the wallpaper when we paint over
	/// it.
	xcb_render_picture_t root_pict;
	/// Pictures of pixel of different alpha value, used as a mask to
	/// paint transparent images
	xcb_render_picture_t alpha_pict[256];

	// XXX don't know if these are really needed

	/// 1x1 white picture
	xcb_render_picture_t white_pixel;
	/// 1x1 black picture
	xcb_render_picture_t black_pixel;

	/// Width and height of the target pixmap
	int target_width, target_height;

	xcb_special_event_t *present_event;
} xrender_data;
