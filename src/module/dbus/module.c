#define MODULE_NAME dbus
#include "module.h"
#include "common.h"
#ifdef CONFIG_DBUS
#include "dbus.h"
#endif

static int MODULE(init)(modev_t evid, module_t *module, session_t *ps, void *ud)
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
static int MODULE(exit)(modev_t evid, module_t *module, session_t *ps, void *ud)
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
static int MODULE(win_added)(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_added(ps, ud);
	}

	return 0;
}
static int MODULE(win_focus)(modev_t evid, module_t *module, session_t *ps, void *ud)
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
static int MODULE(win_destroyed)(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_destroyed(ps, ud);
	}

	return 0;
}
static int MODULE(win_unmapped)(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_unmapped(ps, ud);
	}

	return 0;
}
static int MODULE(win_mapped)(modev_t evid, module_t *module, session_t *ps, void *ud)
{
	UNUSED(evid);
	UNUSED(module);

	if (ps->o.dbus) {
		cdbus_ev_win_mapped(ps, ud);
	}

	return 0;
}
#endif

int loadmod_dbus(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);
	module->name = "dbus";
	module->modev_cb[MODEV_EARLY_INIT] = MODULE(init);
#ifdef CONFIG_DBUS
	module->modev_cb[MODEV_EXIT] = MODULE(exit);
	module->modev_cb[MODEV_WIN_ADDED] = MODULE(win_added);
	module->modev_cb[MODEV_WIN_FOCUSIN] = MODULE(win_focus);
	module->modev_cb[MODEV_WIN_FOCUSOUT] = MODULE(win_focus);
	module->modev_cb[MODEV_WIN_DESTROYED] = MODULE(win_destroyed);
	module->modev_cb[MODEV_WIN_UNMAPPED] = MODULE(win_unmapped);
	module->modev_cb[MODEV_WIN_MAPPED] = MODULE(win_mapped);
#endif
	return 0;
}
#undef MODULE_NAME
