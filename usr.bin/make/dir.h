/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
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
 *	$Id$
 */

/* dir.h --
 */

#ifndef	_DIR
#define	_DIR

typedef struct Path {
    char         *name;	    	/* Name of directory */
    int	    	  refCount; 	/* Number of paths with this directory */
    int		  hits;	    	/* the number of times a file in this
				 * directory has been found */
    Hash_Table    files;    	/* Hash table of files in directory */
} Path;

void Dir_Init __P((void));
void Dir_End __P((void));
Boolean Dir_HasWildcards __P((char *));
void Dir_Expand __P((char *, Lst, Lst));
char *Dir_FindFile __P((char *, Lst));
int Dir_MTime __P((GNode *));
void Dir_AddDir __P((Lst, char *));
char *Dir_MakeFlags __P((char *, Lst));
void Dir_ClearPath __P((Lst));
void Dir_Concat __P((Lst, Lst));
void Dir_PrintDirectories __P((void));
void Dir_PrintPath __P((Lst));
void Dir_Destroy __P((ClientData));
ClientData Dir_CopyDir __P((ClientData));

#endif /* _DIR */
