#include "common.h"

#include "utils/compiler.h"
#include "utils/utils.h"

#include "module.h"

module_t *module_load(session_t *ps, modinfo_t *modinfo, void *ud)
{
	UNUSED(ps);
	module_t *module = cmalloc(module_t);
	memset(module, 0x00, sizeof(module_t));
	module->info = *modinfo;
	module->windata_cookie = -1;
	log_info("Loading module '%s'...", module->info.name);
	if (module->info.load && module->info.load(ps, module, ud) != 0)
		goto fail;
	log_info("Module '%s' successfully loaded!", module->info.name);
	ps->modules = crealloc(ps->modules, ps->num_modules + 1);
	ps->modules[ps->num_modules++] = module;
	return module;
fail:
	free(module);
	return NULL;
}
void module_unload(session_t *ps, module_t *module, void *ud)
{
	UNUSED(ps);
	UNUSED(module);
	UNUSED(ud);
	// TODO
}

void module_subscribe(module_t *module, modev_t evid, modev_cb_t cb)
{
	module->modev_cb[evid] = cb;
}
void module_unsubscribe(module_t *module, modev_t evid)
{
	module->modev_cb[evid] = NULL;
}

int module_emit(modev_t evid, session_t *ps, void *ud)
{
	int nemitted = 0;
	for (size_t i=0; i < ps->num_modules; i++) {
		module_t *module = ps->modules[i];
		modev_cb_t cb = module->modev_cb[evid];
		if (cb) {
			cb(evid, module, ps, ud);
			nemitted++;
		}
	}
	return nemitted;
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
