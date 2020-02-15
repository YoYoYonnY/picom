#define MODULE_NAME shadow
#include "module.h"

int loadmod_shadow(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);
	module->name = "shadow";
	return 0;
}
#undef MODULE_NAME
