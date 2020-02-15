#include "common.h"
#include "compiler.h"
#include "utils.h"
#include "module.h"

void module_emit(modev_t evid, session_t *ps, void *ud)
{
	for (size_t i=0; i < ps->num_modules; i++) {
		module_t *pm = &ps->modules[i];
		modev_cb_t cb = pm->modev_cb[evid];
		if (cb) {
			cb(evid, ps, ud);
		}
	}
}
void module_load(session_t *ps, modloader_cb_t loader, void *ud)
{
	static const module_t module_def = {
		.name = NULL,
		.loader = NULL,
		.modev_cb = {0},
	};
	UNUSED(ps);
	ps->modules = crealloc(ps->modules, ps->num_modules + 1);
	module_t *module = &ps->modules[ps->num_modules++];
	*module = module_def;
	module->loader = loader;
	if (loader(ps, module, ud) == 0) {
		log_info("Module '%s' successfully loaded!", module->name);
	}
}
