/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI cwd.h,v 2.2 1996/04/08 19:32:26 bostic Exp
 *
 * $FreeBSD: src/usr.bin/doscmd/cwd.h,v 1.2 1999/08/28 01:00:08 peter Exp $
 */

static inline u_char *
ustrcpy(u_char *s1, u_char *s2)
{
    return((u_char *)strcpy((char *)s1, (char *)s2));
}

static inline u_char *
ustrcat(u_char *s1, u_char *s2)
{
    return((u_char *)strcat((char *)s1, (char *)s2));
}

static inline u_char *
ustrncpy(u_char *s1, u_char *s2, unsigned n)
{
    return((u_char *)strncpy((char *)s1, (char *)s2, n));
}

static inline int
ustrcmp(u_char *s1, u_char *s2)
{
    return(strcmp((char *)s1, (char *)s2));
}

static inline int
ustrncmp(u_char *s1, u_char *s2, unsigned n)
{
    return(strncmp((char *)s1, (char *)s2, n));
}

static inline int
ustrlen(u_char *s)
{
    return(strlen((char *)s));
}

static inline u_char *
ustrrchr(u_char *s, u_char c)
{
    return((u_char *)strrchr((char *)s, c));
}

static inline u_char *
ustrdup(u_char *s)
{
    return((u_char *)strdup((char *)s));
}

static inline int
ustat(u_char *s, struct stat *sb)
{
    return(stat((char *)s, sb));
}

static inline int
uaccess(u_char *s, int mode)
{
    return(access((char *)s, mode));
}

extern void   init_path(int drive, u_char *base, u_char *where);
extern void   dos_makereadonly(int drive);
extern int    dos_readonly(int drive);
extern u_char *dos_getcwd(int drive);
extern u_char *dos_getpath(int drive);
extern int    dos_makepath(u_char *where, u_char *newpath);
extern int    dos_setcwd(u_char *where);
extern int    dos_to_real_path(u_char *dospath, u_char *realpath, int *);
extern void   real_to_dos(u_char *real, u_char *dos);
extern void   dos_to_real(u_char *dos, u_char *real);
extern u_char **get_entries(u_char *path);
extern int    get_space(int drive, fsstat_t *fs);
extern int    find_first(u_char *path, int attr,
			 dosdir_t *dir, find_block_t *dta);
extern int    find_next(dosdir_t *dir, find_block_t *dta);
