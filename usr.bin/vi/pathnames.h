/* $FreeBSD$ */

/* Read standard system paths first. */
#include <paths.h>

#ifndef	_PATH_EXRC
#define	_PATH_EXRC	".exrc"
#endif

#ifndef	_PATH_MSGCAT
#define	_PATH_MSGCAT	"/usr/share/vi/catalog/"
#endif

#ifndef	_PATH_NEXRC
#define	_PATH_NEXRC	".nexrc"
#endif

/* On linux _PATH_PRESERVE is only writable by root */
#define	NVI_PATH_PRESERVE	"/var/tmp/vi.recover/"

#ifndef	_PATH_SYSEXRC
#define	_PATH_SYSEXRC	"/etc/vi.exrc"
#endif

#ifndef	_PATH_TAGS
#define	_PATH_TAGS	"tags"
#endif
