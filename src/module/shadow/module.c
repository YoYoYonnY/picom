#include "module.h"

static int load(session_t *ps, module_t *module, void *ud) {
	UNUSED(ps);
	UNUSED(module);
	UNUSED(ud);
	return 0;
}
modinfo_t modinfo_shadow = {
	.name = "shadow",
	.load = load,
	.unload = NULL,
};
