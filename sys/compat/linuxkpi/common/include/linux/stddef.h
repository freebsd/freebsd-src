/* Public domain */

#ifndef _LINUXKPI_LINUX_STDDEF_H_
#define	_LINUXKPI_LINUX_STDDEF_H_

#include <sys/stddef.h>

/*
 * FreeBSD has multiple (vendor) drivers containing copies of this
 * and including LinuxKPI headers.  Put the #defines behind guards.
 */

#ifndef __struct_group
#define	__struct_group(_tag, _name, _attrs, _members...)		\
    union {								\
	struct { _members } _attrs;					\
	struct _tag { _members } _attrs _name;				\
    } _attrs
#endif

#ifndef	struct_group
#define	struct_group(_name, _members...)				\
    __struct_group(/* no tag */, _name, /* no attrs */, _members)
#endif

#ifndef	struct_group_tagged
#define	struct_group_tagged(_tag, _name, _members...)			\
    __struct_group(_tag, _name, /* no attrs */, _members)
#endif

#endif	/* _LINUXKPI_LINUX_STDDEF_H_ */
