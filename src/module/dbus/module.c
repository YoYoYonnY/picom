#define MODULE_NAME dbus
#include "module.h"

int loadmod_dbus(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);
	module->name = "dbus";
	return 0;
}
#undef MODULE_NAME
