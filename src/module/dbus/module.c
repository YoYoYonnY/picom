#include "module.h"
#include "common.h"
#ifdef CONFIG_DBUS
#include "dbus.h"
#endif

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

// XXX Still deciding on whether having multiple instances of the same module loaded makes sense
// For now, we just use a single global options struct shared by all instances
// This will of course break, so currently, trying to load the same module twice doesn't work at all
// If we decide we don't need support for multiple module instances we can just inline this, if we want
static inline bool is_module_enabled(module_t *module)
{
	UNUSED(module);
	return options.enabled;
}
static inline cdbus_session_t *get_cdbus_session(module_t *module)
{
	UNUSED(module);
	return options.session;
}

static int oninit(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(ud);

	if (!is_module_enabled(module)) return 1;

#ifdef CONFIG_DBUS
	cdbus_session_t *session = cdbus_create(ps, DisplayString(ps->dpy));
	if (session) {
		cfg_setpointer(&module->cfg_module, module->options, prop.session, session);
	} else {
		cfg_setbool(&module->cfg_module, module->options, prop.enabled, false);
	}
#else
	log_fatal("DBus support not compiled in!");
	exit(1);
#endif

	return 0;
}
#ifdef CONFIG_DBUS
static int onexit(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(ps);
	UNUSED(ud);

	if (!is_module_enabled(module)) return 1;

	cdbus_session_t *session = get_cdbus_session(module);

	assert(session);
	cdbus_destroy(session);

	return 0;
}
static int onwin_added(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(ps);

	if (!is_module_enabled(module)) return 1;

	cdbus_session_t *session = get_cdbus_session(module);

	cdbus_ev_win_added(session, ud);

	return 0;
}
static int onwin_focus(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(ps);

	if (!is_module_enabled(module)) return 1;

	cdbus_session_t *session = get_cdbus_session(module);

	if (evid == MODEV_WIN_FOCUSIN) {
		cdbus_ev_win_focusin(session, ud);
	} else {
		cdbus_ev_win_focusout(session, ud);
	}

	return 0;
}
static int onwin_destroyed(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(ps);

	if (!is_module_enabled(module)) return 1;

	cdbus_session_t *session = get_cdbus_session(module);

	cdbus_ev_win_destroyed(session, ud);

	return 0;
}
static int onwin_unmapped(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(ps);

	if (!is_module_enabled(module)) return 1;

	cdbus_session_t *session = get_cdbus_session(module);

	cdbus_ev_win_unmapped(session, ud);

	return 0;
}
static int onwin_mapped(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(ps);

	if (!is_module_enabled(module)) return 1;

	cdbus_session_t *session = get_cdbus_session(module);

	cdbus_ev_win_mapped(session, ud);

	return 0;
}
#endif

static int load(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);

	module->options = &options;
#define OPTION(...) MODULE_ADD_OPTION(prop, &module->cfg_module, &options, __VA_ARGS__)
#include "cfg_mod.h"
#undef OPTION
#define OPTION(...) MODULE_SET_OPTION_DEFAULT(prop, &module->cfg_module, &options, __VA_ARGS__)
#include "cfg_mod.h"
#undef OPTION

	module_subscribe(module, MODEV_EARLY_INIT, oninit);
#ifdef CONFIG_DBUS
	module_subscribe(module, MODEV_EXIT, onexit);
	module_subscribe(module, MODEV_WIN_ADDED, onwin_added);
	module_subscribe(module, MODEV_WIN_FOCUSIN, onwin_focus);
	module_subscribe(module, MODEV_WIN_FOCUSOUT, onwin_focus);
	module_subscribe(module, MODEV_WIN_DESTROYED, onwin_destroyed);
	module_subscribe(module, MODEV_WIN_UNMAPPED, onwin_unmapped);
	module_subscribe(module, MODEV_WIN_MAPPED, onwin_mapped);
#endif
	return 0;
}
modinfo_t modinfo_dbus = {
	.name = "dbus",
	.load = load,
	.unload = NULL,
};
