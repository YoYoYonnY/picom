#include "common.h"
#include "compiler.h"
#include "utils.h"
#include "module.h"

int module_emit(modev_t evid, session_t *ps, void *ud)
{
	int nemitted = 0;
	for (size_t i=0; i < ps->num_modules; i++) {
		module_t *module = &ps->modules[i];
		modev_cb_t cb = module->modev_cb[evid];
		if (cb) {
			cb(evid, module, ps, ud);
			nemitted++;
		}
	}
	return nemitted;
}
int module_load(session_t *ps, modloader_cb_t loader, void *ud)
{
	static const module_t module_def = {
		.name = NULL,
		.loader = NULL,
		.modev_cb = {0},
		.windata_cookie = -1,
	};
	UNUSED(ps);
	ps->modules = crealloc(ps->modules, ps->num_modules + 1);
	module_t *module = &ps->modules[ps->num_modules++];
	*module = module_def;
	module->loader = loader;
	if (loader(ps, module, ud) == 0) {
		log_info("Module '%s' successfully loaded!", module->name);
		return 0;
	}
	return -1;
}
windata_cookie_t module_reserve_windowdata(session_t *ps, module_t *module, size_t reserve)
{
	windata_cookie_t cookie;
	if (module->windata_cookie != -1) {
		return -1;
	}
	if (reserve > INT_MAX) {
		return -1;
	}
	cookie = unsigned_to_int_checked(ps->reserved_windata);
	ps->reserved_windata += reserve;
	module->windata_cookie = cookie;
	module->windata_reserved = reserve;
	return cookie;
}
