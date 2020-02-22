#include "module.h"
#include "compton-compat/common.h"

static int on_backend_create(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(module);
	UNUSED(evid);
	UNUSED(ud);

	if (ps->o.experimental_backends) {
		assert(!ps->backend_data);
		// Reinitialize win_data
		assert(backend_list[ps->o.backend]);
		ps->backend_data = backend_list[ps->o.backend]->init(ps);
		if (!ps->backend_data) {
			log_fatal("Failed to initialize backend, aborting...");
			quit(ps);
			return 1;
		}
		ps->backend_data->ops = backend_list[ps->o.backend];

		// window_stack shouldn't include window that's
		// not in the hash table at this point. Since
		// there cannot be any fading windows.
		HASH_ITER2(ps->windows, _w) {
			if (!_w->managed) {
				continue;
			}
			auto w = (struct managed_win *)_w;
			assert(w->state == WSTATE_MAPPED || w->state == WSTATE_UNMAPPED);
			if (w->state == WSTATE_MAPPED) {
				// We need to reacquire image
				log_debug("Marking window %#010x (%s) for update after "
				          "redirection",
				          w->base.id, w->name);
				if (w->shadow) {
					struct color c = {
					    .red = ps->o.shadow_red,
					    .green = ps->o.shadow_green,
					    .blue = ps->o.shadow_blue,
					    .alpha = ps->o.shadow_opacity,
					};
					win_bind_shadow(ps->backend_data, w, c,
					                ps->gaussian_map);
				}

				w->flags |= WIN_FLAGS_PIXMAP_STALE;
				ps->pending_updates = true;
			}
		}
	}
	// The old backends binds pixmap lazily, nothing to do here
	return 0;
}
static int on_backend_destroy(modev_t evid, module_t *module, session_t *ps, void *ud) {
	UNUSED(module);
	UNUSED(evid);
	UNUSED(ud);

	win_stack_foreach_managed_safe(w, &ps->window_stack) {
		// Wrapping up fading in progress
		if (win_skip_fading(ps, w)) {
			// `w` is freed by win_skip_fading
			continue;
		}

		if (ps->backend_data) {
			if (w->state == WSTATE_MAPPED) {
				win_release_images(ps->backend_data, w);
			} else {
				assert(!w->win_image);
				assert(!w->shadow_image);
			}
		}
		free_paint(ps, &w->paint);
	}

	if (ps->backend_data && ps->root_image) {
		ps->backend_data->ops->release_image(ps->backend_data, ps->root_image);
		ps->root_image = NULL;
	}

	if (ps->backend_data) {
		ps->backend_data->ops->deinit(ps->backend_data);
		ps->backend_data = NULL;
	}
	return 0;
}

static int load(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);

	module_subscribe(module, MODEV_BACKEND_CREATE_START, on_backend_create);
	module_subscribe(module, MODEV_BACKEND_DESTROY_START, on_backend_destroy);

	return 0;
}
modinfo_t modinfo_backend = {
	.name = "backend",
	.load = load,
	.unload = NULL,
};
