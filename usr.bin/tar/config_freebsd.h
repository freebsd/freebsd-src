/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* A default configuration for FreeBSD, used if there is no config.h. */

#include <sys/param.h>  /* __FreeBSD_version */

#if __FreeBSD__ > 4
#define	HAVE_ACL_GET_PERM 0
#define	HAVE_ACL_GET_PERM_NP 1
#define	HAVE_ACL_PERMSET_T 1
#define	HAVE_ACL_USER 1
#endif
#undef	HAVE_ATTR_XATTR_H
#define	HAVE_BZLIB_H 1
#define	HAVE_CHFLAGS 1
#define	HAVE_CHROOT 1
#define	HAVE_DECL_OPTARG 1
#define	HAVE_DECL_OPTIND 1
#define	HAVE_DIRENT_D_NAMLEN 1
#define	HAVE_DIRENT_H 1
#define	HAVE_D_MD_ORDER 1
#define	HAVE_ERRNO_H 1
#undef	HAVE_EXT2FS_EXT2_FS_H
#define	HAVE_FCHDIR 1
#define	HAVE_FCNTL_H 1
#define	HAVE_FNMATCH 1
#define	HAVE_FNMATCH_H 1
#define	HAVE_FNM_LEADING_DIR 1
#define	HAVE_FTRUNCATE 1
#undef	HAVE_GETXATTR
#define	HAVE_GRP_H 1
#define	HAVE_INTTYPES_H 1
#define	HAVE_LANGINFO_H 1
#undef	HAVE_LGETXATTR
#undef	HAVE_LIBACL
#define	HAVE_LIBARCHIVE 1
#define	HAVE_LIBBZ2 1
#define	HAVE_LIBZ 1
#define	HAVE_LIMITS_H 1
#undef	HAVE_LINUX_EXT2_FS_H
#undef	HAVE_LINUX_FS_H
#undef	HAVE_LISTXATTR
#undef	HAVE_LLISTXATTR
#define	HAVE_LOCALE_H 1
#define	HAVE_MALLOC 1
#define	HAVE_MEMMOVE 1
#define	HAVE_MEMORY_H 1
#define	HAVE_MEMSET 1
#if __FreeBSD_version >= 450002 /* nl_langinfo introduced */
#define	HAVE_NL_LANGINFO 1
#endif
#define	HAVE_PATHS_H 1
#define	HAVE_PWD_H 1
#define	HAVE_REGEX_H 1
#define	HAVE_SETLOCALE 1
#define	HAVE_STDARG_H 1
#define	HAVE_STDINT_H 1
#define	HAVE_STDLIB_H 1
#define	HAVE_STRCHR 1
#define	HAVE_STRDUP 1
#define	HAVE_STRERROR 1
#define	HAVE_STRFTIME 1
#define	HAVE_STRINGS_H 1
#define	HAVE_STRING_H 1
#define	HAVE_STRRCHR 1
#define	HAVE_STRUCT_STAT_ST_FLAGS 1
#undef	HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
#define	HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC 1
#define	HAVE_SYS_ACL_H 1
#define	HAVE_SYS_IOCTL_H 1
#define	HAVE_SYS_PARAM_H 1
#define	HAVE_SYS_STAT_H 1
#define	HAVE_TIME_H 1
#define	HAVE_SYS_TYPES_H 1
#define	HAVE_UINTMAX_T 1
#define	HAVE_UNISTD_H 1
#define	HAVE_UNSIGNED_LONG_LONG
#define	HAVE_VPRINTF 1
#define	HAVE_WCTYPE_H 1
#define	HAVE_ZLIB_H 1
#undef	MAJOR_IN_MKDEV
#define	STDC_HEADERS 1

