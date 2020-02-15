#define MODULE_NAME blur
#include "module.h"

int loadmod_blur(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(ud);
	module->name = "blur";
	return 0;
}
#undef MODULE_NAME
