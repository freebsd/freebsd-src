/* nbtool_config.h.  Generated automatically by configure.  */
/*	$NetBSD: nbtool_config.h.in,v 1.4 2004/06/20 22:20:15 jmc Exp $	*/

#ifndef	__NETBSD_NBTOOL_CONFIG_H__
#define	__NETBSD_NBTOOL_CONFIG_H__

/* Values set by "configure" based on available functions in the host. */

#define PATH_BSHELL "/bin/sh"

/* #undef HAVE_ALLOCA_H */
#define HAVE_DIRENT_H 1
#define HAVE_ERR_H 1
/* #undef HAVE_FEATURES_H */
#define HAVE_GETOPT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIBGEN_H 1
/* #undef HAVE_NDIR_H */
#define HAVE_NETDB_H 1
/* #undef HAVE_MACHINE_BSWAP_H */
/* #undef HAVE_MALLOC_H */
#define HAVE_SYS_POLL_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STRING_H 1
/* #undef HAVE_SYS_DIR_H */
#define HAVE_SYS_ENDIAN_H 1
/* #undef HAVE_SYS_NDIR_H */
/* #undef HAVE_SYS_SYSLIMITS_H */
/* #undef HAVE_SYS_SYSMACROS_H */
#define HAVE_SYS_TYPES_H 1
#define HAVE_TERMIOS_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1

#define HAVE_ID_T 1
#define HAVE_SOCKLEN_T 1
#define HAVE_LONG_LONG 1
#define HAVE_U_LONG 1
#define HAVE_U_CHAR 1
#define HAVE_U_INT 1
#define HAVE_U_SHORT 1
#define HAVE_U_QUAD_T 1

/* #undef HAVE_BSWAP16 */
/* #undef HAVE_BSWAP32 */
/* #undef HAVE_BSWAP64 */
/* #undef HAVE_HTOBE16 */
/* #undef HAVE_HTOBE32 */
/* #undef HAVE_HTOBE64 */
/* #undef HAVE_HTOLE16 */
/* #undef HAVE_HTOLE32 */
/* #undef HAVE_HTOLE64 */
/* #undef HAVE_BE16TOH */
/* #undef HAVE_BE32TOH */
/* #undef HAVE_BE64TOH */
/* #undef HAVE_LE16TOH */
/* #undef HAVE_LE32TOH */
/* #undef HAVE_LE64TOH */

#define HAVE_DIR_DD_FD 1
#define HAVE_STRUCT_DIRENT_D_NAMLEN 1
#define HAVE_STRUCT_STAT_ST_FLAGS 1
#define HAVE_STRUCT_STAT_ST_GEN 1
#define HAVE_STRUCT_STAT_ST_BIRTHTIME 1
/* #undef HAVE_STRUCT_STAT_ST_ATIM */
/* #undef HAVE_STRUCT_STAT_ST_MTIMENSEC */
#define HAVE_STRUCT_STATFS_F_IOSIZE 1

#define HAVE_DECL_OPTIND 1
#define HAVE_DECL_OPTRESET 1
#define HAVE_DECL_SYS_SIGNAME 1

#define HAVE_ASPRINTF 1
/* #undef HAVE_ASNPRINTF */
#define HAVE_BASENAME 1
/* #undef HAVE_CGETNEXT */
#define HAVE_DEVNAME 1
/* #undef HAVE_DIRFD */
#define HAVE_DIRNAME 1
#define HAVE_FGETLN 1
#define HAVE_FLOCK 1
/* #undef HAVE_FPARSELN */
#define HAVE_FUTIMES 1
#define HAVE_GETOPT 1
#define HAVE_GETOPT_LONG 1
#define HAVE_GROUP_FROM_GID 1
#define HAVE_ISBLANK 1
#define HAVE_ISSETUGID 1
#define HAVE_LCHFLAGS 1
#define HAVE_LCHMOD 1
#define HAVE_LCHOWN 1
#define HAVE_LUTIMES 1
#define HAVE_MKSTEMP 1
#define HAVE_MKDTEMP 1
#define HAVE_POLL 1
#define HAVE_PREAD 1
#define HAVE_PUTC_UNLOCKED 1
/* #undef HAVE_PWCACHE_USERDB */
#define HAVE_PWRITE 1
#define HAVE_RANDOM 1
#define HAVE_SETENV 1
#define HAVE_SETGROUPENT 1
#define HAVE_SETPASSENT 1
#define HAVE_SETPROGNAME 1
#define HAVE_SNPRINTF 1
#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1
#define HAVE_STRSEP 1
/* #undef HAVE_STRSUFTOULL */
#define HAVE_STRTOLL 1
#define HAVE_USER_FROM_UID 1
#define HAVE_VASPRINTF 1
/* #undef HAVE_VASNPRINTF */
#define HAVE_VSNPRINTF 1

#define HAVE_DECL_SETGROUPENT 1
#define HAVE_DECL_SETPASSENT 1

/* #undef WORDS_BIGENDIAN */

/* Typedefs that might be missing. */

/* #undef size_t */
/* #undef uint8_t */
/* #undef uint16_t */
/* #undef uint32_t */
/* #undef uint64_t */
/* #undef u_int8_t */
/* #undef u_int16_t */
/* #undef u_int32_t */
/* #undef u_int64_t */

/* Now pull in the compatibility definitions. */

#include "compat_defs.h"

#endif	/* !__NETBSD_NBTOOL_CONFIG_H__ */
