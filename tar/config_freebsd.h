/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */

/* A default configuration for FreeBSD, used if there is no config.h. */

#include <sys/param.h>  /* __FreeBSD_version */

#undef	HAVE_ATTR_XATTR_H
#define	HAVE_CHROOT 1
#define	HAVE_DIRENT_D_NAMLEN 1
#define	HAVE_DIRENT_H 1
#define	HAVE_D_MD_ORDER 1
#define	HAVE_ERRNO_H 1
#undef	HAVE_EXT2FS_EXT2_FS_H
#define	HAVE_FCHDIR 1
#define	HAVE_FCNTL_H 1
#define	HAVE_GRP_H 1
#define	HAVE_LANGINFO_H 1
#undef	HAVE_LIBACL
#define	HAVE_LIBARCHIVE 1
#define	HAVE_LIMITS_H 1
#define	HAVE_LINK 1
#undef	HAVE_LINUX_EXT2_FS_H
#undef	HAVE_LINUX_FS_H
#define	HAVE_LOCALE_H 1
#define	HAVE_MBTOWC 1
#undef	HAVE_NDIR_H
#if __FreeBSD_version >= 450002 /* nl_langinfo introduced */
#define	HAVE_NL_LANGINFO 1
#endif
#define	HAVE_PATHS_H 1
#define	HAVE_PWD_H 1
#define	HAVE_READLINK 1
#define	HAVE_REGEX_H 1
#define	HAVE_SETLOCALE 1
#define	HAVE_SIGNAL_H 1
#define	HAVE_STDARG_H 1
#define	HAVE_STDLIB_H 1
#define	HAVE_STRING_H 1
#define	HAVE_STRUCT_STAT_ST_FLAGS 1
#undef	HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
#undef	HAVE_STRUCT_STAT_ST_MTIME_N
#undef	HAVE_STRUCT_STAT_ST_MTIME_USEC
#define	HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC 1
#undef	HAVE_STRUCT_STAT_ST_UMTIME
#define	HAVE_SYMLINK 1
#define	HAVE_SYS_CDEFS_H 1
#undef	HAVE_SYS_DIR_H
#define	HAVE_SYS_IOCTL_H 1
#undef	HAVE_SYS_NDIR_H
#define	HAVE_SYS_PARAM_H 1
#define	HAVE_SYS_STAT_H 1
#define	HAVE_TIME_H 1
#define	HAVE_SYS_TYPES_H 1
#define	HAVE_UINTMAX_T 1
#define	HAVE_UNISTD_H 1
#define	HAVE_UNSIGNED_LONG_LONG
#define	HAVE_WCTYPE_H 1
#define	HAVE_ZLIB_H 1
#undef	MAJOR_IN_MKDEV
