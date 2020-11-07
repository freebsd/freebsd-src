/*	$NetBSD: dir.h,v 1.32 2020/10/25 10:00:20 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)dir.h	8.1 (Berkeley) 6/6/93
 */

/*
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)dir.h	8.1 (Berkeley) 6/6/93
 */

#ifndef	MAKE_DIR_H
#define	MAKE_DIR_H

/* A cache for the filenames in a directory. */
typedef struct CachedDir {
    char *name;			/* Name of directory, either absolute or
				 * relative to the current directory.
				 * The name is not normalized in any way,
				 * that is, "." and "./." are different.
				 *
				 * Not sure what happens when .CURDIR is
				 * assigned a new value; see Parse_DoVar. */
    int refCount;		/* Number of SearchPaths with this directory */
    int hits;			/* The number of times a file in this
				 * directory has been found */
    HashTable files;		/* Hash set of files in directory;
				 * all values are NULL. */
} CachedDir;

void Dir_Init(void);
void Dir_InitDir(const char *);
void Dir_InitCur(const char *);
void Dir_InitDot(void);
void Dir_End(void);
void Dir_SetPATH(void);
Boolean Dir_HasWildcards(const char *);
void Dir_Expand(const char *, SearchPath *, StringList *);
char *Dir_FindFile(const char *, SearchPath *);
char *Dir_FindHereOrAbove(const char *, const char *);
time_t Dir_MTime(GNode *, Boolean);
CachedDir *Dir_AddDir(SearchPath *, const char *);
char *Dir_MakeFlags(const char *, SearchPath *);
void Dir_ClearPath(SearchPath *);
void Dir_Concat(SearchPath *, SearchPath *);
void Dir_PrintDirectories(void);
void Dir_PrintPath(SearchPath *);
void Dir_Destroy(void *);
SearchPath *Dir_CopyDirSearchPath(void);

/* Stripped-down variant of struct stat. */
struct make_stat {
    time_t mst_mtime;
    mode_t mst_mode;
};

int cached_lstat(const char *, struct make_stat *);
int cached_stat(const char *, struct make_stat *);

#endif /* MAKE_DIR_H */
