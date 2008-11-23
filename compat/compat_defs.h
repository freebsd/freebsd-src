/*	$NetBSD: compat_defs.h,v 1.43 2004/06/23 11:08:01 tron Exp $	*/

#ifndef	__NETBSD_COMPAT_DEFS_H__
#define	__NETBSD_COMPAT_DEFS_H__

/* Work around some complete brain damage. */

#undef _POSIX_SOURCE
#undef _POSIX_C_SOURCE

/* System headers needed for (re)definitions below. */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
/* time.h needs to be pulled in first at least on netbsd w/o _NETBSD_SOURCE */
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#if HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#if HAVE_SYS_SYSMACROS_H
/* major(), minor() on SVR4 */
#include <sys/sysmacros.h>
#endif
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDDEF_H
#include <stddef.h>
#endif

/* We don't include <pwd.h> here, so that "compat_pwd.h" works. */
struct passwd;

/* Some things usually in BSD <sys/cdefs.h>. */
#ifndef __RENAME
#define __RENAME(x)
#endif

/* Dirent support. */
#include <dirent.h>
#define NAMLEN(dirent) (strlen((dirent)->d_name))

#if !HAVE_FPARSELN || defined(__NetBSD__)
# define FPARSELN_UNESCESC	0x01
# define FPARSELN_UNESCCONT	0x02
# define FPARSELN_UNESCCOMM	0x04
# define FPARSELN_UNESCREST	0x08
# define FPARSELN_UNESCALL	0x0f
char *fparseln(FILE *, size_t *, size_t *, const char [3], int);
#endif

#define __nbcompat_bswap16(x)	((((x) << 8) & 0xff00) | (((x) >> 8) & 0x00ff))

#define __nbcompat_bswap32(x)	((((x) << 24) & 0xff000000) | \
				 (((x) <<  8) & 0x00ff0000) | \
				 (((x) >>  8) & 0x0000ff00) | \
				 (((x) >> 24) & 0x000000ff))

#define __nbcompat_bswap64(x)	(((u_int64_t)bswap32((x)) << 32) | \
				 ((u_int64_t)bswap32((x) >> 32)))

#if !HAVE_BSWAP16
#ifdef bswap16
#undef bswap16
#endif
#define bswap16(x)	__nbcompat_bswap16(x)
#endif
#if !HAVE_BSWAP32
#ifdef bswap32
#undef bswap32
#endif
#define bswap32(x)	__nbcompat_bswap32(x)
#endif
#if !HAVE_BSWAP64
#ifdef bswap64
#undef bswap64
#endif
#define bswap64(x)	__nbcompat_bswap64(x)
#endif

#if !HAVE_PWCACHE_USERDB
int uid_from_user(const char *, uid_t *);
int pwcache_userdb(int (*)(int), void (*)(void),
		struct passwd * (*)(const char *), struct passwd * (*)(uid_t));
int gid_from_group(const char *, gid_t *);
int pwcache_groupdb(int (*)(int), void (*)(void),
		struct group * (*)(const char *), struct group * (*)(gid_t));
#endif
/* Make them use our version */
#  define user_from_uid __nbcompat_user_from_uid
/* Make them use our version */
#  define group_from_gid __nbcompat_group_from_gid
#if HAVE_GROUP_FROM_GID
const char *group_from_gid(gid_t, int);
#endif

#if !HAVE_SETENV
int setenv(const char *, const char *, int);
#endif

#if !HAVE_STRSUFTOLL
long long strsuftoll(const char *, const char *, long long, long long);
long long strsuftollx(const char *, const char *,
			long long, long long, char *, size_t);
#endif

#if !HAVE_USER_FROM_UID
const char *user_from_uid(uid_t, int);
#endif

#if !HAVE_GROUP_FROM_GID
const char *group_from_gid(gid_t, int);
#endif

/*
 * getmode() and setmode() are always defined, as these function names
 * exist but with very different meanings on other OS's.  The compat
 * versions here simply accept an octal mode number; the "u+x,g-w" type
 * of syntax is not accepted.
 */

#define getmode __nbcompat_getmode
#define setmode __nbcompat_setmode

mode_t getmode(const void *, mode_t);
void *setmode(const char *);

/* Eliminate assertions embedded in binaries. */

#undef _DIAGASSERT
#define _DIAGASSERT(x)

/* Various sources use this */
#undef	__RCSID
#define	__RCSID(x)
#undef	__SCCSID
#define	__SCCSID(x)
#undef	__COPYRIGHT
#define	__COPYRIGHT(x)
#undef	__KERNEL_RCSID
#define	__KERNEL_RCSID(x,y)

/* Heimdal expects this one. */

#undef RCSID
#define RCSID(x)

#ifndef MAXFRAG
#define MAXFRAG 8
#endif

#endif	/* !__NETBSD_COMPAT_DEFS_H__ */
