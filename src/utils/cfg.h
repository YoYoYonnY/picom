#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "types.h"

/// Defines a configuration layout for modules
/// Note that the cfg_t does not actually store the values, but where the
/// values can be _found_ (i.e. an offset into a struct).
typedef struct cfg cfg_t;
/// Internal representation of the property
typedef struct cfg_item cfg_item_t;
/// Fast index to name/type/offset of a property
typedef int cfg_prop_t;

#define DEFINE_BASICTYPES(DEFINE_BASICTYPE) \
	DEFINE_BASICTYPE(bool,     BOOLEAN,  0x01, bool,    false,    sizeof(bool)) \
	DEFINE_BASICTYPE(byte,     BYTE,     0x02, uint8_t, 0,        sizeof(uint8_t)) \
	DEFINE_BASICTYPE(int,      INTEGER,  0x04, int,     0,        sizeof(int)) \
	DEFINE_BASICTYPE(float,    FLOATING, 0x08, double,  0.0,      sizeof(double)) \
	DEFINE_BASICTYPE(string,   STRING,   0x10, char*,   NULL,     sizeof(char *)) \
	DEFINE_BASICTYPE(pointer,  POINTER,  0x20, void*,   NULL,     sizeof(void *)) \
	DEFINE_BASICTYPE(list,     LIST,     0x40, buf_t,   BUF_NULL, sizeof(buf_t))

enum cfg_basic_type {
	CFG_TNONE,
#define DEFINE_BASICTYPE(LNAME, UNAME, VALUE, TYPE, DEFAULT, SIZE) CFG_T##UNAME = VALUE,
	DEFINE_BASICTYPES(DEFINE_BASICTYPE)
#undef DEFINE_BASICTYPE
};

typedef const void *(*cfg_get_t)(const cfg_t *self, const cfg_item_t *item, const void *obj, enum cfg_basic_type repr);
typedef bool (*cfg_set_t)(const cfg_t *self, const cfg_item_t *item, void *obj, enum cfg_basic_type repr, const void *value);
typedef bool (*cfg_unset_t)(const cfg_t *self, const cfg_item_t *item, void *obj);
typedef struct cfg_type {
	/// The representation the property will be stored as
	enum cfg_basic_type repr;
	/// Returns the value of a property, or NULL if it cannot be found.
	cfg_get_t get;
	/// Changes the value of a property
	cfg_set_t set;
	/// Unset a property
	/// Here, you can free memory and/or set a sentinal value (such as 0 or NULL)
	cfg_unset_t unset;
} cfg_type_t;
/// An item in the property map
struct cfg_item {
	const char         *name;   /// Name of the property
	enum cfg_basic_type repr;   /// Basic type this property is represented as
	const cfg_type_t   *type;   /// Actual type of this property
	int                 offset; /// Offset of this property
};
struct cfg {
	cfg_item_t *items;
	int min_start;
	int max_end;
};

#define DEFINE_BASICTYPE(LNAME, UNAME, VALUE, TYPE, DEFAULT, SIZE) extern cfg_type_t cfg_type_##LNAME;
DEFINE_BASICTYPES(DEFINE_BASICTYPE)
#undef DEFINE_BASICTYPE

/// Returns the max size of a set of basic types
size_t cfg_typesize(enum cfg_basic_type type);

void cfg_init(cfg_t *self);
void cfg_fini(cfg_t *self);

/// Register a property in the configuration object
/// @param offset A byte offset into a buffer where the value will be stored and read from. May be negative.
/// You can use offsetof in stddef.h to find the right offset in a struct.
/// If the property already exists, returns the offset, without changing it.
/// Currently, multiple different keys can be defined with the same offset, but they MUST also have the same type and representation.
/// This is not checked for you!
cfg_prop_t cfg_addprop(cfg_t *self, const char *key, const cfg_type_t *type, int offset);
/// Returns an index to the property if it exists, -1 otherwise.
#define cfg_getprop(self, key) cfg_addprop((cfg_t *)(self), (key), NULL, INT_MIN)
cfg_item_t *cfg_getpropitem(const cfg_t *self, cfg_prop_t prop);
#define cfg_getpropname(self, prop) cfg_getpropitem(self, prop)->name
#define cfg_getproprepr(self, prop) cfg_getpropitem(self, prop)->repr
#define cfg_getproptype(self, prop) cfg_getpropitem(self, prop)->type
#define cfg_getpropoffset(self, prop) cfg_getpropitem(self, prop)->offset

/// Returns pointer to the value referenced by property in the given object
/// @param repr The representation we are expecting to be returned
const void *cfg_get(const cfg_t *self, const void *obj, cfg_prop_t prop, enum cfg_basic_type repr);
/// Change the value of a property
/// Returns true if the value was changed, false otherwise
/// A return of false might indicate that the value is invalid
/// @param repr The representation we are passing value as
bool cfg_set(const cfg_t *self, void *obj, cfg_prop_t prop, enum cfg_basic_type repr, void *value);
/// Unsets a value, if available.
/// Returns true if value was unset, false otherwise
bool cfg_unset(const cfg_t *self, void *obj, cfg_prop_t prop);

#define DEFINE_BASICTYPE(LNAME, UNAME, VALUE, TYPE, DEFAULT, SIZE) \
	static inline bool cfg_set##LNAME(const cfg_t *self, void *obj, cfg_prop_t prop, TYPE value) { \
		return cfg_set(self, obj, prop, CFG_T##UNAME, &value); \
	} \
	static inline const TYPE *cfg_get##LNAME(const cfg_t *self, const void *obj, cfg_prop_t prop) { \
		return (const TYPE *)cfg_get(self, obj, prop, CFG_T##UNAME); \
	} \
	static inline TYPE cfg_get##LNAME##_def(const cfg_t *self, const void *obj, cfg_prop_t prop) { \
		const TYPE *value = (const TYPE *)cfg_get(self, obj, prop, CFG_T##UNAME); \
		if (!value) return DEFAULT; \
		_Pragma("GCC diagnostic push"); \
		_Pragma("GCC diagnostic ignored \"-Wdiscarded-qualifiers\"") \
		return *value; \
		_Pragma("GCC diagnostic pop"); \
	}
DEFINE_BASICTYPES(DEFINE_BASICTYPE)
#undef DEFINE_BASICTYPE
