/*-
 * Copyright (c) 2012 Gleb Kurtsou <gleb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_GEN_COMPAT_H_
#define	_GEN_COMPAT_H_

#include <dirent.h>

#define FREEBSD11_DIRSIZ(dp)						\
	(sizeof(struct freebsd11_dirent) - sizeof((dp)->d_name) +	\
	    (((dp)->d_namlen + 1 + 3) &~ 3))

struct freebsd11_dirent;
struct freebsd11_stat;
struct freebsd11_statfs;

struct freebsd11_dirent *freebsd11_readdir(DIR *);
int	freebsd11_readdir_r(DIR *, struct freebsd11_dirent *,
	    struct freebsd11_dirent **);

int	freebsd11_getmntinfo(struct freebsd11_statfs **, int);

char	*freebsd11_devname(__uint32_t dev, __mode_t type);
char	*freebsd11_devname_r(__uint32_t dev, __mode_t type, char *buf,
	    int len);

/*
 * We want freebsd11_fstat in C source to result in resolution to
 * - fstat@FBSD_1.0 for libc.so (but we do not need the _definition_
 *   of this fstat, it is provided by libsys.so which we want to use).
 * - freebsd11_fstat for libc.a (since if we make it fstat@FBSD_1.0
 *   for libc.a, then final linkage into static object ignores version
 *   and would reference fstat, which is the current syscall, not the
 *   compat syscall). libc.a provides the freebsd11_fstat implementation.
 *   Note that freebsd11_fstat from libc.a is not used for anything, but
 *   we make it correct nonetheless, just in case it would.
 * This is arranged by COMPAT_SYSCALL, and libc can just use freebsd11_fstat.
 */
#ifdef PIC
#define	COMPAT_SYSCALL(rtype, fun, args, sym, ver)			\
    rtype fun args; __sym_compat(sym, fun, ver);
#else
#define	COMPAT_SYSCALL(rtype, fun, args, sym, ver)			\
    rtype fun args;
#endif

COMPAT_SYSCALL(int, freebsd11_stat, (const char *, struct freebsd11_stat *),
    stat, FBSD_1.0);
COMPAT_SYSCALL(int, freebsd11_lstat, (const char *, struct freebsd11_stat *),
    lstat, FBSD_1.0);
COMPAT_SYSCALL(int, freebsd11_fstat, (int, struct freebsd11_stat *),
    fstat, FBSD_1.0);
COMPAT_SYSCALL(int, freebsd11_fstatat, (int, const char *,
    struct freebsd11_stat *, int), fstatat, FBSD_1.1);

COMPAT_SYSCALL(int, freebsd11_statfs, (const char *,
    struct freebsd11_statfs *), statfs, FBSD_1.0);
COMPAT_SYSCALL(int, freebsd11_getfsstat, (struct freebsd11_statfs *, long,
    int), getfsstat, FBSD_1.0);

COMPAT_SYSCALL(int, freebsd14_setgroups, (int gidsize, const __gid_t *gidset),
    setgroups, FBSD_1.0);

#undef COMPAT_SYSCALL

#endif /* _GEN_COMPAT_H_ */
