#ifndef _LINUX_MODULE_PARAMS_H
#define _LINUX_MODULE_PARAMS_H
/* Macros for (very simple) module parameter compatibility with 2.6. */
#include <linux/module.h>

/* type is byte, short, ushort, int, uint, long, ulong, bool. (2.6
   has more, but they are not supported).  perm is permissions when
   it appears in sysfs: 0 means doens't appear, 0444 means read-only
   by everyone, 0644 means changable dynamically by root, etc.  name
   must be in scope (unlike MODULE_PARM).
*/
#define module_param(name, type, perm)					     \
	static inline void *__check_existence_##name(void) { return &name; } \
	MODULE_PARM(name, _MODULE_PARM_STRING_ ## type)

#define _MODULE_PARM_STRING_byte "b"
#define _MODULE_PARM_STRING_short "h"
#define _MODULE_PARM_STRING_ushort "h"
#define _MODULE_PARM_STRING_int "i"
#define _MODULE_PARM_STRING_uint "i"
#define _MODULE_PARM_STRING_long "l"
#define _MODULE_PARM_STRING_ulong "l"
#define _MODULE_PARM_STRING_bool "i"

#endif /* _LINUX_MODULE_PARAM_TYPES_H */
