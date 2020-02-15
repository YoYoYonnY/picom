#ifndef MODULE_H
#define MODULE_H
#include "meta.h"
#include "utils.h"

typedef struct session session_t;

/// List of stages that modules may run in, in order of execution
enum modstage {
	STAGE_DECORATION = 10, // First, add decoration (ex: borders)
	STAGE_SHADER = 20, // Then, execute our custom shaders (ex: invert colors)
	STAGE_TRANSPARENCY = 30, // Apply transparency (do this later so decoration can be transparent too)
	STAGE_BLUR = 40, // Perform blurring last
} modstage_t;

/// Events emited by picom
typedef enum {
	MODEV_INIT,
	MODEV_EARLY_INIT,
	MODEV_EXIT,
	MODEV_WIN_ADDED,
	MODEV_WIN_FOCUSIN,
	MODEV_WIN_FOCUSOUT,
	MODEV_WIN_DESTROYED,
	MODEV_WIN_UNMAPPED,
	MODEV_WIN_MAPPED,
	NUM_MODEVENTS
} modev_t;

typedef struct module module_t;
typedef void (*modev_cb_t)(modev_t evid, session_t *ps, void *ud);
typedef int (*modloader_cb_t)(session_t *ps, module_t *pm, void *ud);

struct module {
	/// Name of the module (used as config namespace)
	const char *name;
	/// Loader the module was loaded with
	modloader_cb_t loader;
	/// Array of event handlers for this module
	modev_cb_t modev_cb[NUM_MODEVENTS];
};

void module_emit(modev_t evid, session_t *ps, void *ud);
void module_load(session_t *ps, modloader_cb_t loader, void *ud);

#ifdef MODULE_NAME
/// Every module should define a function `loadmod_${NAME}`, and fill the
/// module structure it obtains as argument
extern int CONCAT(loadmod_, MODULE_NAME)(session_t *ps, module_t *module, void *ud);
#endif
#endif /* MODULE_H */

#ifdef MODULE_NAME
#ifndef MODULE
// Macro function for name-mangling
#  define MODULE(name) CONCAT4(mod, MODULE_NAME, _, name)
#endif
#endif
