#define MODULE_NAME transparency
#include "module.h"

int loadmod_transparency(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);
	module->name = "transparency";
	return 0;
}
#undef MODULE_NAME
