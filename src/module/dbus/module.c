#include "module.h"
#include "common.h"
#ifdef CONFIG_DBUS
#include "dbus.h"
#endif

static int oninit(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);
	UNUSED(ud);

	if (ps->o.dbus) {
#ifdef CONFIG_DBUS
		cdbus_init(ps, DisplayString(ps->dpy));
		if (!ps->dbus_data) {
			ps->o.dbus = false;
		}
#else
		log_fatal("DBus support not compiled in!");
		exit(1);
#endif
	}

	return 0;
}
#ifdef CONFIG_DBUS
static int onexit(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);
	UNUSED(ud);

	if (ps->o.dbus) {
		assert(ps->dbus_data);
		cdbus_destroy(ps);
	}

	return 0;
}
static int onwin_added(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_added(ps, ud);
	}

	return 0;
}
static int onwin_focus(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		if (evid == MODEV_WIN_FOCUSIN) {
			cdbus_ev_win_focusin(ps, ud);
		} else {
			cdbus_ev_win_focusout(ps, ud);
		}
	}

	return 0;
}
static int onwin_destroyed(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_destroyed(ps, ud);
	}

	return 0;
}
static int onwin_unmapped(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_unmapped(ps, ud);
	}

	return 0;
}
static int onwin_mapped(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_mapped(ps, ud);
	}

	return 0;
}
#endif

static int load(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);
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
