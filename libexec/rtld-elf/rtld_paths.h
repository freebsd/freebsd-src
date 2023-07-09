/*-
 * Copyright 1996, 1997, 1998, 1999, 2000 John D. Polstra.
 * Copyright 2003 Alexander Kabaev <kan@FreeBSD.ORG>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _RTLD_PATHS_H
#define _RTLD_PATHS_H

#undef _PATH_ELF_HINTS

#ifndef _RTLD_COMPAT_LIB_SUFFIX
#ifdef COMPAT_libcompat
#define	_RTLD_COMPAT_LIB_SUFFIX	COMPAT_libcompat
#else
#define	_RTLD_COMPAT_LIB_SUFFIX	""
#endif
#endif

#ifndef _RTLD_COMPAT_ENV_SUFFIX
#ifdef COMPAT_LIBCOMPAT
#define	_RTLD_COMPAT_ENV_SUFFIX	COMPAT_LIBCOMPAT "_"
#else
#define	_RTLD_COMPAT_ENV_SUFFIX	""
#endif
#endif

#ifndef __PATH_ELF_HINTS
#define	__PATH_ELF_HINTS(_lc)	"/var/run/ld-elf" _lc ".so.hints"
#endif

#ifndef _PATH_ELF_HINTS
#define	_PATH_ELF_HINTS		__PATH_ELF_HINTS(_RTLD_COMPAT_LIB_SUFFIX)
#endif

#ifndef _PATH_LIBMAP_CONF
#define	_PATH_LIBMAP_CONF	"/etc/libmap" _RTLD_COMPAT_LIB_SUFFIX ".conf"
#endif

#ifndef __BASENAME_RTLD
#define	__BASENAME_RTLD(_lc)	"ld-elf" _lc ".so.1"
#endif

#ifndef _BASENAME_RTLD
#define	_BASENAME_RTLD		__BASENAME_RTLD(_RTLD_COMPAT_LIB_SUFFIX)
#endif

#ifndef __PATH_RTLD
#define	__PATH_RTLD(_lc)	"/libexec/" __BASENAME_RTLD(_lc)
#endif

#ifndef _PATH_RTLD
#define	_PATH_RTLD		__PATH_RTLD(_RTLD_COMPAT_LIB_SUFFIX)
#endif

#ifndef STANDARD_LIBRARY_PATH
#define	STANDARD_LIBRARY_PATH	"/lib" _RTLD_COMPAT_LIB_SUFFIX ":/usr/lib" _RTLD_COMPAT_LIB_SUFFIX
#endif

#ifndef LD_
#define	LD_			"LD_" _RTLD_COMPAT_ENV_SUFFIX
#endif

#ifndef TOKEN_LIB
#define	TOKEN_LIB		"lib" _RTLD_COMPAT_LIB_SUFFIX
#endif

#ifdef IN_RTLD
extern const char *ld_elf_hints_default;
extern const char *ld_path_libmap_conf;
extern const char *ld_path_rtld;
extern const char *ld_standard_library_path;
extern const char *ld_env_prefix;
#endif

#endif /* _RTLD_PATHS_H */
