#pragma once
#include "common.h"

// XXX It would probably make sense to expose this as part of the public API,
// if we ever get one

#include "utils/meta.h"
#include "utils/cfg.h"
#include "utils/utils.h"

#include "win.h"

typedef int sesdata_cookie_t; // TODO
typedef struct session session_t;

/// List of stages that modules may run in, in order of execution
enum module_stage {
	STAGE_PREPARE = 0,
	STAGE_DECORATE = 100,
	STAGE_BLUR = 300,
	STAGE_SHADOW = 400,
	STAGE_SHADE = 200,
	STAGE_COMPOSE = 500,
} modstage_t;

/// Events emited by picom
typedef enum module_event_id {
	MODEV_EARLY_INIT,
	MODEV_INIT,
	MODEV_EARLY_EXIT,
	MODEV_EXIT,

	MODEV_STAGE_PAINT_START,
	MODEV_STAGE_PAINT_PREPARE,

	/* struct managed_window *ud */
	MODEV_STAGE_WIN_PREPARE,
	MODEV_STAGE_WIN_DECORATE,
	MODEV_STAGE_WIN_BLUR,
	MODEV_STAGE_WIN_SHADOW,
	MODEV_STAGE_WIN_SHADE,
	MODEV_STAGE_WIN_COMPOSE,

	/* ??? */
	MODEV_STAGE_SCREEN_PREPARE,
	MODEV_STAGE_SCREEN_DECORATE,
	MODEV_STAGE_SCREEN_BLUR,
	MODEV_STAGE_SCREEN_SHADOW,
	MODEV_STAGE_SCREEN_SHADE,
	MODEV_STAGE_SCREEN_COMPOSE,
	MODEV_STAGE_SCREEN_CLEANUP,

	MODEV_STAGE_PAINT_CLEANUP,
	MODEV_STAGE_PAINT_DONE,

	 /* struct managed_window *ud */
	MODEV_WIN_ADDED,
	MODEV_WIN_FOCUSIN,
	MODEV_WIN_FOCUSOUT,
	MODEV_WIN_DESTROYING,
	MODEV_WIN_DESTROYED,
	MODEV_WIN_UNMAPPED,
	MODEV_WIN_MAPPED,
	MODEV_WIN_CHANGED,

	MODEV_SCREEN_REDIRECT_START,
	MODEV_SCREEN_REDIRECT_DONE,

	MODEV_SCREEN_UNREDIRECT_START,
	MODEV_SCREEN_UNREDIRECT_DONE,

	MODEV_BACKEND_CREATE_START,
	MODEV_BACKEND_CREATE_DONE,

	MODEV_BACKEND_DESTROY_START,
	MODEV_BACKEND_DESTROY_DONE,

	NUM_MODEVENTS
} modev_t;

/// Opaque module type
typedef struct module module_t;
/// Event Handler
typedef int (*modev_cb_t)(modev_t evid, module_t *module, session_t *ps, void *ud);
/// Generic Module Function
typedef int (*modcb_t)(session_t *ps, module_t *module, void *ud);
typedef struct module_info {
	/// Name of the module (used as config namespace)
	const char *name;
	/// Loader to load the module with
	modcb_t load;
	/// Unloader to call at program exit
	modcb_t unload;
} modinfo_t;

typedef struct session session_t;

struct module {
	modinfo_t info;
	/// Registered module-local/session-local variables
	/// They are stored in the module->options pointer
	cfg_t cfg_module;
	/// Registered window-local variables
	/// They are stored in the reserved windata area
	cfg_t cfg_window;
	/// Pointer to struct with configuration variables for this module
	void *options;
	// ===========    Private Parts     ===========
	/// Handle to the shared object associated with this module (if any)
	void *handle;
	/// Array of event handlers for this module
	// FIXME: This wastes a lot of space.. Turn this into a hashmap.
	modev_cb_t modev_cb[NUM_MODEVENTS];
	/// Cookie allocated by this module, or -1
	windata_cookie_t windata_cookie;
	/// Reserved spaces for the cookie, in bytes
	size_t windata_reserved;
};

/// Load a module
/// A module may stay loaded between sessions, meaning it's load handler
/// won't be called again, so proper initialization should be done in a
/// MODEV_INIT event handler.
module_t *module_load(session_t *ps, modinfo_t *modinfo, void *ud);
void module_unload(session_t *ps, module_t *module, void *ud);

void module_subscribe(module_t *module, modev_t evid, modev_cb_t cb);
void module_unsubscribe(module_t *module, modev_t evid);

int module_emit(modev_t evid, session_t *ps, void *ud);
/// Reserves extra buffer space for your module in each window.
/// The windata_cookie_t returned by this function can be used to obtain a pointer to its value
/// If the window data cannot be created, or if it already exists, returns -1
windata_cookie_t module_reserve_windowdata(session_t *ps, module_t *module, size_t reserve);
sesdata_cookie_t module_reserve_sessiondata(session_t *ps, module_t *module, size_t reserve);

#define MODULE_DECLARE_OPTION(TYPE, NAME, CFGTYPE, DEFAULT) TYPE NAME;
#define MODULE_DECLARE_PROPERTY(TYPE, NAME, CFGTYPE, DEFAULT) cfg_prop_t NAME;

#define MODULE_ADD_OPTION(PROPS, CFG, OBJ, TYPE, NAME, CFGTYPE, DEFAULT) \
	PROPS.NAME = cfg_addprop(CFG, STRINGIFY(NAME), &(CFGTYPE), offsetof(__typeof__(*(OBJ)), NAME));
#define MODULE_SET_OPTION_DEFAULT(PROPS, CFG, OBJ, TYPE, NAME, CFGTYPE, DEFAULT) \
	UNUSED(cfg_set(CFG, OBJ, PROPS.NAME, (CFGTYPE).repr, &(TYPE){ DEFAULT }));

#define DEFINE_BASICTYPE(LNAME, UNAME, VALUE, TYPE, DEFAULT, SIZE) \
	static inline bool module_set##LNAME(const module_t *self, cfg_prop_t prop, TYPE value) { \
		return cfg_set##LNAME(&self->cfg_module, self->options, prop, value); \
	} \
	static inline const TYPE *module_get##LNAME(const module_t *self, cfg_prop_t prop) { \
		return cfg_get##LNAME(&self->cfg_module, self->options, prop); \
	} \
	static inline TYPE module_get##LNAME##_def(const module_t *self, cfg_prop_t prop) { \
		return cfg_get##LNAME##_def(&self->cfg_module, self->options, prop); \
	}
#ifndef NDEBUG
/* UNSAFE! For debugging only! */
#define DEFINE_BASICTYPE_EXTRA(LNAME, UNAME, VALUE, TYPE, DEFAULT, SIZE) \
	static inline bool module_xset##LNAME(const module_t *self, const char *key, TYPE value) { \
		cfg_prop_t prop = cfg_getprop(&self->cfg_module, key); \
		return module_set##LNAME(self, prop, value); \
	} \
	static inline const TYPE *module_xget##LNAME(const module_t *self, const char *key) { \
		cfg_prop_t prop = cfg_getprop(&self->cfg_module, key); \
		return module_get##LNAME(self, prop); \
	} \
	static inline TYPE module_xget##LNAME##_def(const module_t *self, const char *key) { \
		cfg_prop_t prop = cfg_getprop(&self->cfg_module, key); \
		return module_get##LNAME##_def(self, prop); \
	}
#endif
DEFINE_BASICTYPES(DEFINE_BASICTYPE)
DEFINE_BASICTYPES(DEFINE_BASICTYPE_EXTRA)
#undef DEFINE_BASICTYPE