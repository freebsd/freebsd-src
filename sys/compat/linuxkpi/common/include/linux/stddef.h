/* Public domain */

#ifndef _LINUXKPI_LINUX_STDDEF_H_
#define	_LINUXKPI_LINUX_STDDEF_H_

#include <sys/stddef.h>

#define	struct_group(NAME, ...)						\
    union {								\
	struct { __VA_ARGS__ };						\
	struct { __VA_ARGS__ } NAME;					\
    }

#endif	/* _LINUXKPI_LINUX_STDDEF_H_ */

