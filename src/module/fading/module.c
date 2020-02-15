#define MODULE_NAME fading
#include "module.h"

int loadmod_fading(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);
	module->name = "fading";
	return 0;
}
#undef MODULE_NAME
