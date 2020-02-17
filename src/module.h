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
	/// Registered module-local variables
	cfg_t cfg_module;
	/// Registered window-local variables
	cfg_t cfg_window;
	/// Registered session-local variables
	cfg_t cfg_session;
	/// Pointer to struct with configuration variables for this module
	void *options;
	// ===========    Private Parts     ===========
	/// Handle to the shared object associated with this module (if any)
	void *handle;
	/// Array of event handlers for this module
	modev_cb_t modev_cb[NUM_MODEVENTS];
	/// Cookie allocated by this module, or -1
	windata_cookie_t windata_cookie;
	/// Reserved spaces for the cookie, in bytes
	size_t windata_reserved;
};

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
#define MODULE_DECLARE_OPTIONS(OPTIONS) \
	struct module_options { \
		OPTIONS(MODULE_DECLARE_OPTION) \
	} options; \
	struct module_properties { \
		OPTIONS(MODULE_DECLARE_PROPERTY) \
	} prop

#define MODULE_ADD_OPTION(TYPE, NAME, CFGTYPE, DEFAULT) \
	prop.NAME = cfg_addprop(&module->cfg_module, STRINGIFY(NAME), &CFGTYPE, offsetof(struct module_options, NAME));
#define MODULE_ADD_OPTIONS(OPTIONS) \
	do { \
		module->options = &options; \
		OPTIONS(MODULE_ADD_OPTION) \
	} while (0)
