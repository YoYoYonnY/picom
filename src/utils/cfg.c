#include "cfg.h"
#include "utils/utils.h"

#include <string.h>

#define DEFINE_BASICTYPE(LNAME, UNAME, VALUE, TYPE, DEFAULT, SIZE) cfg_type_t cfg_type_##LNAME = { CFG_T##UNAME, NULL, NULL, NULL };
DEFINE_BASICTYPES(DEFINE_BASICTYPE)
#undef DEFINE_BASICTYPE

void cfg_init(cfg_t *self)
{
	self->items = NULL;
}
void cfg_fini(cfg_t *self)
{
	free(self->items);
}

size_t cfg_typesize(enum cfg_basic_type type)
{
	size_t result = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#define DEFINE_BASICTYPE(LNAME, UNAME, VALUE, TYPE, DEFAULT, SIZE) if ((type & CFG_T##UNAME) && result < SIZE) result = SIZE;
	DEFINE_BASICTYPES(DEFINE_BASICTYPE)
#undef DEFINE_BASICTYPE
#pragma GCC diagnostic pop
	return result;
}

cfg_prop_t cfg_addprop(cfg_t *self, const char *key, const cfg_type_t *type, int offset)
{
	int idx = 0;
	for (struct cfg_item *item = self->items; item && item->name; item++, idx++) {
		if (strcmp(item->name, key) == 0) {
			return idx;
		}
	}
	if (offset == INT_MIN) return -1;
	self->items = crealloc(self->items, idx + 2);
	self->items[idx].name = key;
	self->items[idx].type = type;
	self->items[idx].offset = offset;
	self->items[idx+1].name = NULL;
	return idx;
}
cfg_item_t *cfg_getpropitem(const cfg_t *self, cfg_prop_t prop)
{
	return &self->items[prop];
}

const void *cfg_get(const cfg_t *self, const void *obj, cfg_prop_t prop, enum cfg_basic_type repr)
{
	cfg_item_t *item = cfg_getpropitem(self, prop);
	if (!item) return NULL;
	if (item->type->get) return item->type->get(self, item, obj, repr);
	if (repr != item->type->repr) return NULL;
	return (uint8_t *)obj + item->offset;
}
bool cfg_set(const cfg_t *self, void *obj, cfg_prop_t prop, enum cfg_basic_type repr, void *value)
{
	cfg_item_t *item = cfg_getpropitem(self, prop);
	if (!item) return NULL;
	if (item->type->set) return item->type->set(self, item, obj, repr, value);
	if (repr != item->type->repr) return false;
	memcpy((uint8_t *)obj + item->offset, value, cfg_typesize(repr));
	return true;
}
