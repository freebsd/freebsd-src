/*
 * Copyright (c) 2003 by Joel Baker.
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
 * 3. Neither the name of the Author nor the names of any contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * $FreeBSD$
 */

#ifndef _FTW_H
#define _FTW_H

#include <sys/stat.h>

__BEGIN_DECLS

/* Enumerated values for 'flag' when calling [n]ftw */

enum {
    FTW_D,   /* Directories */
    FTW_DNR, /* Unreadable directory */
    FTW_F,   /* Regular files */
    FTW_SL,  /* Symbolic link */
    FTW_NS,  /* stat(2) failed */

#if __XSI_VISIBLE /* X/Open */

/* Flags for nftw only */

    FTW_DP, /* Directory, subdirs visited */
    FTW_SLN, /* Dangling symlink */

#endif /* __XSI_VISIBLE */
};

#if __XSI_VISIBLE /* X/Open */

/* Enumerated values for 'flags' when calling nftw */

enum {
    FTW_CHDIR = 1, /* Do a chdir(2) when entering a directory */
    FTW_DEPTH = 2, /* Report files first (before directory) */
    FTW_MOUNT = 4, /* Single filesystem */
    FTW_PHYS  = 8  /* Physical walk; ignore symlinks */
};

#define FTW_PHYS FTW_PHYS
#define FTW_MOUNT FTW_MOUNT
#define FTW_CHDIR FTW_CHDIR
#define FTW_DEPTH FTW_DEPTH

/* FTW struct for callbacks from nftw */

struct FTW {
    int base;
    int level;
};

#endif /* __XSI_VISIBLE */

/* Typecasts for callback functions */

typedef int (*__ftw_func_t) \
    (const char *file, const struct stat *status, int flag);

/* ftw: walk a directory tree, calling a function for each element */

extern int ftw (const char *dir, __ftw_func_t func, int descr);

#if __XSI_VISIBLE /* X/Open */

typedef int (*__nftw_func_t) \
    (const char *file, const struct stat *status, int flag, struct FTW *detail);

/* nftw: walk a directory tree, calling a function for each element; much
 * like ftw, but with behavior flags and minty freshness.
 */

extern int nftw (const char *dir, __nftw_func_t func, int descr, int flags);

#endif /* __XSI_VISIBLE */

__END_DECLS

#endif /* _FTW_H */
