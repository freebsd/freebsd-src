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
 * $FreeBSD$
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
ustrncat(u_char *s1, const u_char *s2, size_t n)
{
    return((u_char *)strncat((char *)s1, (const char *)s2, n));
}

static inline u_char *
ustrncpy(u_char *s1, u_char *s2, size_t n)
{
    return((u_char *)strncpy((char *)s1, (char *)s2, n));
}

static inline int
ustrcmp(u_char *s1, u_char *s2)
{
    return(strcmp((char *)s1, (char *)s2));
}

static inline int
ustrncmp(const u_char *s1, const u_char *s2, size_t n)
{
    return(strncmp((const char *)s1, (const char *)s2, n));
}

static inline int
ustrlen(const u_char *s)
{
    return(strlen((const char *)s));
}

static inline u_char *
ustrrchr(u_char *s, u_char c)
{
    return((u_char *)strrchr((char *)s, c));
}

static inline u_char *
ustrdup(const u_char *s)
{
    return((u_char *)strdup((const char *)s));
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

extern void	init_path(int, const u_char *, const u_char *);
extern void	dos_makereadonly(int);
extern int	dos_readonly(int);
extern u_char	*dos_getcwd(int);
extern u_char	*dos_getpath(int);
extern int	dos_makepath(u_char *, u_char *);
extern int	dos_match(u_char *, u_char *);
extern int	dos_setcwd(u_char *);
extern int	dos_to_real_path(u_char *, u_char *, int *);
extern void	real_to_dos(u_char *, u_char *);
extern void	dos_to_real(u_char *, u_char *);
extern u_char	**get_entries(u_char *);
extern int	get_space(int, fsstat_t *);
extern int	find_first(u_char *, int, dosdir_t *, find_block_t *);
extern int	find_next(dosdir_t *, find_block_t *);
